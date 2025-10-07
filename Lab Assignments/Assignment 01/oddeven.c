#include <stdio.h>

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        int num = 0;
        for (int k = 0; argv[i][k] != '\0'; k++) {
            num = num * 10 + (argv[i][k] - '0');
        }
        
        if (num % 2 == 0)
            printf("%d is Even\n", num);
        else
            printf("%d is Odd\n", num);
    }
    return 0;
}
