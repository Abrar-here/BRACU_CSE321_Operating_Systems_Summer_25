#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>

struct msg {
    long int type;
    char txt[6];
};

int main() {
    int mqid = msgget(1234, 0666 | IPC_CREAT);
    if (mqid < 0) { 
        perror("msgget"); 
        exit(1); }

    char workspace[20];
    printf("Please enter the workspace name:\n");
    scanf("%s", workspace);

    if (strcmp(workspace, "cse321") != 0) {
        printf("Invalid workspace name\n");
        msgctl(mqid, IPC_RMID, NULL);
        exit(0);
    }

    struct msg wsMsg = {1, ""};
    strncpy(wsMsg.txt, workspace, 6);
    msgsnd(mqid, &wsMsg, sizeof(wsMsg.txt), 0);
    printf("Workspace name sent to otp generator from log in: %s\n", workspace);

    if (fork() == 0) {
        struct msg recvWs;
        msgrcv(mqid, &recvWs, sizeof(recvWs.txt), 1, 0);
        printf("OTP generator received workspace name from log in: %s\n", recvWs.txt);

        struct msg otpMsg = {2, ""};
        snprintf(otpMsg.txt, 6, "%d", getpid());
        msgsnd(mqid, &otpMsg, sizeof(otpMsg.txt), 0);
        printf("OTP sent to log in from OTP generator: %s\n", otpMsg.txt);

        otpMsg.type = 3;
        msgsnd(mqid, &otpMsg, sizeof(otpMsg.txt), 0);
        printf("OTP sent to mail from OTP generator: %s\n", otpMsg.txt);

        if (fork() == 0) {
            msgrcv(mqid, &otpMsg, sizeof(otpMsg.txt), 3, 0);
            printf("Mail received OTP from OTP generator: %s\n", otpMsg.txt);
            otpMsg.type = 4;
            msgsnd(mqid, &otpMsg, sizeof(otpMsg.txt), 0);
            printf("OTP sent to log in from mail: %s\n", otpMsg.txt);
            exit(0);
        }
        wait(NULL);
        exit(0);
    }
    wait(NULL);
    struct msg otpFromGen, otpFromMail;
    msgrcv(mqid, &otpFromGen, sizeof(otpFromGen.txt), 2, 0);
    printf("Log in received OTP from OTP generator: %s\n", otpFromGen.txt);
    msgrcv(mqid, &otpFromMail, sizeof(otpFromMail.txt), 4, 0);
    printf("Log in received OTP from mail: %s\n", otpFromMail.txt);

    printf("%s\n", strcmp(otpFromGen.txt, otpFromMail.txt) == 0 ? "OTP Verified" : "OTP Incorrect");
    msgctl(mqid, IPC_RMID, NULL);
    return 0;
}
