// 20/03/2019. jgarrigos.
// this example shows how to use array arguments for the hw function,
// instead of pointers, through an saxi_lite interface. 
// 
#include <stdio.h>

void complex_prod_sw(float din1array[10], float din2array[10], float doutarray[10]){

	int i = 0;

	for (i=0;i<10;i+=2){
		doutarray[i] = din1array[i] * din2array[i] - din1array[i+1] * din2array[i+1];
		doutarray[i+1] = din1array[i+1] * din2array[i] + din1array[i] * din2array[i+1];
	}

	return;

}

void complex_prod(float din1array[10], float din2array[10], float doutarray[10]);

int main(){
// complex numbers stored in array as:
// x1
// y1
// x2
// y2
// ...
// xn
// yn

	float datain1[10] = {0.0,2.0,4.0,6.0,8.0,10.0,11.0,12.0,13.0,14.0};
	float datain2[10] = {0.0,2.0,4.0,6.0,8.0,10.0,11.0,12.0,13.0,14.0};
	float dataout_sw[10];
	float dataout_hw[10];
	int i;

	complex_prod_sw(datain1, datain2, dataout_sw);
	complex_prod(datain1, datain2, dataout_hw);

	printf("datain1=0,2,4,6,8,10,11,12,13,14\r\n");
	printf("datain2=0,2,4,6,8,10,11,12,13,14\r\n");
	printf("[i]    dataout_sw      dataout_hw\r\n");
	printf("----------------------------------\r\n");

	for (i=0;i<10;i++){
		printf("%d         %f              %f     \r\n", i, dataout_sw[i], dataout_hw[i]);
	}

	return 0;

}
