#include "kernel/types.h"
#include "user.h"

void screening(int *input, int num)
{
    if (num==0) {
        return;
    }
    int p[2],i,prime=*input;
    pipe(p);
    char buff[4];
    printf("prime %d\n",prime);     //将第一个接收到的数字打印
    int pid=fork();
	if(pid<0){
		printf("error!");
	}
	else if (pid==0) {
	    close(p[0]);
	    for (i=0;i<num;i++) {
	        write(p[1], (char *)(input + i), 4);    //将所有数字写入管道
	    }
	    close(p[1]);
	    exit(0);
    } 
	else {
	    close(p[1]);
	    num=0;
	    while(read(p[0],buff,4)) {
	        int temp=*((int*)buff);
	        if ((temp%prime)!=0) {    
	            *input++=temp;      //将不能被第一个数整除的数保留下来进行下一轮筛选
		        num++;            //保留的数字的个数
	        }
	    }
	    screening(input-num,num);
	    close(p[0]);
	    wait(0);
    }
}

int main(int argc, char *argv[]) {
  int input[34];
  for (int i=0;i<34;i++) {
    input[i]=i+2;
  }
  screening(input, 34);
  exit(0);
}



