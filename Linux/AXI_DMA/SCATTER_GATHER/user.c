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
#define MAGIC_NUMBER 'a'
#define BD_WRITE _IOW(MAGIC_NUMBER, 1, char[200])
#define BD_CREATE _IOW(MAGIC_NUMBER, 2, char[200])
#define BD_CHECK _IOW(MAGIC_NUMBER, 3, char[200])
#define BD_READ _IOR(MAGIC_NUMBER, 4, char[200])


static int fd;

void dma_poll(void);

/* To clear Buffer*/
void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}


/* Common function to write into axi-dma registers */
void dma_write(char *buf){
    int bytes_written;
    // Write to the device file
    bytes_written = write(fd, buf, strlen(buf));
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

    char data[100];
    if (pass==1) {
    snprintf(data, sizeof(data), "SCR: %s", buffer);
     dma_write(data);
	}
    else if(pass==2){
    snprintf(data, sizeof(data), "STL: %s", buffer);
     dma_write(data);
    dma_poll();
	}
    else if(pass==3){
    snprintf(data, sizeof(data), "DCR: %s", buffer);
    dma_write(data);   
	}
   else if(pass==4){
    snprintf(data, sizeof(data), "DTL: %s", buffer);
    dma_write(data);    
    dma_poll();
	}
   else if(pass==5){
    snprintf(data, sizeof(data), "DMARUN: %s", buffer);
    dma_write(data);    
	}
   else if(pass==6){
    snprintf(data, sizeof(data), "DMASTOP: %s", buffer);
    dma_write(data);    
	}
   else {
    snprintf(data, sizeof(data), "DMAERROR: %s", buffer);
    dma_write(data);        
	}
}

/* Dma Operations */
int dma_func(void){
	int pass;
	char buffer[100];
	int choice1;
	int choice2;
 	repeat2: 
  	while(1){
  	printf("Choose an operation:\n");
   	printf("1. To Write into DMA device\n");
    	printf("2. To Read the last written parameters from userspace\n");
   	printf("3. For main menu\n");
    	printf("Enter your choice: ");
        if (scanf("%d", &choice1) != 1) {  
        printf("Invalid input!\n");
        clear_stdin();  // Clears leftover input
        }	
        switch (choice1) {
         case 1:
	 repeat3:
    	  while(1){
  	   printf("Choose an DMA Operation:\n");
   	   printf("1. MM2S Current Buffer Descriptor\n");
           printf("2. MM2S Tail Buffer Descriptorn\n");
   	   printf("3. S2MM Current Buffer Descriptor\n");
   	   printf("4. S2MM Tail Buffer Descriptor\n");
   	   printf("5. DMA ON\n");
   	   printf("6. DMA OFF\n");
   	   printf("7. Error Checking\n");
   	   printf("8. To go back menu\n");
    	   printf("Enter your choice:\n ");
           if (scanf("%d", &choice2) != 1) {  
            printf("Invalid input!\n");
            clear_stdin(); 
           }	
	   printf("--Please make sure providing address are aligned--\n ");
	
	    switch (choice2) {
       		case 1:
		    printf("--Please write first buffer descriptor address into current buffer descriptor register(MM2S)-- \n");
    		    if (scanf("%s", buffer) != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin();  // Clears leftover input
   		     }	
		    pass=1;
		    dma_writing(buffer, pass);
		    break;
 	       case 2:
		    printf("--Please write last buffer descriptor address into tail buffer descriptor register(MM2S)--- \n");
    		    if (scanf("%s", buffer)  != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin();  
   		     }	
		    pass=2;
		    dma_writing(buffer, pass);
		    break;
	        case 3:
		    printf("--Please write first buffer descriptor address into current buffer descriptor register(S2MM)---\n ");
    		    if (scanf("%s", buffer)  != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin(); 
   		     }	
		    pass=3;
		    dma_writing(buffer, pass);
		    break;
		case 4:
		    printf("--Please write last buffer descriptor address into tail buffer descriptor register(S2MM)--- \n");
    		    if (scanf("%s", buffer)  != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin();  
   		     }	
		    pass=4;
		    dma_writing(buffer, pass);
		    break;
		case 5:
		    printf("--Please Write Channel Offset to ON-- \n");
    		    if (scanf("%s", buffer)  != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin();  
   		     }	
		    pass=5;
		    dma_writing(buffer, pass);
		    break;
		case 6:
		    printf("--Please Write Channel Offset to OFF--\n ");
    		    if (scanf("%s", buffer)  != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin();  
   		     }	
		    pass=6;
		    dma_writing(buffer, pass);
		    break;
		case 7:
		    printf("--Please Write Channel Offset to Check-- \n");
    		    if (scanf("%s", buffer)  != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin();  
   		     }	
		    pass=7;
		    dma_writing(buffer, pass);
		    break;
		case 8:
		    goto repeat2;
		default:
	            printf("Invalid choice.\n");
		    break;
		}
	    }
	   break;
        case 2:
            dma_read();
	   break;
	case 3:
	    return 0;
        default:
            printf("Invalid choice.\n");
    	}
    }
}

