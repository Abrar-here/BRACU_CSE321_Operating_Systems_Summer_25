#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "a");
    if (file == NULL) {
        printf("Error opening or creating the file.\n");
        return 1;
    }

    char input[256];

    printf("Enter strings to write to the file:\n");
    while (1) {
        printf("Enter string: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        if (input[0] == '-' && input[1] == '1' && input[2] == '\n') {
            break;
        }

        fputs(input, file);
    }

    fclose(file);
    printf("Data written to %s successfully.\n", argv[1]);

    return 0;
}
