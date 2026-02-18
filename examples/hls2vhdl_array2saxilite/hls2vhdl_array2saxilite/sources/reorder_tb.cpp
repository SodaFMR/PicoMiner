// 02/06/2017. jgarrigos. 
// this example shows how to use array arguments for the hw function,
// instead of pointers, through an saxi_lite interface. 
// 
#include <stdio.h>

void reorder_sw (int dinarray[10], int doutarray[10]){

	int i = 0;

	for (i=0;i<10;i++){
		doutarray[9-i] = dinarray[i] + i;
	}

	return;

}

void reorder(int dinarray[10], int doutarray[10]);

int main(){

	int datain[10] = {0,2,4,6,8,10,11,12,13,14};
	int dataout_sw[10];
	int dataout_hw[10];
	int i;

	reorder_sw(datain, dataout_sw);
	reorder(datain, dataout_hw);

	printf("datain=0,2,4,6,8,10,11,12,13,14\r\n");
	printf("[i]    dataout_sw      dataout_hw\r\n");
	printf("----------------------------------\r\n");

	for (i=0;i<10;i++){
		printf("%d         %d              %d     \r\n", i, dataout_sw[i], dataout_hw[i]);
	}

	return 0;

}
