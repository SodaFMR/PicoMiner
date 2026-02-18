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
#include "xcomplex_sqr.h"
#include "xparameters.h"

void complex_sqr_sw(float REin, float IMin, float *REout, float *IMout){

// (a + bi)^2 = (a^2 - b^2) + (2ab)i

		*REout = REin * REin - IMin * IMin;
		*IMout = 2 * REin * IMin;


	return;

}



int main()
{

    // complex numbers stored in array as:
	float RE_array_in[10] = {0.0f,2.0f,4.4f,2.0f,-8.0f,10.0f,11.4f,12.0f,13.0f,-1.1f};
	float IM_array_in[10] = {0.0f,-3.0f,4.0f,6.0f,3.1f,10.0f,3.2f,12.0f,13.0f,-14.2f};
	float RE_array_out_sw[10];
	float IM_array_out_sw[10];
	float RE_array_out_hw[10];
	float IM_array_out_hw[10];
	u32 RE_tmp;
	u32 IM_tmp;
	int i;

	XComplex_sqr_Config *dev_config_ptr; // pointer to device's config
	XComplex_sqr dev_ptr; // pointer to device's instance
	int status;

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
    dev_config_ptr = XComplex_sqr_LookupConfig(XPAR_COMPLEX_SQR_0_DEVICE_ID);
    if (dev_config_ptr == (XComplex_sqr_Config *) NULL) {
        	printf("ERROR: Lookup of accelerator configuration failed.\n\r");
        	return 1;
    }
    printf(">>>>>>>>>Device configuration recovered. \n\r");

    /*
     * Initialize device with the recovered config
     */
    status = XComplex_sqr_CfgInitialize(&dev_ptr, dev_config_ptr);
    if (status != XST_SUCCESS) {
            printf("ERROR: Device initialization failed.\n\r");
            return 1;
    }
    printf(">>>>>>>>>Device initialization succeeded. \n\r");


    /*
     * Compute output from the 'device' implemented as sw
     */
	for (i=0;i<10;i++){
		complex_sqr_sw(RE_array_in[i], IM_array_in[i], &RE_array_out_sw[i], &IM_array_out_sw[i]);
	}

    /*
     * Assert device Start to begin computation
     */
    printf("Assert device Start to begin computation.\n\r");
    XComplex_sqr_Start(&dev_ptr);

    XComplex_sqr_EnableAutoRestart(&dev_ptr);

    /*
     * Compute output from the device implemented in hw.
     */
	for (i=0;i<10;i++){
		// Send data to device inputs
		XComplex_sqr_Set_REin(&dev_ptr, *((u32*)&RE_array_in[i]));
		XComplex_sqr_Set_IMin(&dev_ptr, *((u32*)&IM_array_in[i]));
		// for debugging purposes:
		// printf("Sent data %d: (%f + %fi)\n\r", i, RE_array_in[i], IM_array_in[i]);
		// Assert device Start to begin computation
		// XComplex_sqr_Start(&dev_ptr);
		// wait while the output is computed
		while(!XComplex_sqr_IsDone(&dev_ptr)){
		}
		// Read device output data
		RE_tmp = XComplex_sqr_Get_REout(&dev_ptr);
		IM_tmp = XComplex_sqr_Get_IMout(&dev_ptr);
		RE_array_out_hw[i] = *((float*)&RE_tmp);
		IM_array_out_hw[i] = *((float*)&IM_tmp);
		// for debugging purposes:
		// printf("Read dataout %d: (%f + %fi)\n\r", i, RE_array_out_hw[i], IM_array_out_hw[i]);
	}


	printf("RE_array_in[10] = {0.0,2.0,4.4,2.0,-8.0,10.0,11.4,12.0,13.0,-1.1};\r\n");
	printf("IM_array_in[10] = {0.0,-3.0,4.0,6.0,3.1,10.0,3.2,12.0,13.0,-14.2};\r\n");
	printf("\r\n");
	printf("[i]  RE_array_out_sw  IM_array_out_sw  RE_array_out_hw  IM_array_out_hw\r\n");
	printf("-------------------------------------------------------------------------------\r\n");
	// for printf column formatting check:
	// check https://www.eecis.udel.edu/~sprenkle/cisc105/making_columns.html
	for (i=0;i<10;i++){
		printf("%d \t %8.3f \t %8.3f \t %8.3f \t %8.3f \r\n",
		 i, RE_array_out_sw[i], IM_array_out_sw[i], RE_array_out_hw[i], IM_array_out_hw[i]);
	}

    cleanup_platform();
    return 0;
}
