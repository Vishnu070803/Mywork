#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/ioctl.h>
#define PAGE_SIZE 4096
#define DEVICE_FILE "/dev/vconv_driver"
#define BUFFER_SIZE_L (196 * 1024) 
#define BUFFER_SIZE_K (196)       
#define MAGIC_NUMBER 'a'
#define BD_WRITE _IOW(MAGIC_NUMBER, 1, char[200])
#define BD_CREATE _IOW(MAGIC_NUMBER, 2, char[200])
#define BD_CHECK _IOR(MAGIC_NUMBER, 3, char[500])


static int fd;

void dma_poll(void);

/* To clear Buffer*/
void clear_stdin() {
	int c;
	while ((c = getchar()) != '\n' && c != EOF) {}
}


/* Common function to write into axi-dma registers */
void dma_write(char *buf) {
	int bytes_written;
	bytes_written = write(fd, buf, strlen(buf));
		printf("data in dma_eriting: %s \n", buf);
	if (bytes_written < 0) {
		perror("Failed to write to device file");
		exit(EXIT_FAILURE);
	}
	// printf("Wrote to device: %s\n", buf);

}

/* Dma read function to read the last written parameters from userspace */
void dma_read(void)
{
	char buffer[4096];
	ssize_t bytes_read;
	bytes_read = read(fd, buffer, sizeof(buffer) - 1);
	if (bytes_read < 0) {
		perror("Failed to read from device file");
		exit(EXIT_FAILURE);
	}
	buffer[bytes_read] = '\0';
	printf("Data from device:\n%s\n", buffer);
}
/* To choose a register for writing */
void dma_writing(char *buffer, int pass)
{

	char data[160];
	if (pass==1) {
		snprintf(data, sizeof(data), "SCR: %s", buffer);
		dma_write(data);
	}
	else if(pass==2) {
		snprintf(data, sizeof(data), "STL: %s", buffer);
		data[sizeof(data) - 1] = '\0';
		printf("data in dma_eriting: %s \n", data);
		dma_write(data);
		dma_poll();
	}
	else if(pass==3) {
		snprintf(data, sizeof(data), "DCR: %s", buffer);
		dma_write(data);
	}
	else if(pass==4) {
		snprintf(data, sizeof(data), "DTL: %s", buffer);
		dma_write(data);
		dma_poll();
	}
	else if(pass==5) {
		snprintf(data, sizeof(data), "DMARUN: %s", buffer);
		dma_write(data);
	}
	else if(pass==6) {
		snprintf(data, sizeof(data), "DMASTOP: %s", buffer);
		dma_write(data);
	}
	else if(pass==7) {
		snprintf(data, sizeof(data), "DMAERROR: %s", buffer);
		dma_write(data);
	}
	else {
		snprintf(data, sizeof(data), "DMARESET: %s", buffer);
		dma_write(data);
	}

}

