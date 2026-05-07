#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define PID_FILE ".monitor_pid"

volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {

    if (sig == SIGINT) {
        char msg[] = "\n SIGINT received. Deleting PID file and shutting down\n";
        write(STDOUT_FILENO, msg, strlen(msg) - 1);
        keep_running = 0;
    }
    else if (sig == SIGUSR1) {
        char msg[] = "\nSignal SIGUSR1 received: A new report has been added to the system!\n";
        write(STDOUT_FILENO, msg, strlen(msg) - 1);
    }
}
int main() {
    pid_t my_pid = getpid();

    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        perror("Error at the PID file creation\n");
        return -1;
    }
    
    char pid_str[20];
    sprintf(pid_str, "%d\n", my_pid);
    if (write(fd, pid_str, strlen(pid_str)) < 0) {
        perror("Error at the PID file write\n");
        close(fd);
        return -1;
    }
    close(fd);
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("Error at the SIGUSR1 handler\n");
        return -1;
    }

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("Error at the SIGINT handler\n");
        return -1;
    }

    while (keep_running) {
        pause();
    }

    if (unlink(PID_FILE) == -1) {
        perror("Error: at deleting the PID file .monitor_pid\n");
    }
    else {
        printf("PID file %s successfully deleted\n", PID_FILE);
    }

    return 0;
//foloseste assing save asta este pentru variabile globale pentru  a evita spre ex situatia de pemtru un print hello world o sa apara hello hello word in cel mai bun caz deci de vazut acolo cum se implemnateaza si adaugat 
}
