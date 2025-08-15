#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define DEVICE_PATH "/dev/key0"
#define BUFFER_SIZE 64

int main(void) {
    int fd;
    ssize_t bytes_read;
    char buffer[BUFFER_SIZE];

    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open the device");
        return EXIT_FAILURE;
    }

    printf("Listening for key events...\n");

    while (1) {
        bytes_read = read(fd, buffer, BUFFER_SIZE);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;  // Retry if interrupted
            }
            perror("Read error");
            break;
        }

        if (bytes_read == 0) {
            printf("No data read.\n");
            continue;
        }

        printf("Received %zd bytes: ", bytes_read);
        for (ssize_t i = 0; i < bytes_read; i++) {
            printf("0x%02x ", (unsigned char)buffer[i]);
        }
        printf("\n");
    }

    close(fd);
    return EXIT_SUCCESS;
}

