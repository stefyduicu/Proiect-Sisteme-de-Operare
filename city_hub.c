#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>

void start_monitor()
{
    int filedes[2];
    if(pipe(filedes)<0)
    {
        perror("Problema creare pipe");
        return;
    }
    pid_t hub_mon=fork();
    if(hub_mon<0)
    {
        perror("Problema creare proces copil hub_mon");
        close(filedes[0]);
        close(filedes[1]);
        return;
    }
    if(hub_mon==0)
    {
        pid_t monitor=fork();
        if(monitor<0)
        {
            perror("Problema creare proces copil monitor");
            exit(1);
        }
        if(monitor==0)
        {
            close(filedes[0]);
            dup2(filedes[1],STDOUT_FILENO);
            close(filedes[1]);
            execl("./monitor_reports","monitor_reports",NULL);
            perror("A esuat inlocuirea procesului cu monitor_reports");
            exit(1);
        }
        close(filedes[1]);
        char buffer[512];
        int bytes_read;
        while((bytes_read=read(filedes[0],buffer,sizeof(buffer)-1))>0)
        {
            buffer[bytes_read]='\0';
            printf("MONITOR: %s",buffer);
            fflush(stdout);
            if(strstr(buffer,"TERMINATED:")!=NULL)
            {
                printf("HUB: Monitor ended\n");
                fflush(stdout);
            }
        }
        close(filedes[0]);
        wait(NULL);
        exit(0);
    }
    close(filedes[0]);
    close(filedes[1]);
    printf("Monitor now active\n");
}

void calculate_scores(char *districts[],int count)
{
    for(int i=0;i<count;i++)
    {
        int filedes[2];
        if(pipe(filedes)<0)
        {
            perror("Problema creare pipe");
            continue;
        }
        pid_t scorer=fork();
        if(scorer<0)
        {
            perror("Problema creare proces");
            close(filedes[0]);
            close(filedes[1]);
            continue;
        }
        if(scorer==0)
        {
            close(filedes[0]);
            dup2(filedes[1],STDOUT_FILENO);
            close(filedes[1]);
            execl("./scorer","scorer",districts[i],NULL);
            perror("Problema la execul scorer-ului");
            exit(1);
        }
        close(filedes[1]);
        char buffer[512];
        int bytes_read;
        while((bytes_read=read(filedes[0],buffer,sizeof(buffer)-1))>0)
        {
            buffer[bytes_read]='\0';
            printf("%s",buffer);
        }
        close(filedes[0]);
        waitpid(scorer,NULL,0);
    }
}

int main()
{
    char command[512];
    while(1)
    {
        printf("city_hub> ");
        fflush(stdout);
        if(fgets(command,sizeof(command),stdin)==NULL)
            break;
        command[strcspn(command,"\n")]='\0';

        if(strcmp(command,"exit")==0)
            break;
        else if(strcmp(command,"start_monitor")==0)
            start_monitor();
        else if(strncmp(command,"calculate_scores",16)==0)
        {
            char *districts[20];
            int count=0;
            char *token=strtok(command," ");
            token=strtok(NULL," ");
            while(token!=NULL&&count<20)
            {
                districts[count++]=token;
                token=strtok(NULL," ");
            }
            if(count==0)
                printf("Folosire: calculate_scores <district1> [district2] ...\n");
            else
                calculate_scores(districts,count);
        }
        else
            printf("Unknown command\n");
    }
    return 0;
}
