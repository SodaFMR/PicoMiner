/*
 *
 */
void vector_operator(int din1[100], int din2[100], int sum[100], int *dot){
#pragma HLS INTERFACE s_axilite port=dot bundle=myaxi
#pragma HLS INTERFACE s_axilite port=sum bundle=myaxi
#pragma HLS INTERFACE s_axilite port=din2 bundle=myaxi
#pragma HLS INTERFACE s_axilite port=din1 bundle=myaxi
#pragma HLS INTERFACE s_axilite port=return bundle=myaxi

	int i = 0;
	int prod = 0;

	loop1:for (i=0;i<100;i++){
		sum[i] = din1[i] + din2[i];
		prod += din1[i] * din2[i];
	}

	*dot = prod;
	return;
}
