/* Copyright 2019 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */
/*
/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>

#include "perf_mon_functions.h"

#include "flc_reg_rw.h"
#include "flc_dma_to_device.c"
#include "flc_dma_from_device.c"

#define FLC1_ENABLED 1

#define SIZE 0x2000000
#define DES_BUF_ADDRESS_REG_OFFSET 0x8

//
#define DES_BUF_ADDRESS_HIGH_REG_OFFSET 0xC 
#define BUF_ADDRESS_HIGH 0x00000001 
//
#define DES_CONTROL_REG_OFFSET 0x18 
#define DES_NEXT_DES_REG_OFFSET 0x00000000 
#define DES_NEXT_DES_MSB_REG_OFFSET 0x00000004 
#define INIT_BUF_ADDRESS 0x80000000 
#define DES_SIZE 0x00000080 
#define SYS_RSTN 0x80008
FILE *outputFile = NULL; 
uint64_t file_ptr = 0;
int miss_count = 0;
uint64_t ddr_hit_txns = 0;
int wr_count = 0;
int output_wr_count = 1;
#define  DDR_BASE  0x140000000

//FLC addresses stored in DDR
#define WRITE_DMA_DATA_BASE 0x140000000ULL
#define WRITE_DMA_DATA_BASE_LOW ((WRITE_DMA_DATA_BASE) & 0xFFFFFFFFUL)
#define WRITE_DMA_DATA_BASE_HI  ((WRITE_DMA_DATA_BASE >> 32) & 0xFFFFFFFFUL)

//DMA Params
#define DMA_DESCRIPTOR_MAX_TRANSFER_SIZE 0x2000000 //32Mb
#define DMA_DESCRIPTOR_SIZE 0x80

//DMA registers
#define S2MM_DMA_CONTROL_OFFSET 		0x40030
#define S2MM_CURRENT_DES_POINTER_OFFSET 	0x40038
#define S2MM_TAIL_DES_POINTER_OFFSET    	0x40040
void miss_mapper(FILE *trace_file,char *missed_txns_bin)
{
        int fdw,size;
        FILE *fdr,*wr_file,*rd_file,*rw_file,*miss_file;
        uint64_t pkt,access_size,access_type_tmp,access_type,address_offset;
        fdr = trace_file;
        wr_file = fopen("txns_file.txt","a");
	miss_file= fopen(missed_txns_bin, "rb");
        if(fdr==NULL)
        {
                printf("\nCould not open the file");
                exit(1);
        }
        if(wr_file==NULL)
        {
                printf("\nCould not open the file");
                exit(1);
        }
        if(miss_file==NULL)
        {
                printf("\nCould not open the file");
                exit(1);
        }
	uint64_t miss_number = 0;
	uint64_t serial_number = 0;
	uint32_t hit_count = 0;
	printf("The file pointer value %d\n",file_ptr);
        while(fread(&serial_number,sizeof(uint64_t),1,miss_file) != 0)
	{
	 	miss_number = serial_number & 0x7fffffffffffffff;
		//printf("mis_number :  %d\n",miss_number);
		hit_count = 0;
		while(file_ptr < (miss_number) && file_ptr != miss_number)
        	{
			if(fread(&pkt,sizeof(pkt), 1, fdr) == 0)break;
			++file_ptr;
                	address_offset = (pkt >> 16 ) & 0xFFFFFFFF;
                	if(hit_count < 100){
				fprintf(wr_file,"0x%08lx,0x%08lx,y,n\n",file_ptr,address_offset);
				wr_count ++;
			}
			hit_count++;
		}
		if(fread(&pkt,sizeof(pkt), 1, fdr) == 0)break;
		++file_ptr;
                address_offset = (pkt >> 16 ) & 0xFFFFFFFF;
		if(serial_number & 0x8000000000000000)
		{
                	fprintf(wr_file,"0x%08lx,0x%08lx,n,n\n",file_ptr,address_offset);
				wr_count ++;
		}
		else
		{
                	fprintf(wr_file,"0x%08lx,0x%08lx,n,y\n",file_ptr,address_offset);
			wr_count ++;
			ddr_hit_txns ++;
		}
	}
        fflush(wr_file);
        fclose(wr_file);
        fclose(miss_file);
}
#if 0
uint32_t fill_write_descriptors_and_start_dma(uint64_t transfer_size) 
{
	printf("Started filling write descriptors for transfer size %ld\n",transfer_size);
	uint64_t current_data_address = WRITE_DMA_DATA_BASE;
	uint64_t current_descriptor_base = 0x10000;
	uint64_t remaining_transfer_size = transfer_size;
	uint32_t current_transfer_size = DMA_DESCRIPTOR_MAX_TRANSFER_SIZE;
	
	do	
	{
		if(remaining_transfer_size < (uint64_t) DMA_DESCRIPTOR_MAX_TRANSFER_SIZE) current_transfer_size = (uint32_t) remaining_transfer_size;
		else current_transfer_size = DMA_DESCRIPTOR_MAX_TRANSFER_SIZE-1;

		writel(current_descriptor_base+DMA_DESCRIPTOR_SIZE,current_descriptor_base);
		writel(0x0,current_descriptor_base+0x04);
		writel(current_data_address & 0xFFFFFFFFUL,current_descriptor_base+0x08);
		writel((current_data_address >> 32) & 0xFFFFFFFFUL,current_descriptor_base+0x0c);
		writel(current_transfer_size,current_descriptor_base+0x18);

		remaining_transfer_size -= current_transfer_size;
		current_descriptor_base += DMA_DESCRIPTOR_SIZE;
		current

	}while(remaining_transfer_size != 0);

	uint32_t s2mm_tail_des_pointer_val = ((uint32_t)current_descriptor_) - DMA_DESCRIPTOR_SIZE;	
	//Initializing the DMA registers
	writel(0x4,MM2S_DMA_CONTROL_OFFSET); //Reset DMA
	writel(0x0,MM2S_CURRENT_DES_POINTER_OFFSET); //Initial BD offset
	writel(0x00011013,MM2S_DMA_CONTROL_OFFSET);
	writel(0x00000000,0x00050014);
	writel(0x00000000,0x0005000C);
	writel(mm2s_tail_des_pointer_val,MM2S_TAIL_DES_POINTER_OFFSET);
	printf("Completed filling read descriptors\n");
	return mm2s_tail_des_pointer_val;
}
#endif
int main(int argc,char* argv[])
{
	char enter;
//    writel(0x0,SYS_RSTN);
//    sleep(1);
//    writel(0x1,SYS_RSTN);
//    sleep(1);
//    
	printf("Reset System Complete!\n");

	printf("Please configure FLC using the Freedom Studio APP\n");
	printf("After configuring press Enter\n");
	scanf("%c",&enter);

	FILE* wr_file = fopen("txns_file.txt","w");
	fclose(wr_file);

	int res = system("sh axi_dma.sh");

	int i=0, rc = 0;
	
     	perf_mon_init();
	writel(0x01,0x80000);
	sleep(1);
	writel(0x00,0x80000);

// memory 0x1800000000 configuration 0x50000 buffer descriptors space 0x00000
// memory 0x1400000000 configuration 0x40000 buffer descriptors space 0x10000
	uint32_t cmdType, ret = 0;
	uint64_t addr, len;
	uint64_t transfer_count = 1;
	uint64_t fsize=0, j = 0, div = 1;
	int fdw,size;
	uint64_t access_size = SIZE;
	uint64_t address_offset = 0x180000000;
	uint64_t written_address_offset = 0x140000000;
	uint64_t written_data_bytes = 0;
	uint64_t comp_written_data_bytes = 0;
	int num_files = getopt_integer(argv[1]);
	printf("\nThe Number of files is %d\n",num_files);
	FILE *fdr[num_files];
	FILE *fdr1;
	uint32_t next_des_reg = DES_NEXT_DES_REG_OFFSET;
	uint32_t next_des_val = DES_NEXT_DES_REG_OFFSET + DES_SIZE;
	uint32_t next_des_msb_reg = DES_NEXT_DES_MSB_REG_OFFSET;
	uint32_t next_des_msb_val = 0x00000000;
	uint32_t buf_address_val = INIT_BUF_ADDRESS;
	uint32_t buf_address_reg = DES_BUF_ADDRESS_REG_OFFSET;
	uint32_t buf_address_high_val = BUF_ADDRESS_HIGH;
	uint32_t buf_address_high_reg = DES_BUF_ADDRESS_HIGH_REG_OFFSET;
	uint32_t control_val = SIZE-1;
	uint32_t control_reg = DES_CONTROL_REG_OFFSET;
	int k=0;
	uint32_t mm2s_dma_control_reg = 0x50000;
	uint32_t mm2s_current_des_pointer_reg = 0x50008;
	uint32_t mm2s_tail_des_pointer_reg = 0x50010;
	uint32_t mm2s_dma_control_val = 0x00000004;
	uint32_t mm2s_current_des_pointer_val = 0x00000000;
	uint32_t mm2s_tail_des_pointer_val;
	//Determining the number of descriptors required
	int des = 0;
	uint32_t rem = 0;
	uint32_t transactions = 0;
	uint32_t val0,val1,val2,val3,val4,val5,des_val,prog,seconds = 0;
	uint32_t total_txns,hit_txns,ipm_miss_txns,ddr_miss_txns=0;
	float percent = 0;
	uint32_t value_1 = readl(0x10000);
	uint32_t value_2 = readl(0x1001C);
	for(int file=0;file<num_files;file++)
	{
		address_offset = 0x180000000;
		val0,val1,val2,val3,val4,val5,des_val,prog = 0;
		total_txns,hit_txns,ipm_miss_txns,ddr_hit_txns,ddr_miss_txns=0;
		printf("Reading trace file %s\n", argv[2+file]);
		fdr1 = fopen(argv[2+file],"rb");	
		if(fdr1 == NULL)printf("Error in opening trace file");
		else printf("Trace file opened %d\n", fdr1);
		fseek(fdr1, 0L, SEEK_END);
		fsize = ftell(fdr1);
		printf("File size is : 0x%x\n", fsize);
		des = fsize/SIZE;
		rem = fsize%SIZE;
		transactions = fsize/8;
		printf("remiander  : 0x%x\n", rem);
		printf("transactions  : 0x%x\n", transactions);
		des = des+1;
		printf("\nNo of descriptors needed for file %d : 0x%x\n",file+1, des);
		fseek(fdr1, 0L, SEEK_SET);

		//Writing data to the DDR 
		printf("\nDumping data of file %d to Memory started\n",file+1);	
		for(int j = 0;j<des;j++)
		{
			if(j==0)
			{
				address_offset = (address_offset);
			}
			else
			{
				address_offset = ((address_offset) + (access_size));
			}
			if(j==(des-1))
			{
				dump_data("/dev/xdma0_h2c_0" , address_offset,0x00,rem,0x00 , transfer_count, argv[2+file] , NULL);
				//printf("\nThe address offset 0x%16lx",address_offset);
			}
			else
			{
			        dump_data("/dev/xdma0_h2c_0" , address_offset,0x00,access_size,0x00 , transfer_count, argv[2+file] , NULL);
				//printf("\nThe address offset 0x%16lx",address_offset);
			}
		}
	
		printf("\nDumping data of file %d to Memory completed",file+1);	

		//populating the descriptor registers 
			
		for(k;k<des;k++)
		{
			if(k==0)
			{
				next_des_val = next_des_val;
			}
			else
			{
				next_des_val = ((k+1)*DES_SIZE)+DES_NEXT_DES_REG_OFFSET;
			}
			buf_address_val = (k*SIZE)+INIT_BUF_ADDRESS;
			buf_address_reg = next_des_reg + DES_BUF_ADDRESS_REG_OFFSET;
			buf_address_high_reg = next_des_reg + DES_BUF_ADDRESS_HIGH_REG_OFFSET;
			control_reg = next_des_reg + DES_CONTROL_REG_OFFSET;
			next_des_msb_reg = next_des_reg + DES_NEXT_DES_MSB_REG_OFFSET;

			if(k==(des-1))
			{
			}
			else
			{
			}
			writel(next_des_val,next_des_reg);
			writel(next_des_msb_val,next_des_msb_reg);
			writel(buf_address_val,buf_address_reg);
			writel(buf_address_high_val,buf_address_high_reg);
			if(k==(des-1))
			{
				writel(rem,control_reg);
			}
			else
			{
				writel(control_val,control_reg);
			}


			if(k==(des-1))
			{
				next_des_reg = next_des_reg;
			}
			else
			{
				next_des_reg = next_des_val;
			}
			printf("\n");
		}
		
		mm2s_tail_des_pointer_val = next_des_reg;	
		//Initializing the DMA registers
		mm2s_dma_control_val = 0x00000004; //need to review the value
		writel(mm2s_dma_control_val,mm2s_dma_control_reg);
		writel(mm2s_current_des_pointer_val,mm2s_current_des_pointer_reg);
		mm2s_dma_control_val = 0x00011013; //need to review the value
		writel(mm2s_dma_control_val,mm2s_dma_control_reg);
		writel(0x00000000,0x0005000C);
		writel(mm2s_tail_des_pointer_val,mm2s_tail_des_pointer_reg);
		writel(0x00000000,0x00050014);
	        
		// set ddr_ptr = trans_info ptr
		uint64_t ddr_ptr = DDR_BASE;
		miss_count = 0;

        	fseek(fdr1, 0L, SEEK_SET);

		wr_count = 0;
	
		outputFile = fopen("output_file.txt", "w");
		if (outputFile == NULL) {
		    fprintf(stderr, "Failed to open output file\n");
		    return 1;
		}
		while((readl(0x50004) & 0x2) == 0)
		{
			des_val = readl(0x50008);
			prog = (mm2s_tail_des_pointer_val-des_val)/128;
			prog = des-prog;
			get_rd_perf_mon(&val0,&val1, &val2,&val3,&val4, &val5);
			total_txns = val0 + val1;
			hit_txns = val2 + val3;
		        //ddr_hit_txns = val4 + val5;
			ipm_miss_txns = total_txns - hit_txns;
			printf("trace : %.2f\n",((float)prog/des)*100);
			sleep(1);
			//printf("miss count :  %d\n",miss_count);
			//printf("ipm miss tranactions count :  %d\n",ipm_miss_txns);
			if(miss_count < ipm_miss_txns)
			{
				recieve_data("/dev/xdma0_c2h_0" , (DDR_BASE + miss_count*8),0x00,(ipm_miss_txns-miss_count)*8,0x00 , 0x1, "missed_txns_file.bin");
        			miss_mapper(fdr1,"missed_txns_file.bin");
				miss_count = ipm_miss_txns; // ipm_miss_txns-miss_count > 1000 ? miss_count + 1000 : ipm_miss_txns;
				sleep(5);
 			}
			//printf("ddr hit tranactions count :  %ld\n",ddr_hit_txns);
		        ddr_miss_txns = ipm_miss_txns - ddr_hit_txns;
			/*
			if(wr_count/(output_wr_count*4096) > 0) {
				fprintf(outputFile,"0x%08x,0x%08x,0x%08x,%.4f,0x%08x,0x%08x,%.4f\n",total_txns,hit_txns,ipm_miss_txns,((float)hit_txns/total_txns)*100,ddr_hit_txns,ddr_miss_txns,((float)ddr_hit_txns/total_txns)*100);
				output_wr_count += (wr_count/(output_wr_count*4096)) ;
			}
			fflush(outputFile);
			*/
			
		}
		//perf_mon_dump(file,ddr_hit_txns);
	        fclose(outputFile);
		printf("\n");
		fclose(fdr1);
	}
	printf("Please copy the txns_file.txt for gui to process furthur \n");

	uint32_t read_size = ipm_miss_txns * 8;
	recieve_data("/dev/xdma0_c2h_0" , 0x140000000,0x00,read_size ,0x00 , 0x1, "final_missed_txns_file.bin");
		
}
