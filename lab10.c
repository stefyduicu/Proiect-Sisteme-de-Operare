#include<stdio.h>
#include<unistd.h>
//f[0]-read  f[1]-write
int main()
{

  int fd[2];
  pipe(fd);
  pid_t ret=fork();
  if(ret==0){
    close(fd[0]);
    dup2(fd[1],STDOUT_FILENO);
    close(fd[1]);
    while(1)
      {
	printf("hello world");
	fflush(stdout);
      }
  }
  else
    {
      close(fd[1]);
      char b[128];
      while(1)
	{
	  int n=read(fd[0],b,sizeof(b)-1);
	  b[n]='\0';
	  printf("%s",b);
	  fflush(stdout);
	}
    }
}
