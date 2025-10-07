#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int p;
int *fibArray = NULL;
int qcount;
int *qindices;
void* generateFib(void *arg);
void* searchFib(void *arg);

int main() {
    pthread_t t1, t2;
    printf("Enter the term of fibonacci sequence:\n");
    scanf("%d", &p);
    if (p < 0 || p > 40) {
        printf("Value out of range.\n");
        return 0;
    }
    pthread_create(&t1, NULL, generateFib, NULL);
    pthread_join(t1, NULL);

    for (int i = 0; i <= p; i++) {
        printf("a[%d] = %d\n", i, fibArray[i]);
    }
    printf("How many numbers you are willing to search?:\n");
    scanf("%d", &qcount);

    if (qcount < 0) {
        printf("Invalid search value.");
        free(fibArray);
        return 0;
    }
    qindices = (int*)malloc(qcount * sizeof(int));
    for (int i = 0; i < qcount; i++) {
        printf("Enter search %d:\n", i + 1);
        scanf("%d", &qindices[i]);
    }
    pthread_create(&t2, NULL, searchFib, NULL);
    pthread_join(t2, NULL);
    free(fibArray);
    free(qindices);
    return 0;
}

void* generateFib(void *arg) {
    fibArray = (int*)malloc((p + 1) * sizeof(int));

    if (p >= 0) fibArray[0] = 0;
    if (p >= 1) fibArray[1] = 1;
    for (int i = 2; i <= p; i++) {
        fibArray[i] = fibArray[i - 1] + fibArray[i - 2];
    }
    pthread_exit(NULL);
}

void* searchFib(void *arg) {
    for (int i = 0; i < qcount; i++) {
        int idx = qindices[i];
        if (idx >= 0 && idx <= p) {
            printf("result of search #%d = %d\n", i + 1, fibArray[idx]);
        } else {
            printf("result of search #%d = -1\n", i + 1);
        }
    }
    pthread_exit(NULL);
}
