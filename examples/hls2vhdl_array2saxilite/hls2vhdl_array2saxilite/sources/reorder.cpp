
#include <stdio.h>


void reorder(int dinarray[10], int doutarray[10]){
#pragma HLS INTERFACE s_axilite port=return bundle=myaxi
#pragma HLS INTERFACE s_axilite port=doutarray bundle=myaxi
#pragma HLS INTERFACE s_axilite port=dinarray bundle=myaxi

	int i = 0;

	for (i=0;i<10;i++){
		doutarray[9-i] = dinarray[i] + i;
	}

	return;

}
