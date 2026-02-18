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

#include <stdio.h>
#include "platform.h"
//#include "xil_printf.h"
#include "xparameters.h"
/* Definitions for peripheral AXI_BRAM_CTRL_0 */
/*
#define XPAR_AXI_BRAM_CTRL_0_DEVICE_ID 0
#define XPAR_AXI_BRAM_CTRL_0_DATA_WIDTH 32
#define XPAR_AXI_BRAM_CTRL_0_ECC 0
#define XPAR_AXI_BRAM_CTRL_0_FAULT_INJECT 0
#define XPAR_AXI_BRAM_CTRL_0_CE_FAILING_REGISTERS 0
#define XPAR_AXI_BRAM_CTRL_0_UE_FAILING_REGISTERS 0
#define XPAR_AXI_BRAM_CTRL_0_ECC_STATUS_REGISTERS 0
#define XPAR_AXI_BRAM_CTRL_0_CE_COUNTER_WIDTH 0
#define XPAR_AXI_BRAM_CTRL_0_ECC_ONOFF_REGISTER 0
#define XPAR_AXI_BRAM_CTRL_0_ECC_ONOFF_RESET_VALUE 0
#define XPAR_AXI_BRAM_CTRL_0_WRITE_ACCESS 0
#define XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR 0x40000000
#define XPAR_AXI_BRAM_CTRL_0_S_AXI_HIGHADDR 0x40001FFF
#define XPAR_AXI_BRAM_CTRL_0_S_AXI_CTRL_BASEADDR 0xFFFFFFFF
#define XPAR_AXI_BRAM_CTRL_0_S_AXI_CTRL_HIGHADDR 0xFFFFFFFF
*/
#include "xbram.h" //here there are the following definitions:
/*
	void XBram_WriteReg(u32 BaseAddress, u32 RegOffset, u32 Data)
	#define XBram_WriteReg(BaseAddress, RegOffset, Data) \
	XBram_Out32((BaseAddress) + (RegOffset), (u32)(Data))
	XBram_Out16
	XBram_Out8


	u32 XBram_ReadReg(u32 BaseAddress, u32 RegOffset)
	#define XBram_ReadReg(BaseAddress, RegOffset) \
	XBram_In32((BaseAddress) + (RegOffset))
	XBram_In16
	XBram_In8
*/
#include "xgpio.h"

#include <sleep.h>

#define ARM_SENT_DATA 0x00000000
//#define MB_CATCHED_DATA 0x00000004
#define MB_SENT_DATA 0x00000008
//#define ARM_CATCHED_DATA 0x0000000C

#define NDATA 100

int main()
{

	XBram Bram;	/* The Instance of the BRAM Driver */
	XBram_Config *ConfigPtr;
	XGpio sws;
	int Status;
	u32 sws_data;
	u32 RamData[NDATA];
	int i;
	char keypress;


    init_platform();

    printf("\e[3J");
    print("\n\nHello World\n\r");

    /*
     * Initialize Bram device
     */
    ConfigPtr = XBram_LookupConfig(XPAR_AXI_BRAM_CTRL_0_DEVICE_ID);
    if (ConfigPtr == (XBram_Config *) NULL) {
    	printf(">>>>>> Failure: Unable to find Bram config.\n");
    	return XST_FAILURE;
    }
    printf(">>>>>> Bram config found.\n");
    Status = XBram_CfgInitialize(&Bram, ConfigPtr,
    			     ConfigPtr->CtrlBaseAddress);
    if (Status != XST_SUCCESS) {
    	printf(">>>>>> Failure: Unable initialize Bram.\n");
    	return XST_FAILURE;
    }
    printf(">>>>>> Bram initialized.\n");

    /*
     * Initialize sws
     */
    Status = XGpio_Initialize(&sws,XPAR_AXI_GPIO_0_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        print(">>>>>> Failure: Unable initialize sws.\n");
        return XST_FAILURE;
    }
    printf(">>>>>> sws initialized.\n");

	/*
	 * set ARM_SENT_DATA to 0
	 */
	XBram_Out32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (ARM_SENT_DATA), 0);
	printf("ARM_SENT_DATA set to 0\n");



    while(1){
    	// Read sws values and store at Bram[16]
    	sws_data = XGpio_DiscreteRead(&sws, 1);
    	printf("sws = 0x%02x\n", (unsigned int)sws_data);
    	XBram_Out32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (16), sws_data);
    	sleep(1);


		// Send more data to bram and assert ARM_SENT_DATA
		for (i=20;i<(20+NDATA);i+=4){
			XBram_Out32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (i), i);
			//usleep(1000);
		}
		XBram_Out32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (ARM_SENT_DATA), 1);
		printf(">>>>>>>> ARM wrote data to bram and set ARM_SENT_DATA = 1.\n");

		// wait for MB catching data (wait MB resets ARM_SENT_DATA)
		while(XBram_In32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (ARM_SENT_DATA)) == 1){
			i++; //usleep(1000);
		}
		printf(">>>>>>>> MB catched data and set ARM_SENT_DATA = 0.\n");

		// wait for MB output data (wait MB asserts MB_SENT_DATA)
		while(XBram_In32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (MB_SENT_DATA)) == 0){
			i++; //usleep(1000);
		}
		printf(">>>>>>>> MB wrote data to bram and set MB_SENT_DATA = 1.\n");

		// read data from MB and reset MB_SENT_DATA
		for (i=20;i<(20+NDATA);i+=4){
			RamData[i/4] = XBram_In32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (i));
			//usleep(1000);
		}
		XBram_Out32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (MB_SENT_DATA), 0);
		printf(">>>>>>>> ARM catched data and set MB_SENT_DATA = 0.\n");

		// print input and output data
		printf("arm wrote       mb returned \n");
		for (i=20;i<NDATA;i+=4){
			printf("%d \t\t %d \n", i,(unsigned int)RamData[i/4]);
		}


		/*
		 * Promt for exit or continue
		 */
		printf("Continue y/n: ");
		scanf("%s", &keypress);
		printf("%c\n\r", keypress);
		if (keypress == 'n')
			break;

    }

    cleanup_platform();
    return 0;
}
