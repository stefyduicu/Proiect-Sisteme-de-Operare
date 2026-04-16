#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

typedef struct report
{
    char id[128];
    char inspector_name[128];
    float latitude,longitude;
    char issue_category[64];
    int severity;
    time_t timestamp;
    char description_text[512];
}report;

void create_district(char district[])
{
    mkdir(district,0750);
    char path[256];

    //reports.dat
    sprintf(path,"%s/reports.dat",district);
    int file1=open(path,O_CREAT | O_RDWR,0664);
    close(file1);
    chmod(path,0664);

    //district.cfg
    sprintf(path,"%s/district.cfg",district);
    int file2=open(path,O_CREAT | O_RDWR,0640);
    close(file2);
    chmod(path,0640);

    //logged_district
    sprintf(path,"%s/logged_district",district);
    int file3=open(path,O_CREAT | O_RDWR,0644);
    close(file3);
    chmod(path,0644);

}

void add(char district[],int x,int y,char issue_category[],int severity,char description_text[])
{
    // si managerul si inspectorul pot adauga, nu fac stat pentru add
    char path[100];
    sprintf(path,"%s/reports.dat",district);
    int file=open(path,O_WRONLY | O_APPEND);
    if(file<0)
    {
        create_district(district);
    }

}

void list(char district[])
{
  stat();
}

void view(char district[],char report[])
{
  stat();
}

void remove_report(char district[],char report [])
{
  stat();
}

int main(int argc,char **argv)
{

}
