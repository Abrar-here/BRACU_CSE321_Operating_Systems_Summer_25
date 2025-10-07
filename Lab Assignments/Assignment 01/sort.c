#include <stdio.h>

int main(int argc, char *argv[]) {
    int arr[100], n = argc - 1;

    for (int i = 0; i < n; i++) {
        arr[i] = 0;
        for (int j = 0; argv[i + 1][j] != '\0'; j++) {
            arr[i] = arr[i] * 10 + (argv[i + 1][j] - '0');
        }
    }

    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] < arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    return 0;
}
