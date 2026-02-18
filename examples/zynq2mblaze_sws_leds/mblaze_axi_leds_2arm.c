/*
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 *
 *
 * This file is a generated sample test application.
 *
 * This application is intended to test and/or illustrate some
 * functionality of your system.  The contents of this file may
 * vary depending on the IP in your system and may use existing
 * IP driver functions.  These drivers will be generated in your
 * SDK application project when you run the "Generate Libraries" menu item.
 *
 */

#include <stdio.h>
#include "xparameters.h" //here there are the following definitions:
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
//#include "xil_cache.h"
#include "xgpio.h"
//#include "gpio_header.h"
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


#define ARM_SENT_DATA 0x00000000
//#define MB_CATCHED_DATA 0x00000004
#define MB_SENT_DATA 0x00000008
//#define ARM_CATCHED_DATA 0x0000000C

#define NDATA 100


int main()
{
	/*
   Xil_ICacheEnable();
   Xil_DCacheEnable();


   {
      int status;

      status = GpioOutputExample(XPAR_AXI_GPIO_1_DEVICE_ID,8);
   }
   Xil_DCacheDisable();
   Xil_ICacheDisable();

   */
	XBram Bram;	/* The Instance of the BRAM Driver */
	XBram_Config *ConfigPtr;
	XGpio leds;
	int Status;
	u32 sws_data;
	u32 RamData[NDATA];
	int i;


	// Initialize Bram
    ConfigPtr = XBram_LookupConfig(XPAR_AXI_BRAM_CTRL_0_DEVICE_ID);
    if (ConfigPtr == (XBram_Config *) NULL) {
    	return XST_FAILURE;
    }
    Status = XBram_CfgInitialize(&Bram, ConfigPtr,
    			     ConfigPtr->CtrlBaseAddress);
    if (Status != XST_SUCCESS) {
    	return XST_FAILURE;
    }

    // Initialize leds
	Status = XGpio_Initialize(&leds,XPAR_AXI_GPIO_1_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}



	while(1){
		// Read sws values from Bram[16] and send to de leds
		sws_data = XBram_In32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (16));
	   	XGpio_DiscreteWrite(&leds, 1, sws_data);
	   	//MB_Sleep(1000);


    	// wait for ARM sending new data (wait ARM asserts ARM_SENT_DATA)
    	while (XBram_In32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (ARM_SENT_DATA)) == 0){
    		i=0; //MB_Sleep(1000);
    	}


    	// read data from ARM and reset ARM_SENT_DATA
    	for (i=20;i<(20+NDATA);i+=4){
    		RamData[i/4] = XBram_In32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (i));
    		//MB_Sleep(1000);
    	}
    	XBram_Out32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (ARM_SENT_DATA), 0);


    	// compute dataout and assert MB_SENT_DATA
    	for (i=20;i<(20+NDATA);i+=4){
    		XBram_Out32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (i), RamData[i/4] + 2);
    		//MB_Sleep(1000);
    	}
    	XBram_Out32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (MB_SENT_DATA), 1);


    	//wait for ARM catching data (wait ARM resets MB_SENT_DATA)
    	while (XBram_In32((XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR) + (MB_SENT_DATA)) == 0){
    	    i=0; //MB_Sleep(1000);
    	}

	}


	return 0;

}
