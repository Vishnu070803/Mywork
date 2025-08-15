#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#define BUFFER_SIZE 1024

int8_t write_buf[BUFFER_SIZE] = {0};
int8_t read_buf[BUFFER_SIZE] = {0};

int main() {
    int fd;
    char option;

    // Open the device file
    fd = open("/dev/vishnu", O_RDWR); // Updated device name
    if (fd < 0) {
        perror("Cannot open device file");
        return EXIT_FAILURE;
    }

    while (1) {
        printf("**** Please Enter the Option ******\n");
        printf("        1. Write               \n");
        printf("        2. Read                \n");
        printf("        3. Exit                \n");
        printf("*********************************\n");
        scanf(" %c", &option);
        printf("Your Option = %c\n", option);

        switch (option) {
            case '1':
                printf("Enter the string to write into driver: ");
                scanf(" %[^\t\n]", write_buf); // Reading string

                // Write the string to the device
                ssize_t result = write(fd, write_buf, strlen(write_buf) + 1);
                if (result < 0) {
                    perror("Failed to write to device");
                } else {
                    printf("Data Writing ... Done!\n");
                }
                break;

            case '2':
                // Read data from the device
                ssize_t bytes_read = read(fd, read_buf, sizeof(read_buf));
                if (bytes_read < 0) {
                    perror("Failed to read from device");
                } else {
                    printf("Data Reading ... Done!\n");
                    printf("Data = %s\n", read_buf);
                }
                break;

            case '3':
                // Close the file descriptor and exit
                close(fd);
                return EXIT_SUCCESS;

            default:
                printf("Enter a Valid option = %c\n", option);
                break;
        }
    }
 //   close(fd); // This line will not be reached due to the return in case '3'
    return EXIT_SUCCESS;
}