/* Dma Operations */
int dma_func(void) {
	int pass;
	unsigned long number;
	char buffer[100];
	int choice1;
	int choice2;
	repeat2:
	printf("Choose an operation:\n");
	printf("1. To Write into DMA device\n");
	printf("2. To Read the last written parameters from userspace\n");
	printf("3. For main menu\n");
	printf("Enter your choice: ");
	while(1) {
		if (scanf("%d", &choice1) != 1) {
			printf("Invalid input! Try again\n");
			clear_stdin();
			continue;
		}
		switch (choice1) {
		case 1:
		repeat3:
			printf("Choose an DMA Operation:\n");
			printf("1. MM2S Current Buffer Descriptor\n");
			printf("2. MM2S and s2mm Tail Buffer Descriptor\n");
			printf("3. S2MM Current Buffer Descriptor\n");
			printf("4. S2MM Tail Buffer Descriptor(wont work)\n");
			printf("5. DMA ON\n");
			printf("6. DMA OFF\n");
			printf("7. Error Checking\n");
			printf("8. Reset\n");
			printf("9. To go into previous menu\n");
			printf("--Please make sure providing address are aligned--\n ");
			while(1) {
				if (scanf("%d", &choice2) != 1) {
					printf("Invalid input! Try again\n");
					clear_stdin();
					continue;
				}
				switch (choice2) {
				case 1:
					printf("--Please write first buffer descriptor address into current buffer descriptor register(MM2S)-- \n");
					while(1) {
						if (scanf("%li", &number) != 1) {
							printf("Invalid input!, Try again.\n");
							clear_stdin();
							continue;
						}
						break;
					}
					snprintf(buffer, sizeof(buffer), "0x%lx", number);
					pass=1;
					dma_writing(buffer, pass);
					break;
				case 2:
					printf("--Please write address of last buffer descriptor(MM2S)--- \n");
					while(1) {
						if (scanf("%li", &number) != 1) {
							printf("Invalid input!, Try again.\n");
							clear_stdin();
							continue;
						}
						break;
					}
					printf("--Please write address of last buffer descriptor(S2MM)--- \n");
					while(1) {
						if (scanf("%li", &number1) != 1) {
							printf("Invalid input!, Try again.\n");
							clear_stdin();
							continue;
						}
						break;
					}

					snprintf(buffer, sizeof(buffer), "0x%lx 0x%lx", number, number1);
					pass=2;
					dma_writing(buffer, pass);
					break;
				case 3:
					printf("--Please write first buffer descriptor address into current buffer descriptor register(S2MM)---\n ");
					while(1) {
						if (scanf("%li", &number) != 1) {
							printf("Invalid input!, Try again.\n");
							clear_stdin();
							continue;
						}
						break;
					}
					snprintf(buffer, sizeof(buffer), "0x%lx", number);
					buffer[sizeof(buffer) - 1] = '\0';
					printf("Data in dma_func: %s\n", buffer);
					pass=3;
					dma_writing(buffer, pass);
					break;
				case 4:
					printf("--Please write last buffer descriptor address into tail buffer descriptor register(S2MM)--- \n");
					while(1) {
						if (scanf("%li", &number) != 1) {
							printf("Invalid input!, Try again.\n");
							clear_stdin();
							continue;
						}
						break;
					}
					snprintf(buffer, sizeof(buffer), "0x%lx", number);
					pass=4;
					dma_writing(buffer, pass);
					break;
				case 5:
					printf("1. MM2S Channel\n");
					printf("2. S2MM Channel\n");
					printf("Choose an DMA channel:\n");
					while(1) {
						if (scanf("%d", &choice1) != 1) {
							printf("Invalid input! Try again.\n");
							clear_stdin();
							continue;
						}
						pass = 5;
						if(choice1 == 1) {
							strcpy(buffer, "0x0");
							dma_writing(buffer, pass);
							break;
						} else if(choice1 == 2) {
							strcpy(buffer, "0x30");
							dma_writing(buffer, pass);
							break;
						} else {
							printf("Invalid Choice! Try again.\n");
						}
					}
					break;
				case 6:
					printf("1. MM2S Channel\n");
					printf("2. S2MM Channel\n");
					printf("Choose an DMA channel:\n");
					while(1) {
						if (scanf("%d", &choice1) != 1) {
							printf("Invalid input! Try again\.n");
							clear_stdin();
							continue;
						}
						pass = 6;
						if(choice1 == 1) {
							strcpy(buffer, "0x0");
							dma_writing(buffer, pass);
							break;
						} else if(choice1 == 2) {
							strcpy(buffer, "0x30");
							dma_writing(buffer, pass);
							break;
						} else {
							printf("Invalid Choice! Try again.\n");
						}
					}
					break;
				case 7:
					printf("1. MM2S Channel\n");
					printf("2. S2MM Channel\n");
					printf("Choose an DMA channel:\n");
					while(1) {
						if (scanf("%d", &choice1) != 1) {
							printf("Invalid input! Try again.\n");
							clear_stdin();
							continue;
						}
						pass = 7;
						if(choice1 == 1) {
							strcpy(buffer, "0x0");
							dma_writing(buffer, pass);
							dma_poll();
							break;
						} else if(choice1 == 2) {
							strcpy(buffer, "0x30");
							dma_writing(buffer, pass);
							dma_poll();
							break;
						} else {
							printf("Invalid Choice! Try again.\n");
						}
					}
					break;
				case 8:
					printf("1. MM2S Channel\n");
					printf("2. S2MM Channel\n");
					printf("Choose an DMA channel:\n");
					while(1) {
						if (scanf("%d", &choice1) != 1) {
							printf("Invalid input! Try again.\n");
							clear_stdin();
							continue;
						}
						pass = 8;
						if(choice1 == 1) {
							strcpy(buffer, "0x0");
							dma_writing(buffer, pass);
							break;
						} else if(choice1 == 2) {
							strcpy(buffer, "0x30");
							dma_writing(buffer, pass);
							break;
						} else {
							printf("Invalid Choice! Try again.\n");
						}

					}
					break;
				case 9:
					goto repeat2;
				default:
					printf("Invalid choice!, Try Again\n");
					break;
				}
				goto repeat3;
			}
			break;
		case 2:
			dma_read();
			goto repeat2;
		case 3:
			return 0;
		default:
			printf("Invalid choice. Try again.\n");
		}
	}
}

