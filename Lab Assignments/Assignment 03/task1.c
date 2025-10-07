#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

struct shared {
    char sel[100];
    int b;
};

int main() {
    int shm_id;
    key_t shm_key = 1234;
    struct shared *mem;

    shm_id = shmget(shm_key, sizeof(struct shared), 0666 | IPC_CREAT);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    mem = (struct shared*) shmat(shm_id, NULL, 0);

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(1);
    }

    printf("Provide Your Input From Given Options:\n");
    printf("1. Type a to Add Money\n");
    printf("2. Type w to Withdraw Money\n");
    printf("3. Type c to Check Balance\n");

    char user_choice;
    scanf(" %c", &user_choice);
    mem->sel[0] = user_choice;
    mem->sel[1] = '\0';
    mem->b = 1000;

    printf("Your selection: %c\n\n", user_choice);
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        if (strcmp(mem->sel, "a") == 0) {
            int amt;
            printf("Enter amount to be added:\n");
            scanf("%d", &amt);
            if (amt > 0) {
                mem->b += amt;
                printf("Balance added successfully\n");
                printf("Updated balance after addition:\n%d\n", mem->b);
            } else {
                printf("Adding failed, Invalid amount\n");
            }
        }
        else if (strcmp(mem->sel, "w") == 0) {
            int amt;
            printf("Enter amount to be withdrawn:\n");
            scanf("%d", &amt);
            if (amt > 0 && amt < mem->b) {
                mem->b -= amt;
                printf("Balance withdrawn successfully\n");
                printf("Updated balance after withdrawal:\n%d\n", mem->b);
            } else {
                printf("Withdrawal failed, Invalid amount\n");
            }
        }
        else if (strcmp(mem->sel, "c") == 0) {
            printf("Your current balance is:\n%d\n", mem->b);
        }
        else {
            printf("Invalid selection\n");
        }
        char msg[] = "Thank you for using\n";
        write(pipe_fd[1], msg, strlen(msg)+1);

        close(pipe_fd[1]);
        exit(0);
    } 
    else {
        wait(NULL);

        char buffer[100];
        read(pipe_fd[0], buffer, sizeof(buffer));
        printf("%s", buffer);

        shmdt(mem);
        shmctl(shm_id, IPC_RMID, NULL);
    }
    return 0;
}
