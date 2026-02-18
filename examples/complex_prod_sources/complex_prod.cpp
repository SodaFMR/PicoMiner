
#include <stdio.h>


void complex_prod(float din1array[10], float din2array[10], float doutarray[10]){
#pragma HLS INTERFACE s_axilite port=return bundle=myaxi
#pragma HLS INTERFACE s_axilite port=doutarray bundle=myaxi
#pragma HLS INTERFACE s_axilite port=din1array bundle=myaxi
#pragma HLS INTERFACE s_axilite port=din2array bundle=myaxi

	int i = 0;

	for (i=0;i<10;i+=2){
		doutarray[i] = din1array[i] * din2array[i] - din1array[i+1] * din2array[i+1];
		doutarray[i+1] = din1array[i+1] * din2array[i] + din1array[i] * din2array[i+1];
	}

	return;

}