int common(int option) {
	unsigned long phys_addr;
	unsigned long mem_size;
	int fd1;
	void *mapped;
	off_t page_offset, aligned_addr;
	while (1) {
		printf("Enter the physical address (in hexadecimal format):\n");
		if (scanf("%lx", &phys_addr) != 1) {
			printf("Invalid input! Try again.\n");
			clear_stdin();
			continue;
		}

		printf("Enter the memory size (in bytes):\n");
		if (scanf("%lu", &mem_size) != 1 || mem_size == 0) {
			printf("Invalid input!. Try again.\n");
			clear_stdin();
			continue;
		}
		clear_stdin();
		break;
	}
	/* Calculate page alignment */
	page_offset = phys_addr % PAGE_SIZE;
	aligned_addr = phys_addr - page_offset;

	fd1 = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd1 < 0) {
		perror("open");
		return -1;
	}

	mapped = mmap(NULL, mem_size + page_offset, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, aligned_addr);
	if (mapped == MAP_FAILED) {
		perror("mmap");
		close(fd1);
		return -1;
	}

	//printf("Mapped address: %p\n", (char *)mapped + page_offset);

	volatile uint32_t *ptr = (volatile uint32_t *)((char *)mapped + page_offset);
	size_t num_entries = mem_size / sizeof(uint32_t);
	/* For Writing */
	if (option==1) {
		unsigned long input;
		while (1) {
			printf("Enter first number in the sequence that you want to write:\n");
			if (scanf("%li", &input) != 1) {
				printf("Invalid input! Try again.\n");
				clear_stdin();
				continue;
			}
			for (size_t i = 0; i < num_entries; i++) {
				ptr[i] = (uint32_t)(input);
				input ++;
			}
			clear_stdin();
			break;
		}
	} else if(option == 2) {
		for (size_t i = 0; i < num_entries; i++) {
			uint32_t value = ptr[i];
			printf("Address: %p, Read: 0x%X\n", (void *)(ptr + i), value);
		}
	}

	if (munmap(mapped, mem_size + page_offset) < 0) {
		perror("munmap");
	}

	close(fd1);
	return 0;
}


/* For polling to know the status of dma data transaction */
void dma_poll(void) {
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;

	int ret = poll(&pfd, 1, 20000);
	if (ret == -1) {
		perror("poll() failed");
		return;
	}
	if (ret == 0) {
		printf("Timeout occurred! No data ready within 20000 milliseconds.\n");
	} else {
		if (pfd.revents & POLLIN) {
			printf("Transfer success in this channel!\n");
		} else if (pfd.revents & POLLERR){
			printf("Error:- Transfer Failed!\n");
			return ;
		}else {
			printf("Unexpected event occurred!\n");
			return ;
		}
	}
	return ;
}

void dma_descriptor(char *buffer, int pass) {
	char data[600];
	if (pass == 1 || pass == 2) {
		snprintf(data, sizeof(data), "BDC: %s %d", buffer, pass);
		ioctl(fd, BD_CREATE, data);
	} else if (pass == 3 || pass == 4) {
		snprintf(data, sizeof(data), "BDW: %s %d", buffer, pass);
		ioctl(fd, BD_WRITE, data);
	} else if (pass == 5 || pass == 6) {
		snprintf(data, sizeof(data), "BDR: %s %d", buffer, pass);
		ioctl(fd, BD_CHECK, data);
		printf("Received from driver: %s\n", data);
	}  else {
		printf("Error! Invalid pass number \n");
		return;
	}
}

