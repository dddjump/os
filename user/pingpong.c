#include "kernel/types.h"
#include "user.h"

int main(int argc,char* argv[]){
    int p_ftos[2];     //父进程写入，子进程读出的管道
    int p_stof[2];     //子进程写入，父进程读出的管道
    pipe(p_ftos);
    pipe(p_stof);
    int pid=fork();
    char buf[8];
    if(pid<0){
        printf("error!");
        exit(1);
    }
    else if(pid==0){           //子进程
        close(p_ftos[1]);
        close(p_stof[0]);
        read(p_ftos[0],buf,4);
        close(p_ftos[0]);
        printf("%d: received ping\n",getpid());
        write(p_stof[1],"pong",4);
        close(p_stof[1]);
    }
    else{                      //父进程
        close(p_ftos[0]);
        close(p_stof[1]);
        write(p_ftos[1],"ping",4);
        close(p_ftos[1]);
        read(p_stof[0],buf,4);
        close(p_stof[0]);
        printf("%d: received pong\n",getpid());
    }
    exit(0);
}