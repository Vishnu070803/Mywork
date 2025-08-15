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


#define PAGE_SIZE 4096
#define DEVICE_FILE "/dev/DMA_driver"



void dma_poll(void);


/* To clear Buffer*/
void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

/* Common function to write into axi-dma registers */
void dma_open(char *buf){
	
    int bytes_written;
    int fd = open(DEVICE_FILE, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open device file");
        exit(EXIT_FAILURE);
    }

    // Write to the device file
    bytes_written = write(fd, buf, strlen(buf));
    if (bytes_written < 0) {
        perror("Failed to write to device file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Wrote to device: %s\n", buf);

    close(fd);

}

/* Dma read function to read the last written parameters from userspace */
void dma_read(void)
{
    int fd;
    char buffer[4096];
    ssize_t bytes_read;

    fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        exit(EXIT_FAILURE);
    }

    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        perror("Failed to read from device file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    buffer[bytes_read] = '\0'; 
    printf("Data from device:\n%s\n", buffer);

    close(fd);
}


/* To choose a register for writing */
void dma_writing(char *buffer, int pass)
{    

    char data[100];
    if (pass==1) {
    snprintf(data, sizeof(data), "SA: %s", buffer);
     dma_open(data);
	}
    else if(pass==2){
    snprintf(data, sizeof(data), "SL: %s", buffer);
     dma_open(data);
    dma_poll();
	}
    else if(pass==3){
    snprintf(data, sizeof(data), "DA: %s", buffer);
    dma_open(data);   
	}
   else if(pass==4){
    snprintf(data, sizeof(data), "DL: %s", buffer);
    dma_open(data);    
    dma_poll();
	}
   else if(pass==5){
    snprintf(data, sizeof(data), "DR: %s", buffer);
    dma_open(data);    
	}
   else if(pass==6){
    snprintf(data, sizeof(data), "DS: %s", buffer);
    dma_open(data);    
	}
   else {
    snprintf(data, sizeof(data), "DE: %s", buffer);
    dma_open(data);        
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
    	printf("2. To Read from DMA device\n");
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
   	   printf("1. Source Address\n");
           printf("2. Source Length\n");
   	   printf("3. Destination Address\n");
   	   printf("4. Destination Length\n");
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
		    printf("--Please Write Source address-- \n");
    		    if (scanf("%s", buffer) != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin();  // Clears leftover input
   		     }	
		    pass=1;
		    dma_writing(buffer, pass);
		    break;
 	       case 2:
		    printf("--Please Write Source Length-- \n");
    		    if (scanf("%s", buffer)  != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin();  
   		     }	
		    pass=2;
		    dma_writing(buffer, pass);
		    break;
	        case 3:
		    printf("--Please Write Destination address--\n ");
    		    if (scanf("%s", buffer)  != 1) {  
  		      printf("Invalid input!\n");
  		      clear_stdin(); 
   		     }	
		    pass=3;
		    dma_writing(buffer, pass);
		    break;
		case 4:
		    printf("--Please Write Destination Length-- \n");
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
    size_t mem_size;

    // Ask the user for the physical address and memory size
    printf("Enter the physical address (in hex or decimal): ");
    if (scanf("%lx", &phys_addr) != 1) {  
    printf("Invalid input!\n");
    clear_stdin(); 
    }	
    printf("Enter the memory size (in bytes): ");
    if (scanf("%zu", &mem_size) != 1 || (mem_size == 0)) {  
    printf("Invalid input!\n");
    clear_stdin();  
    }	

    int fd;
    void *mapped;
    off_t page_offset, aligned_addr;

    // Calculate page alignment
    page_offset = phys_addr % PAGE_SIZE;
    aligned_addr = phys_addr - page_offset;

    // Open /dev/mem
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    // Map the physical memory into user space
    mapped = mmap(NULL, mem_size + page_offset, PROT_READ | PROT_WRITE, MAP_SHARED, fd, aligned_addr);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        close(fd);
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
    close(fd);

    return 0;

}


/* For polling to know the status of dma data transaction */
void dma_poll(void) {
    int fd = open(DEVICE_FILE, O_RDWR); 
    if (fd == -1) {
        perror("Failed to open device");
        return;
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN; 

    int ret = poll(&pfd, 1, 5000);
    if (ret == -1) {
        perror("poll() failed");
        close(fd);
        return;
    }

    msleep(10);
    if (ret == 0) {
        printf("Timeout occurred! No data ready within %d milliseconds.\n", ETIMEDOUT);
    } else {
        if (pfd.revents & POLLIN) {
            printf("Transfer success in this channel and polled successfully!\n");
	} else {
            printf("Unexpected event occurred!\n");
	    return ;
        }
    }
    close(fd);
    return ;
}



int main()
{   int option;
    int choice;
    int ret;
 repeat1: 
  while(1){
    printf("Choose an operation:\n");
    printf("1. To Write into Memory\n");
    printf("2. To Read from Memory\n");
    printf("3. To DMA Sector\n");
    printf("4. To EXIT\n");
    printf("Enter your choice: \n");
    if (scanf("%d", &choice) != 1) {  
     printf("Invalid input!\n");
     clear_stdin();  // Clears leftover input
    }	

    switch (choice) {
        case 1:
	    option = 1;
	    ret = common(option);
	    if(ret == 0){
		printf("Successfully written to DDR\n");	
            }else{
		printf("Writing Failed into DDR\n");
	    }	
            break;
	case 2:
	    option = 2;
	    ret = common(option);
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
	    printf("Thank you and BYE !\n");
            return 0;
	default:
            printf("Invalid choice. Exiting.\n");
    }
  }
  return 0;
}