int buffer_descriptor() {
	int option, choice;
	unsigned long choice1;
	char buffer[600];
	unsigned long number;
descriptor:
	while (1) {
		printf("Choose an operation:\n");
		printf("1. To Create Buffer Descriptor\n");
		printf("2. To Write into buffer descriptor registers\n");
		printf("3. To Read the buffer descriptor\n");
		printf("4. To go back\n");
		if (scanf("%d", &choice) != 1) {
			printf("Invalid input! Try again.\n");
			clear_stdin();
			continue;
		}
		switch (choice) {
		case 1:
			printf("--Please select channel for buffer descriptor creation--\n");
			printf("1. For MM2S Channel\n");
			printf("2. For S2MM Channel\n");

			if (scanf("%ld", &choice1) != 1) {
				printf("Invalid input! Try again\n");
				clear_stdin();
				continue;
			}

			switch (choice1) {
			case 1:
				printf("--Enter number of channel descriptors required for MM2S--\n");
				if (scanf("%li", &number) != 1) {
					printf("Invalid input!, Try again.\n");
					continue;
				}
				if(number <= 0){
					printf("Invalid input!, should be positive integer.\n");
					continue;
				}
				snprintf(buffer, sizeof(buffer), "0x%lx", number);
				option = 1;
				dma_descriptor(buffer, option);
				break;

			case 2:
				printf("--Enter number of channel descriptors required for S2MM--\n");
				if (scanf("%li", &number) != 1) {
					printf("Invalid input!, Try again.\n");
					continue;
				}
				if(number <= 0){
					printf("Invalid input!, should be positive integer.\n");
					continue;
				}

				snprintf(buffer, sizeof(buffer), "0x%lx", number);
				option = 2;
				dma_descriptor(buffer, option);
				break;

			default:
				printf("Invalid channel selection!\n");
			}
			break;
		case 2:
			printf("--Please select channel for buffer descriptor Writing--\n");
			printf("1. For MM2S Channel\n");
			printf("2. For S2MM Channel\n");

			if (scanf("%ld", &choice1) != 1) {
				printf("Invalid input! Try again.\n");
				clear_stdin();
				continue;
			}

			switch (choice1) {
			case 1:
				printf("--Enter the required buffer descriptor number for writing (MM2S)--\n");
				if (scanf("%d", &choice) != 1) {
					printf("Invalid input! Try again.\n");
					clear_stdin();
					continue;
				}
				if(number <= 0){
					printf("Invalid input!, should be positive integer.\n");
					continue;
				}

				printf("--Enter the buffer address (MM2S)--\n");
				if (scanf("%li", &choice1) != 1) {
					printf("Invalid input!, Try again.\n");
					clear_stdin();
					continue;
				}
				printf("--Enter the buffer length (MM2S)--\n");
				printf("-- Remember length should be in the format 0x0-(bits-xxXX)-XXXXXX --\n");
				if (scanf("%li", &number) != 1) {
					printf("Invalid input! Try again.\n");
					clear_stdin();
					continue;
				}
				snprintf(buffer, sizeof(buffer), "%d 0x%lX 0x%lX", choice, choice1, number);
				option = 3;
				dma_descriptor(buffer, option);
				break;

			case 2:
				printf("--Enter the required buffer descriptor number for writing (S2MM)--\n");
				if (scanf("%d", &choice) != 1) {
					printf("Invalid input! Try again.\n");
					clear_stdin();
					continue;
				}
				if(number <= 0){
					printf("Invalid input!, should be positive integer.\n");
					continue;
				}


				printf("--Enter the buffer address (S2MM)--\n");
				if (scanf("%li", &choice1) != 1) {
					printf("Invalid input! Try again.\n");
					clear_stdin();
					continue;
				}
				printf("--Enter the buffer length (S2MM)--\n");
				printf("-- Remember length should be in the format 0x0-(bits-xxXX)-XXXXXX --\n");
				if (scanf("%li", &number) != 1) {
					printf("Invalid input! Try again.\n");
					clear_stdin();
					continue;
				}
				snprintf(buffer, sizeof(buffer), "%d 0x%lX 0x%lX", choice, choice1, number);
				option = 4;
				dma_descriptor(buffer, option);
				break;

			default:
				printf("Invalid channel selection! Try again.\n");
			}
			break;
		case 3:
			printf("-- Select the channel for buffer descriptor reading --\n");
			printf("1. For MM2S Channel\n");
			printf("2. For S2MM Channel\n");

			if (scanf("%ld", &choice1) != 1) {
				printf("Invalid input! Try again.\n");
				clear_stdin();
				continue;
			}

			switch (choice1) {
			case 1:
				printf("--Enter buffer descriptor number--\n");
				if (scanf("%li", &number) != 1) {
					printf("Invalid input!, Try again.\n");
					continue;
				}
				if(number <= 0){
					printf("Invalid input!, should be positive integer.\n");
					continue;
				}
				snprintf(buffer, sizeof(buffer), "0x%lx", number);
				option = 5;
				dma_descriptor(buffer, option);
				break;
			case 2:
				printf("--Enter buffer descriptor number-\n");
				if (scanf("%li", &number) != 1) {
					printf("Invalid input!, Try again.\n");
					continue;
				}
				if(number <= 0){
					printf("Invalid input!, should be positive integer.\n");
					continue;
				}
				snprintf(buffer, sizeof(buffer), "0x%lx", number);
				option = 6;
				dma_descriptor(buffer, option);
				break;
			default:
				printf("Invalid channel selection! Try again.\n");
			}
			break;
		case 4:
			return 0;

		default:
			printf("Invalid choice. Try again.\n");
		}
	goto descriptor;
	}
}
  

