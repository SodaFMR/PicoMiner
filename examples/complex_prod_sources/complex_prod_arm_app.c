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
 * 20/03/2019. jgarrigos.
 * This sw app controls a custom axi peripheral designed in hls with
 * floating point arrays as function arguments (instead of pointers).
 * This example shows how to communicate
 * with the device using high level interface functions provided
 * by the sw driver
 *
 */  
#include <stdio.h>
#include <stdlib.h> //for exit()
#include "platform.h"
//#include "xil_printf.h"
#include "xparameters.h" //this includes basic system definitions
#include "xcomplex_prod.h" //include device driver's functions
//#include <time.h> // does not work in ZYNQ-ARM, instead use:
#include "xtime_l.h" // for time measuring

/*
 * Declare device algorithm as sw. This is optional, but serve to compare sw and hw results
 */
void complex_prod_sw(float din1array[10], float din2array[10], float doutarray[10]){

	int i = 0;

	for (i=0;i<10;i+=2){
		doutarray[i] = din1array[i] * din2array[i] - din1array[i+1] * din2array[i+1];
		doutarray[i+1] = din1array[i+1] * din2array[i] + din1array[i] * din2array[i+1];
	}
	return;
}

#define N 10



/*
 * Our main function
 */
int main()
{

// complex numbers stored in array as:
// x1
// y1
// x2
// y2
// ...
// xn
// yn

	// define datain and dataout arrays
	float datain1[N] = {0.3,2.0,4.0,6.0,8.0,10.0,11.0,12.0,13.0,14.0};
	float datain2[N] = {0.1,2.0,4.0,6.0,8.0,10.0,11.0,12.0,13.0,14.0};
	float dataout_sw[N];
	float dataout_hw[N];
	int i;
	int j;

	XComplex_prod_Config *dev_config_ptr; // pointer to device's config
	XComplex_prod dev_ptr; // pointer to device's instance
	int status;
	XTime tStart, tEnd;

	/*
	 * Initialize processor system
	 */
    init_platform();

    /*
     * print hello to test everything is up and running
     */
	printf("\e[1;1H\e[2J"); //clean screen
    printf("Hello World\n\r");

	/*
     * Look up device's configuration
     */
    printf("Look for device's configuration: \n\r");
    dev_config_ptr = XComplex_prod_LookupConfig(XPAR_COMPLEX_PROD_0_DEVICE_ID);
    if (dev_config_ptr == (XComplex_prod_Config *) NULL) {
        	printf("ERROR: Lookup of accelerator configuration failed.\n\r");
        	exit(-1);
    }
    printf(">>>>>>>>>Device configuration recovered. \n\r");

    /*
     * Initialize device with the recovered config
     */
    status = XComplex_prod_CfgInitialize(&dev_ptr, dev_config_ptr);
    if (status != XST_SUCCESS) {
            	printf("ERROR: Device initialization failed.\n\r");
            	exit(-1);
    }
    printf(">>>>>>>>>Device initialization succeeded. \n\r");

    /*
     * Compute output from the 'device' implemented as sw
     */
    XTime_GetTime(&tStart);
    complex_prod_sw(datain1, datain2, dataout_sw);
    XTime_GetTime(&tEnd);
    printf("Output took %llu clock cycles.\n\r", (tEnd - tStart));
    printf("Output took %.2f us.\n\r", 1.0 * (tEnd - tStart) / (COUNTS_PER_SECOND/1000000));

    /*
     * Compute output from the device implemented in hw.
     * THE FOLLOWING LINE (FROM HLS) IS NO LONGER VALID AND MUST BE
     * SUBSTITUTED BY CODE SENDING/RECEIVING DATA TO/FROM THE HW PERIPHERAL
     */
    //complex_prod(datain1, datain2, dataout_sw);

    /*
     * Send data to device inputs
     */
    printf("Send datain1 and datain2 to device.\n\r");
    status = XComplex_prod_Write_din1array_Words(&dev_ptr, 0x00, (int *)datain1, N);
    status = XComplex_prod_Write_din2array_Words(&dev_ptr, 0x00, (int *)datain2, N);
    if (status != N) {
        printf("ERROR: could not send datain to device.\n\r");
        exit(-1);
    }
    printf(">>>>>>datain1[i] and datain2[i] sent to device.\n\r");

    /*
     * Assert device Start to begin computation
     */
    printf("Assert device Start to begin computation.\n\r");
    XComplex_prod_Start(&dev_ptr);

    /*
     * Wait until corresponding output becomes available
     */
    printf("Waiting for device's output.\n\r");
    j=0;

    XTime_GetTime(&tStart);
    while(1){
     	if(XComplex_prod_IsDone(&dev_ptr)){
     		XTime_GetTime(&tEnd);
     		printf("Output took %llu clock cycles.\n\r", (tEnd - tStart));
     		printf("Output took %.2f us.\n\r", 1.0 * (tEnd - tStart) / (COUNTS_PER_SECOND/1000000));
     		break;
     	}
//     	else{
//     		j++; //just to check the number of wait cycles
//     		continue;
//     	}
    }

    printf("Number of waiting loops until device IsDone: %d\n\r\n\r",j);

    /*
     * Read device output data
     */
    printf("Read dataout from device: \n\r");
    status = XComplex_prod_Read_doutarray_Words(&dev_ptr, 0x00, (int *)dataout_hw, N);
    if (status != N) {
        print f("ERROR: could not recover device's output data.\n\r");
            exit(-1);
    }

    /*
     * Compare sw and hw results and check correctness
     */
    printf("\n\rOutput was:\n\r");
    printf("[i]    dataout_sw      dataout_hw\r\n");
    printf("----------------------------------\r\n");
    for (i=0;i<N;i++){
      	printf("%d         %f              %f     \r\n", i, dataout_sw[i], dataout_hw[i]);
    }


    /*
     * Ending. Clean garbage and take a beer
     */
    cleanup_platform();
    return 0;
}
