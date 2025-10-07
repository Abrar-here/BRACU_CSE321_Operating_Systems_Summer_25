#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    pid_t pid = fork();

    if (pid < 0) {
        printf("Fork failed.\n");
        return 1;
    }

    if (pid == 0) {
        execv("./sort", argv);
        printf("Exec failed for sort.\n");
    } 
    else {
        wait(NULL);
        execv("./oddeven", argv);
        printf("Exec failed for oddeven.\n");
    }

    return 0;
}