void load_file_to_ddr(const char *filename, unsigned long file_addr ,size_t size ) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Failed to open /dev/mem");
        fclose(file);
        exit(EXIT_FAILURE);
    } 
    unsigned long page_offset;
    unsigned long aligned_addr;

	page_offset = file_addr % PAGE_SIZE;
	aligned_addr = file_addr - page_offset; 

	void* mem = mmap(NULL, size + page_offset, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, aligned_addr);
	if (mem == MAP_FAILED) {
		perror("mmap");
		close(mem_fd);
		return;
	}

	//printf("Mapped address: %p\n", (char *)mapped + page_offset);

    void *file_mem = (void*)((char *)mem + page_offset);

    size_t elements_read = fread(file_mem, 1, size, file);
    printf("Elements read: %zu\n", elements_read);
    munmap(file_mem, size);
    close(mem_fd);
    fclose(file);
    printf("Loaded %s into DDR at address 0x%0lX\n", filename, file_addr);
}

int main()
{	unsigned long file_addr;
        struct stat st;
	int option;
	int choice;
	int ret;
	fd = open(DEVICE_FILE, O_RDWR);
	if (fd < 0) {
		perror("Failed to open device file");
		exit(EXIT_FAILURE);
	}	

	while(1){

		printf("Choose an operation:\n");
		printf("1. To Write into Memory\n");
		printf("2. To Read from Memory\n");
		printf("3. To Load bin files into Memory\n");
		printf("4. To DMA Sector\n");
		printf("5. To BUFFER-DESCRIPTORS sector\n");
		printf("6. To EXIT\n");
		if (scanf("%d", &choice) != 1) {
			printf("Invalid input! Try again.\n");
			clear_stdin();
			continue;
		}

		switch (choice) {
		case 1:

			ret = common(choice);
			if(ret == 0) {
				printf("Successfully written to DDR\n");
			} else {
				printf("Writing Failed into DDR\n");
			}
			break;
		case 2:

			ret = common(choice);
			if(ret == 0) {
				printf("Successfully readed from DDR\n");
			} else {
				printf("Reading from DDR failed\n");
			}
			break;
		case 3: 
			    while(1){
				printf("Enter the Address to store the files:\n");
				if(scanf("%li", &file_addr) != 1){
				printf("invalid Input! Try Again\n");
				clear_stdin();
				continue;
				}
				clear_stdin();
				break;
 			   }
			    if (stat("/tmp/k_0.bin", &st) == 0) {
			        printf("File size: %lld bytes\n", (long long)st.st_size);
 			    } else {
  			      perror("Error in reading file size");
				break;
			    }
    			load_file_to_ddr("/tmp/k_0.bin", file_addr, BUFFER_SIZE_K);
   			load_file_to_ddr("/tmp/l_0.bin", file_addr + (long long)st.st_size,BUFFER_SIZE_L);
			break;
		case 4:
			dma_func();
			break;
		case 5:

			buffer_descriptor();
			break;

		case 6:

			printf("Thank you and BYE !\n");
			return 0;
		default:
			printf("Invalid choice. Try again.\n");
		}
	}
	close(fd);
	return 0;
}


