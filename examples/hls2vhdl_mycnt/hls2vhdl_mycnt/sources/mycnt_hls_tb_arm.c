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
 * 15/05/2017. jgarrigos.
 * This sw app controls a custom axi peripheral (mycnt_hls) designed in hls
 * that has 2 inputs and 2 outputs.
 * WARNING: MYCNT_HLS BEHAVIOR IS NOT THE SAME OF MYCNT PERIPHERAL
 * (DESIGNED DIRECTELY IN VHDL FOR OTHER EXAMPLE).
 */

#include <stdio.h>
#include "platform.h"

/*
 * Not needed. Not using xil_printf, nor print.
 */
//#include "xil_printf.h"

/*
 * include basic system definitions
 */
#include "xparameters.h"
// for this simple peripheral this is the only definition found in the file that we need:
//#define XPAR_MYCNT_HLS_0_DEVICE_ID 0

/*
 * Include mycnt_hls driver's functions.
 */
#include "xmycnt_hls.h"

/*
 * mycnt_hls_sw included here as in hls, to compare sw and hw results
 */
void mycnt_hls_sw(int din1, int din2, int *dout1, int *dout2){

	static int cnt = 0;

	*dout1 = din1 + din2;

	while(1){
		if (din1==0){
			cnt = 0;
			break;
		}
		else{
			if (cnt < din2){
				cnt = cnt + 1;
			}
			else{
				break;
			}
		}
	}

	*dout2 = cnt;
	return;
}

/*
 * this is the main routine
 */
int main()
{
	int dout1_sw, dout2_sw;
	int dout1_hw, dout2_hw;
	int i;

	XMycnt_hls mycnt_hls_ptr; // device pointer
	XMycnt_hls_Config *mycnt_hls_config_ptr; // device's config pointer
	int status;
	int j;

	/*
	 * hw platform initialization
	 */
    init_platform();

    /*
	 * print hello world to test if hw is alive (arm processor and PS ecosystem
	 * correctly configured, uart, etc.)
	 */
	printf("**************************************************************\n\r");
	printf("Hello World\n\r");
	printf("Calculating sum and differential count for ten inputs.\n\r");
	printf("**************************************************************\n\r");

	/*
	 * Look Up device configuration
	 */
	mycnt_hls_config_ptr = XMycnt_hls_LookupConfig(XPAR_MYCNT_HLS_0_DEVICE_ID);
	if (!mycnt_hls_config_ptr) {
	   	print("ERROR: Lookup of accelerator configuration failed.\n\r");
	   	return XST_FAILURE;
	}

	/*
	 * Initialize the device with the recovered config
	 */
	status = XMycnt_hls_CfgInitialize(&mycnt_hls_ptr, mycnt_hls_config_ptr);
	if (status != XST_SUCCESS) {
	   	print("ERROR: Could not initialize accelerator.\n\r");
	   	exit(-1);
	}


	for(i=0;i<10;i++){
		printf("Iteration #%d\r\n", i);

		/*
		 * calculate output from mycnt_hls implemented as sw
		 */
		mycnt_hls_sw(i, i*1000, &dout1_sw, &dout2_sw);
		printf("mycnt_hls_sw results: sum = %d\t cnt = %d\r\n", dout1_sw, dout2_sw);

		/*
		 * calculate output from mysum implemented as hw
		 */
		//mycnt_hls(i, i*1000, &dout1_hw, &dout2_hw);
		// Now this line from hls code is substituted by the following code
		// that sends data to the peripheral, waits for results available and read them

		/*
		 * send data to peripheral inputs
		 */
		XMycnt_hls_Set_din1(&mycnt_hls_ptr, i);
		XMycnt_hls_Set_din2(&mycnt_hls_ptr, i*1000);

		/*
		 * pulse peripheral ap_start
		 */
		XMycnt_hls_Start(&mycnt_hls_ptr);

		/*
		 * wait until corresponding output becomes available 
		 * (ap_vld=1, as hls adds by default ap_vld handshaking to outputs.)
		 * Or as in this case, just check the IsDone block flag.
		 */
		j=0;
		while(1){
			if(XMycnt_hls_IsDone(&mycnt_hls_ptr))
				break;
			else{
				j++; //just to check the number of wait cycles
				continue;
			}
		}
		printf("loops waiting hw response: %d\n\r",j);

		/*
		 * read peripheral output data
		 */
		dout1_hw = XMycnt_hls_Get_dout1(&mycnt_hls_ptr);
		dout2_hw = XMycnt_hls_Get_dout2(&mycnt_hls_ptr);
		printf("mycnt_hls_hw results: sum = %d\t cnt = %d\r\n", dout1_hw, dout2_hw);
		//Sleep(i*1000/10);
	}

	/*
	 * just to mimic the behavior of the tb we did in hls, 
	 * repet everything one more time, but sending 0 to din1 to reset hw
	 */
	printf("Reset: \r\n");

	mycnt_hls_sw(0, i*1000, &dout1_sw, &dout2_sw);
	printf("mycnt_hls_sw results: sum = %d\t cnt = %d\r\n", dout1_sw, dout2_sw);

	/*
	 * send data to peripheral inputs
	 */
	XMycnt_hls_Set_din1(&mycnt_hls_ptr, 0);
	XMycnt_hls_Set_din2(&mycnt_hls_ptr, i*1000);

	/*
	 * pulse peripheral ap_start
	 */
	XMycnt_hls_Start(&mycnt_hls_ptr);

	/*
	 * wait until corresponding output becomes available 
	 * (ap_vld=1, as hls adds by default ap_vld handshaking to outputs.)
	 * Or as in this case, just check the IsDone block flag.
	 */
	j=0;
	while(1){
		if(XMycnt_hls_IsDone(&mycnt_hls_ptr))
			break;
		else{
			j++; //just to check the number of wait cycles
			continue;
		}
	}
	printf("loops waiting hw response: %d\n\r",j);

	/*
	 * read peripheral output data
	 */
	dout1_hw = XMycnt_hls_Get_dout1(&mycnt_hls_ptr);
	dout2_hw = XMycnt_hls_Get_dout2(&mycnt_hls_ptr);
	//mycnt_hls(0, i*1000, &dout1_hw, &dout2_hw);
	printf("mycnt_hls_hw results: sum = %d\t cnt = %d\r\n", dout1_hw, dout2_hw);


    cleanup_platform();
    return 0;
}
