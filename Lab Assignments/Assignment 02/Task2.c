#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

sem_t semStudents;
sem_t semTutor;
pthread_mutex_t lock;

int waiting = 0;
int served = 0;
int totalStudents = 10;
int maxChairs = 3;

void* tutorThread(void* arg) {
    while (served < totalStudents) {
        sem_wait(&semStudents);
        pthread_mutex_lock(&lock);
        if (waiting > 0) {
            waiting--;
            printf("A waiting student started getting consultation\n");
            printf("Number of students now waiting: %d\n", waiting);
            pthread_mutex_unlock(&lock);
            printf("ST giving consultation\n");
            sem_post(&semTutor);
            sleep(1);
        }
        else {
            pthread_mutex_unlock(&lock);
        }
    }
    return NULL;
}
void* studentThread(void* arg) {
    int id = *(int*)arg;
    sleep(rand() % 4);
    pthread_mutex_lock(&lock);
    if (waiting < maxChairs) {
        printf("Student %d started waiting for consultation\n", id);
        waiting++;
        sem_post(&semStudents);
        pthread_mutex_unlock(&lock);
        sem_wait(&semTutor);
        printf("Student %d is getting consultation\n", id);
        sleep(1);
        printf("Student %d finished getting consultation and left\n", id);
        pthread_mutex_lock(&lock);
        served++;
        printf("Number of served students: %d\n", served);
        pthread_mutex_unlock(&lock);
    } 
    else {
        printf("No chairs remaining in lobby. Student %d Leaving.....\n", id);
        pthread_mutex_unlock(&lock);
        pthread_mutex_lock(&lock);
        served++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}
int main() {
    pthread_t tutor;
    pthread_t students[10];
    int ids[10];
    sem_init(&semStudents, 0, 0);
    sem_init(&semTutor, 0, 0);
    pthread_mutex_init(&lock, NULL);
    pthread_create(&tutor, NULL, tutorThread, NULL);

    for (int i = 0; i < totalStudents; i++) {
        ids[i] = i;
        pthread_create(&students[i], NULL, studentThread, &ids[i]);
        sleep(rand() % 2);
    }
    for (int i = 0; i < totalStudents; i++) {
        pthread_join(students[i], NULL);
    }
    pthread_join(tutor, NULL);
    sem_destroy(&semStudents);
    sem_destroy(&semTutor);
    pthread_mutex_destroy(&lock);
    return 0;
}