int common(int option){
    unsigned long phys_addr;
    unsigned long mem_size; 
    // Ask the user for the physical address and memory size
    printf("Enter the physical address (in hexadecimal format):\n");
    if (scanf("%li", &phys_addr) != 1) {  
    printf("Invalid input!\n");
    clear_stdin(); 
    }	
    printf("Enter the memory size (in bytes): \n");
    if (scanf("%li", &mem_size) != 1 || (mem_size == 0)) {  
    printf("Invalid input!\n");
    clear_stdin();  
    }	

    int fd1;
    void *mapped;
    off_t page_offset, aligned_addr;

    // Calculate page alignment
    page_offset = phys_addr % PAGE_SIZE;
    aligned_addr = phys_addr - page_offset;

    // Open /dev/mem
    fd1 = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd1 < 0) {
        perror("open");
        return -1;
    }

    // Map the physical memory into user space
    mapped = mmap(NULL, mem_size + page_offset, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, aligned_addr);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        close(fd1);
        return -1;
    }

    printf("Mapped address: %p\n", (char *)mapped + page_offset);

    // Access the memory
    volatile uint32_t *ptr = (volatile uint32_t *)((char *)mapped + page_offset);
    size_t num_entries = mem_size / sizeof(uint32_t);
    
    for (size_t i = 0; i < num_entries; i++) {
	if (option == 1){
        ptr[i] = (uint32_t)(0x00000000 + i); // Write unique value if called in writting context
	} 
        // Read back the value
        uint32_t value = ptr[i];
        printf("Address: %p, Read: 0x%X\n", (void *)(ptr + i), value);
    } 

       
    // Unmap memory
    if (munmap(mapped, mem_size + page_offset) < 0) {
        perror("munmap");
    }

    // Close /dev/mem
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
            printf("Transfer success in this channel and polled successfully!\n");
	} else {
            printf("Unexpected event occurred!\n");
	    return ;
        }
    }
    return ;
}

void dma_descriptor(char *buffer, int pass) {
    char data[300];
    if (pass == 1 || pass == 2) {
        snprintf(data, sizeof(data), "BDC: %s %d", buffer, pass);
        ioctl(fd, BD_CREATE, data); 	
	usleep(100000);//100 milliseconds
    } else if (pass == 3 || pass == 4) {
        snprintf(data, sizeof(data), "BDW: %s %d", buffer, pass);
        ioctl(fd, BD_WRITE, data); 
	usleep(100000);
    }else if (pass == 5 || pass == 6) {
        snprintf(data, sizeof(data), "BDN: %s %d", buffer, pass);
        ioctl(fd, BD_CHECK, data);
	usleep(100000);
    }  else {
        printf("Error! Invalid pass number \n");
        return;
    }
}

