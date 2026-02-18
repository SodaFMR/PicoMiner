// 29/04/2020. jgarrigos.
// this example shows how to use array arguments for the hw function,
// instead of pointers, through an saxi_lite interface.
//
#include <stdio.h>

void complex_sqr_sw(float REin, float IMin, float *REout, float *IMout){

// (a + bi)^2 = (a^2 - b^2) + (2ab)i

		*REout = REin * REin - IMin * IMin;
		*IMout = 2 * REin * IMin;


	return;

}

void complex_sqr(float REin, float IMin, float *REout, float *IMout);

int main(){
// complex numbers stored in array as:


	float RE_array_in[10] = {0.0,2.0,4.4,2.0,-8.0,10.0,11.4,12.0,13.0,-1.1};
	float IM_array_in[10] = {0.0,-3.0,4.0,6.0,3.1,10.0,3.2,12.0,13.0,-14.2};
	float RE_array_out_sw[10];
	float IM_array_out_sw[10];
	float RE_array_out_hw[10];
	float IM_array_out_hw[10];
	int i;

	for (i=0;i<10;i++){
		complex_sqr_sw(RE_array_in[i], IM_array_in[i], &RE_array_out_sw[i], &IM_array_out_sw[i]);
		complex_sqr(RE_array_in[i], IM_array_in[i], &RE_array_out_hw[i], &IM_array_out_hw[i]);
	}

	printf("RE_array_in[10] = {0.0,2.0,4.4,2.0,-8.0,10.0,11.4,12.0,13.0,-1.1};\r\n");
	printf("IM_array_in[10] = {0.0,-3.0,4.0,6.0,3.1,10.0,3.2,12.0,13.0,-14.2};\r\n");
	printf("\r\n");
	printf("[i]  RE_array_out_sw  IM_array_out_sw  RE_array_out_hw  IM_array_out_hw\r\n");
	printf("-------------------------------------------------------------------------------\r\n");

	for (i=0;i<10;i++){
		printf("%d \t %f \t %f \t %f \t %f \r\n",
         i, RE_array_out_sw[i], IM_array_out_sw[i], RE_array_out_hw[i], IM_array_out_hw[i]);
	}

	return 0;

}
