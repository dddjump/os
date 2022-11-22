// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  struct spinlock hashlock[NBUCKETS];
  struct buf hashbucket[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i=0;i<NBUCKETS;i++){
    initlock(&bcache.hashlock[i],"bcache.hashlock");
    bcache.hashbucket[i].prev=&bcache.hashbucket[i];
    bcache.hashbucket[i].next=&bcache.hashbucket[i];
  }

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    int hash=(b->blockno)%NBUCKETS;
    // b=&bcache.buf[hash];
    // b->blockno=hash;

    b->next = bcache.hashbucket[hash].next;
    b->prev = &bcache.hashbucket[hash];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[hash].next->prev = b;
    bcache.hashbucket[hash].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int hash=blockno%NBUCKETS;

  acquire(&bcache.hashlock[hash]);

  // Is the block already cached?
  for(b = bcache.hashbucket[hash].next; b != &bcache.hashbucket[hash]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hashlock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.hashbucket[hash].prev; b != &bcache.hashbucket[hash]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.hashlock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // release(&bcache.hashlock[hash]);
  for(int i=0;i<NBUCKETS;i++){
    if(i!=hash){
      acquire(&bcache.hashlock[i]);
      for(b=bcache.hashbucket[i].prev;b!=&bcache.hashbucket[i];b=b->prev){
        if(b->refcnt == 0) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;

          b->next->prev = b->prev;
          b->prev->next = b->next;
          
          b->next=bcache.hashbucket[hash].next;
          b->prev=&bcache.hashbucket[hash];
          bcache.hashbucket[hash].next->prev=b;
          bcache.hashbucket[hash].next=b;
          release(&bcache.hashlock[i]);
          release(&bcache.hashlock[hash]);
          acquiresleep(&b->lock);
          return b;
        }
      } 
      release(&bcache.hashlock[i]);
    } 
  }
  panic("bget: no buffers");
}
// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int hash=(b->blockno)%NBUCKETS;
  acquire(&bcache.hashlock[hash]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[hash].next;
    b->prev = &bcache.hashbucket[hash];
    bcache.hashbucket[hash].next->prev = b;
    bcache.hashbucket[hash].next = b;
  }
  release(&bcache.hashlock[hash]);
}

void
bpin(struct buf *b) {
  int hash=(b->blockno)%NBUCKETS;
  acquire(&bcache.hashlock[hash]);
  b->refcnt++;
  release(&bcache.hashlock[hash]);
}

void
bunpin(struct buf *b) {
  int hash=(b->blockno)%NBUCKETS;
  acquire(&bcache.hashlock[hash]);
  b->refcnt--;
  release(&bcache.hashlock[hash]);
}


