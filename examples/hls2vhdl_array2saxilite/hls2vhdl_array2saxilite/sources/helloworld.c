/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

/*
 * 02/06/2017. jgarrigos.
 * This sw app controls a custom axi peripheral designed in hls with 
 * arrays as function arguments (instead of pointers).  
 * This example shows how to communicate
 * with the device using high level interface functions provided
 * by the driver, but also the low level ones (XReorder_(Read/Write)Reg, commented out).
 *
 */
#include <stdio.h>
#include "platform.h"
//#include "xil_printf.h"

#include "xparameters.h" //This includes basic system definitions 
// that we will need, like these two:
//#define XPAR_REORDER_0_DEVICE_ID 0
//#define XPAR_REORDER_0_S_AXI_MYAXI_BASEADDR 0x43C00000
#include "xreorder.h" //Include device driver's functions,
//and also low_level functions in xreorder_hw.h, that will be useful, like these:
// myaxi
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read)
//        bit 7  - auto_restart (Read/Write)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x40 ~
// 0x7f : Memory 'dinarray' (10 * 32b)
//        Word n : bit [31:0] - dinarray[n]
// 0x80 ~
// 0xbf : Memory 'doutarray' (10 * 32b)
//        Word n : bit [31:0] - doutarray[n]
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)
//#define XREORDER_MYAXI_ADDR_AP_CTRL        0x00
//#define XREORDER_MYAXI_ADDR_GIE            0x04
//#define XREORDER_MYAXI_ADDR_IER            0x08
//#define XREORDER_MYAXI_ADDR_ISR            0x0c
//#define XREORDER_MYAXI_ADDR_DINARRAY_BASE  0x40
//#define XREORDER_MYAXI_ADDR_DINARRAY_HIGH  0x7f
//#define XREORDER_MYAXI_WIDTH_DINARRAY      32
//#define XREORDER_MYAXI_DEPTH_DINARRAY      10
//#define XREORDER_MYAXI_ADDR_DOUTARRAY_BASE 0x80
//#define XREORDER_MYAXI_ADDR_DOUTARRAY_HIGH 0xbf
//#define XREORDER_MYAXI_WIDTH_DOUTARRAY     32
//#define XREORDER_MYAXI_DEPTH_DOUTARRAY     10


/*
 * sw version of the device included here as in hls, to compare sw and hw results
 */
void reorder_sw (int dinarray[10], int doutarray[10]){

	int i = 0;

	for (i=0;i<10;i++){
		doutarray[9-i] = dinarray[i] + i;
	}

	return;

}
#define N 10
/*
 * This is our main function
 */
