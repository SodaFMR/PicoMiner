/*
 *
 */

#include <stdio.h>
#include "mycnt_hls.h"
//#include <unistd.h> // for sleep(seconds) in linux
//#include <windows.h> // for Sleep(milliseconds) in windows

void mycnt_hls_sw(int din1, int din2, int *dout1, int *dout2){

	static int cnt=0;
	int i=0; // inserted this variable and modified dout2 to check
			// the effect of defining cnt as static or not.
			// If cnt is not static, i goes from 0 to din2 in each iteration
			// If cnt is static, i goes always goes up to 1000.

	*dout1 = din1 + din2;

	while(1){
		if (din1==0){
			cnt = 0;
			i = 0;
			break;
		}
		else{
			if (cnt < din2){
				cnt = cnt + 1;
				i = i + 1;
			}
			else{
				break;
			}
		}
	}

	*dout2 = i; // *dout2 = cnt;
	return;
}


int main(int argc, char *argv[]){

	int dout1_sw, dout2_sw;
	int dout1_hw, dout2_hw;
	int i;

	for(i=0;i<10;i++){
		printf("Iteration #%d\r\n", i);
		mycnt_hls_sw(i, i*1000, &dout1_sw, &dout2_sw);
		printf("mycnt_hls_sw results: sum = %d\t cnt = %d\r\n", dout1_sw, dout2_sw);
		mycnt_hls(i, i*1000, &dout1_hw, &dout2_hw);
		printf("mycnt_hls_hw results: sum = %d\t cnt = %d\r\n", dout1_hw, dout2_hw);
		//Sleep(i*1000/10);
	}

	printf("Reset: \r\n", i);
	mycnt_hls_sw(0, i*1000, &dout1_sw, &dout2_sw);
	printf("mycnt_hls_sw results: sum = %d\t cnt = %d\r\n", dout1_sw, dout2_sw);
	mycnt_hls(0, i*1000, &dout1_hw, &dout2_hw);
	printf("mycnt_hls_hw results: sum = %d\t cnt = %d\r\n", dout1_hw, dout2_hw);

	return 0;
}
