#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_FILE "/dev/my_sysfs_driver"
#define BUFFER_SIZE 1024

void read_device()
{
    int fd;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Open the device file for reading
    fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        exit(EXIT_FAILURE);
    }

    // Read from the device file
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        perror("Failed to read from device file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    buffer[bytes_read] = '\0'; // Null-terminate the read data
    printf("Read from device:\n%s\n", buffer);

    close(fd);
}

void write_device()
{
    int fd;
    ssize_t bytes_written;
    char addr1[100], addr2[100], size[100];
    char data[BUFFER_SIZE];

    // Get input from the user for attributes
    printf("Enter value for address1: ");
    scanf("%99s", addr1);

    printf("Enter value for address2: ");
    scanf("%99s", addr2);

    printf("Enter value for size: ");
    scanf("%99s", size);

    // Format the data to be written
    snprintf(data, sizeof(data), "address1: %s address2: %s size: %s", addr1, addr2, size);

    // Open the device file for writing
    fd = open(DEVICE_FILE, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open device file");
        exit(EXIT_FAILURE);
    }

    // Write to the device file
    bytes_written = write(fd, data, strlen(data));
    if (bytes_written < 0) {
        perror("Failed to write to device file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Wrote to device: %s\n", data);

    close(fd);
}

int main()
{
    int choice;
 repeat: 
  while(1){
    printf("Choose an operation:\n");
    printf("1. Read from device\n");
    printf("2. Write to device\n");
    printf("3. Exit from device\n");
    printf("Enter your choice: ");
    scanf("%d", &choice);

    switch (choice) {
        case 1:
            printf("Reading device...\n");
            read_device();
            goto repeat;
        //    break;
        case 2:
            printf("Writing to device...\n");
            write_device();
            goto repeat;
        //    break;
	case 3:
	    printf("Thank you !");
	    return 0;
        default:
            printf("Invalid choice. Exiting.\n");
            exit(EXIT_FAILURE);
    }
  }
    return 0;
}

