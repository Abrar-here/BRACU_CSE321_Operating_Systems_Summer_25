#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int a, b, c;
    int cnt = 1; 
    pid_t main_pid = getpid(); 

    a = fork();
    if (a > 0) cnt++; 
    if (a > 0 && (a % 2 == 1)) { 
        if (fork() > 0) cnt++;
    }

    b = fork();
    if (b > 0) cnt++;
    if (b > 0 && (b % 2 == 1)) {
        if (fork() > 0) cnt++;
    }

    c = fork();
    if (c > 0) cnt++;
    if (c > 0 && (c % 2 == 1)) {
        if (fork() > 0) cnt++;
    }

    while (wait(NULL) > 0);

    if (getpid() == main_pid) {
        printf("Total processes created (including parent): %d\n", cnt);
    }

    return 0;
}