int buffer_descriptor(){
    int option, choice;
    unsigned long choice1;
    char buffer[600];
    unsigned long length;
    while (1) {
        printf("Choose an operation:\n");
        printf("1. To Create Buffer Descriptor\n");
        printf("2. To Write into buffer descriptor registers\n");
        printf("3. To Pass parameters for buffer descriptor reading\n");
        printf("4. To Read the buffer descriptor\n");
        printf("5. To go back\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input!\n");
            clear_stdin();
            continue;
        }

        switch (choice) {
            case 1:
                printf("--Please select channel for buffer descriptor creation--\n");
                printf("1. For MM2S Channel\n");
                printf("2. For S2MM Channel\n");

                if (scanf("%ld", &choice1) != 1) {
                    printf("Invalid input!\n");
                    clear_stdin();
                    continue;
                }

                switch (choice1) {
                    case 1:
                        printf("--Enter number of channel descriptors required for MM2S--\n");
                        if (scanf("%s", buffer) == 0) {
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }
                        option = 1;
                        dma_descriptor(buffer, option);
                        break;  // **FIXED: Added break**

                    case 2:
                        printf("--Enter number of channel descriptors required for S2MM--\n");
                        if (scanf("%s", buffer) == 0) {
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }
                        option = 2;
                        dma_descriptor(buffer, option);
                        break;  // **FIXED: Added break**

                    default:
                        printf("Invalid channel selection!\n");
                }
                break;
            case 2:
                printf("--Please select channel for buffer descriptor Writing--\n");
                printf("1. For MM2S Channel\n");
                printf("2. For S2MM Channel\n");

                if (scanf("%ld", &choice1) != 1) {
                    printf("Invalid input!\n");
                    clear_stdin();
                    continue;
                }

                switch (choice1) {
                    case 1:
                        printf("--Enter the required buffer descriptor number for writing (MM2S)--\n");
                        if (scanf("%d", &choice) != 1) {
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }

                        printf("--Enter the buffer address (MM2S)--\n");
                        if (scanf("%li", &choice1) != 1) {        
	                     printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }
                        printf("--Enter the buffer length (MM2S)--\n");
                        printf("-- Remember length should be in the format 0x0-(bits-xxXX)-XXXXXX --\n");
                        if (scanf("%li", &length) != 1) {  
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }
                        snprintf(buffer, sizeof(buffer), "%d 0x%lX 0x%lX", choice, choice1, length);
                        option = 3;
                        dma_descriptor(buffer, option); 
                        break;

                    case 2:
                        printf("--Enter the required buffer descriptor number for writing (S2MM)--\n");
                        if (scanf("%d", &choice) != 1) { 
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }

                        printf("--Enter the buffer address (S2MM)--\n");
                        if (scanf("%li", &choice1) != 1) {  
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }
                        printf("--Enter the buffer length (S2MM)--\n");
                        printf("-- Remember length should be in the format 0x0-(bits-xxXX)-XXXXXX --\n");
                        if (scanf("%li", &length) != 1) {  
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }
                        snprintf(buffer, sizeof(buffer), "%d 0x%lX 0x%lX", choice, choice1, length);
			option = 4;
                        dma_descriptor(buffer, option);  
                        break;

                    default:
                        printf("Invalid channel selection!\n");
                }
                break;
  	    case 3:
                printf("-- Select the channel for buffer descriptor reading --\n");
                printf("1. For MM2S Channel\n");
                printf("2. For S2MM Channel\n");

                if (scanf("%ld", &choice1) != 1) {
                    printf("Invalid input!\n");
                    clear_stdin();
                    continue;
                }

                switch (choice1) {
                    case 1:
                        printf("--Enter buffer descriptor number--\n");
                        if (scanf("%s", buffer) == 0) {
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }
                        option = 5;
                        dma_descriptor(buffer, option);
                        break;  
                    case 2:
                        printf("--Enter buffer descriptor number-\n");
                        if (scanf("%s", buffer) == 0) {
                            printf("Invalid input!\n");
                            clear_stdin();
                            continue;
                        }
                        option = 6;
                        dma_descriptor(buffer, option);
                        break;
                    default:
                        printf("Invalid channel selection!\n");
                }
                break;
	     case 4:
		memset(buffer, 0, sizeof(buffer));
                printf("-- Notice :- This should be used only after passing params for buffer descriptor reading --\n");
		ioctl(fd, BD_READ, buffer); 
		printf("Received from driver: %s\n", buffer);
		break;
            case 5:
                return 0;

            default:
                printf("Invalid choice. Try again.\n");
        }
    }
}

int main()
{   int option;
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
    printf("3. To DMA Sector\n");
    printf("4. To BUFFER-DESCRIPTORS sector\n");
    printf("5. To EXIT\n");
    printf("Enter your choice: \n");
    if (scanf("%d", &choice) != 1) {  
     printf("Invalid input!\n");
     clear_stdin();  // Clears leftover input
    }	

    switch (choice) {
        case 1:

	    ret = common(choice);
	    if(ret == 0){
		printf("Successfully written to DDR\n");	
            }else{
		printf("Writing Failed into DDR\n");
	    }	
            break;
	case 2:

	    ret = common(choice);
	    if(ret == 0){
		printf("Successfully readed from DDR\n");	
            }else{
		printf("Reading from DDR failed\n");
	    }	
            break;
        case 3:

            dma_func();
            break;
        case 4:

            buffer_descriptor();
            break;

	case 5:

	    printf("Thank you and BYE !\n");
            return 0;
	default:
            printf("Invalid choice.\n");
    }
  }
  close(fd);
  return 0;
}

