#include<stdio.h>
#include<unistd.h>

int main()
{
  int pd[2];
  int pid;
  FILE *stream;
  if(pipe(pfd)<0)
    {
      printf("Eroare la crearea pipe-ului\n");
      exit(-1);
    }
  if((pid=fork())<0)
    {
      printf("Eroare la fork\n");
      exit(-1);
    }
  if(pid==0){
    close(pdf[0]);
    dup2(prd[1],STDOUT_FILENO);
    execlp("./prg    ","./prg    ",NULL);
    printf("Eroare la exec\n");
  }
  close(pdf[1]);
  stream=fdopen(pdf[0],"r");
  fscanf(stream,"%s",string);
  close(pdf[0]);
  return 0;
}
