/*
 *
 */
void mycnt_hls(int din1, int din2, int *dout1, int *dout2){
#pragma HLS INTERFACE s_axilite port=dout2 bundle=myaxi
#pragma HLS INTERFACE s_axilite port=dout1 bundle=myaxi
#pragma HLS INTERFACE s_axilite port=din2 bundle=myaxi
#pragma HLS INTERFACE s_axilite port=din1 bundle=myaxi
#pragma HLS INTERFACE s_axilite port=return bundle=myaxi

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