int main()
{	
	// define some datain arrays to test several examples:
//	int datain[N] = {0,0,0,0,0,0,0,0,0,0};
	int datain[N] = {1,1,1,1,1,1,1,1,1,1};
//	int datain[N] = {0,1,2,3,4,5,6,7,8,9};
//	int datain[N] = {0,2,4,6,8,10,11,12,13,14};
//	int datain[N] = {0,0,0,6,8,10,11,12,13,14};
	int datain_read[N];
	int dataout_sw[N];
	int dataout_hw[N];
	int i;

	XReorder_Config *Reorder_config_ptr; // device's config pointer
	XReorder Reorder_ptr; // device pointer
	int status;
	int j;

    init_platform();

    printf("\e[1;1H\e[2J"); //clean screen
    printf("*****************\n\r");
    printf("Hello World      \n\r");
    printf("*****************\n\r");

    /*
     * calculate output from the 'device' implemented as sw
     */
    reorder_sw(datain, dataout_sw);

    /*
     * calculate output from device implemented in hw
     * THE FOLLOWING LINE (FROM HLS) IS NO LONGER VALID AND MUST BE
     * SUBSTITUTED BY CODE SENDING/RECEIVING DATA TO/FROM THE HW PERIPHERAL
     */
    //reorder(datain, dataout_hw);

    /*
     * Look Up device's configuration
     */
    print("Look for device's configuration: \n\r");
    Reorder_config_ptr = XReorder_LookupConfig(XPAR_REORDER_0_DEVICE_ID);
    if (!Reorder_config_ptr) {
    	   	print("ERROR: Lookup of accelerator configuration failed.\n\r");
    	   	return XST_FAILURE;
    }
    print(">>>>>>Device configured.\n\r\n\r");

    /*
     * Initialize device with the recovered config
     */
    print("Initialize device: \n\r");
    status = XReorder_CfgInitialize(&Reorder_ptr, Reorder_config_ptr);
    if (status != XST_SUCCESS) {
       	print("ERROR: Could not initialize accelerator.\n\r");
       	exit(-1);
    }
    print(">>>>>>Device initialized.\n\r\n\r");

    /*
     * send data to peripheral inputs
     */
    printf("Send datain to device: \n\r");
    status = XReorder_Write_dinarray_Words(&Reorder_ptr,
    		0x00, datain, N);
    if (status != N) {
       	print("ERROR: Could not send data to accelerator.\n\r");
       	exit(-1);
    }
//    for (i=0;i<N;i++){
//    	XReorder_WriteReg(XPAR_REORDER_0_S_AXI_MYAXI_BASEADDR,
//    		(XREORDER_MYAXI_ADDR_DINARRAY_BASE + (i*sizeof(int))), datain[i]);
//    }
    printf(">>>>>>datain[i] sent to device.\n\r");

    /*
     * Read back datain from device, just to test data was received and stored
     */
    printf("Read datain back from device: \n\r");
    printf(">>>>>>datain[i] = ");
    status = XReorder_Read_dinarray_Words(&Reorder_ptr,
    			0x00, datain_read, N);
        if (status != N) {
           	print("ERROR: Could not read datain.\n\r");
           	exit(-1);
        }
    for(i=0;i<N;i++){
    	printf("%d,", datain_read[i]);
    }
//    for (i=0;i<N;i++){
//    	datain_read[i] = XReorder_ReadReg(XPAR_REORDER_0_S_AXI_MYAXI_BASEADDR,
//    			(XREORDER_MYAXI_ADDR_DINARRAY_BASE + (i*sizeof(int))) );
//    	printf("%d,", datain_read[i]);
//    }
    printf("\n\r\n\r");


    /*
     * pulse peripheral ap_start. To start device.
     */
    printf("Activate device_Start (TO COMPUTE THE OUTPUT).\n\r");
    XReorder_Start(&Reorder_ptr);

    /*
     * wait until corresponding output becomes available
     * (ap_vld=1, as hls adds by default ap_vld handshaking to outputs.)
     * Or as in this case, just check the IsDone block flag.
     */
    j=0;
    while(1){
    	if(XReorder_IsDone(&Reorder_ptr))
    	//if(j=12)
    		break;
    	else{
    		j++; //just to check the number of wait cycles
    		continue;
    	}
    }
    printf("Number of waiting loops until device IsDone: %d\n\r\n\r",j);

    /*
     * read device's output data
     */
    printf("Read dataout from device: \n\r");
    printf(">>>>>>dataout[i] = ");
    status = XReorder_Read_doutarray_Words(&Reorder_ptr,
    			0X00, dataout_hw, N);
    if (status != N) {
       	print("ERROR: Could not receive data from accelerator.\n\r");
       	exit(-1);
    }
    for(i=0;i<N;i++){
    	printf("%d,", dataout_hw[i]);
    }
//    for (i=0;i<N;i++){
//    	dataout_hw[i] = XReorder_ReadReg(XPAR_REORDER_0_S_AXI_MYAXI_BASEADDR,
//    					(XREORDER_MYAXI_ADDR_DOUTARRAY_BASE + (i*sizeof(int))) );
//       	printf("%d,", dataout_hw[i]);
//    }
    printf("\n\r\n\r");

    /*
     * compare sw and hw results and check correctness
     */
    printf("For datain = ");
    for(i=0;i<N;i++){
    	printf("%d,", datain[i]);
    }
    printf("\n\rOutput was:\n\r");
    printf("[i]    dataout_sw      dataout_hw\r\n");
    printf("----------------------------------\r\n");
    for (i=0;i<N;i++){
    	printf("%d         %d              %d     \r\n", i, dataout_sw[i], dataout_hw[i]);
    }

    cleanup_platform();
    return 0;
}
