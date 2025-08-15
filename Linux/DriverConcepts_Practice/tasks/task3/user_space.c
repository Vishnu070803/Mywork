#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#define BUFFER_SIZE 1024

char write_buf[BUFFER_SIZE] = "write attr1:";
char temp[BUFFER_SIZE];
char read_buf1[BUFFER_SIZE] = "read attr1:";
char read_buf2[BUFFER_SIZE] = "read attr2:";

int main() {
    int fd;
    char option;

    // Open the device file
    fd = open("/dev/mysysfs_dev", O_RDWR); // Updated device name
    if (fd < 0) {
        perror("Cannot open device file");
        return EXIT_FAILURE;
    }

    while (1) {
        printf("**** Please Enter the Option ******\n");
        printf("        1. To write attr1               \n");
        printf("        2. To read attr1                \n");
        printf("        3. To read attr2                \n");
        printf("        4. Close                        \n");
        printf("*********************************\n");
        scanf(" %c", &option);
        printf("Your Option = %c\n", option);

        switch (option) {
            case '1':
                printf("Enter the string to write into driver: ");
                scanf(" %[^\t\n]", write_buf+12); // Reading string

                // Write the string to the device
                ssize_t result = write(fd, write_buf, strlen(write_buf) + 1);
                if (result < 0) {
                    perror("Failed to write to device");
                } else {
                    printf("Data Writing ... Done!\n");
                }
                break;

            case '2':
                // Send read command to device
                strcpy(temp, read_buf1); // Prepare the read command
                ssize_t bytes_read1 = read(fd, write_buf, sizeof(write_buf));
                if (bytes_read1 < 0) {
                    perror("Failed to read from device");
                } else {
                    printf("Data Reading ... Done!\n");
                    printf("Data = %s\n", write_buf); // Corrected to print write_buf
                }
                break;
                
            case '3':
                // Send read command to device
                strcpy(temp, read_buf2); // Prepare the read command
                ssize_t bytes_read2 = read(fd, write_buf, sizeof(write_buf));
                if (bytes_read2 < 0) {
                    perror("Failed to read from device");
                } else {
                    printf("Data Reading ... Done!\n");
                    printf("Data = %s\n", write_buf); // Corrected to print write_buf
                }
                break;
                
            case '4':
                // Close the file descriptor and exit
                close(fd);
                return EXIT_SUCCESS;

            default:
                printf("Enter a Valid option = %c\n", option);
                break;
        }
    }

    // This line will not be reached due to the return in case '4'
    close(fd);
    return EXIT_SUCCESS;
}

