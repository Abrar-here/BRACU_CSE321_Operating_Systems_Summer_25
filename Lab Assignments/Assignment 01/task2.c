#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t ch_pid, grndch_pid;
    ch_pid = fork();

    if (ch_pid == 0) {
        grndch_pid = fork();
        if (grndch_pid == 0) {
            printf("I am grandchild\n");
        } 
        else if (grndch_pid > 0) {
            wait(NULL); 
            printf("I am child\n");
        }
    } else if (ch_pid > 0) {
        wait(NULL);
        printf("I am parent\n");
    }
    return 0;
}
