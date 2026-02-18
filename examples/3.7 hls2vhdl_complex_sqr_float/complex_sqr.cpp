
#include <stdio.h>


void complex_sqr(float REin, float IMin, float *REout, float *IMout){
#pragma HLS INTERFACE s_axilite port=return bundle=myaxi
#pragma HLS INTERFACE s_axilite port=REin bundle=myaxi
#pragma HLS INTERFACE s_axilite port=IMin bundle=myaxi
#pragma HLS INTERFACE s_axilite port=REout bundle=myaxi
#pragma HLS INTERFACE s_axilite port=IMout bundle=myaxi

// (a + bi)^2 = (a^2 - b^2) + (2ab)i

		*REout = REin * REin - IMin * IMin;
		*IMout = 2 * REin * IMin;


	return;

}
