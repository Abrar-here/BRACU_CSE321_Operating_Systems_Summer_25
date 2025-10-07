#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t ch_pid, grndch_pid;
    int i;

    printf("1. Parent process ID : 0\n");

    ch_pid= fork();
    if (ch_pid< 0) {
        return 1; 
    }

    if (ch_pid == 0) {
        printf("2. Child process ID: %d\n", getpid());
        for (i = 0; i < 3; i++) {
            grndch_pid = fork();
            if (grndch_pid < 0) {
                return 1;
            }
            if (grndch_pid == 0) {
                printf("%d. Grand Child process ID: %d\n", 3 + i, getpid());
                return 0;
            }
        }
        for (i = 0; i < 3; i++) {
            wait(NULL);
        }

        return 0;
    } else {
        wait(NULL);
    }

    return 0;
}
