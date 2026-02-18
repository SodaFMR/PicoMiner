/*
 * 01/09/2020: jgarrigos.
 * Example to test the effect of:
 * loop pipelining
 * loop unrolling
 * function pipelining
 * array partition (requiered by all the previous to take advantage of parallelization)
 */

#include <stdio.h>

void vector_operator_sw(int din1[100], int din2[100], int sum[100], int *dot){

	int i = 0;
	int prod = 0;

	for (i=0;i<100;i++){
		sum[i] = din1[i] + din2[i];
		prod += din1[i] * din2[i];
	}

	*dot = prod;
	return;
}

void vector_operator(int din1[100], int din2[100], int sum[100], int *dot);

int main(){

	int din1[100];
	int din2[100];
	int sum_sw[100];
	int sum_hw[100];
	int dot_sw;
	int dot_hw;
	int i;
	
	for(i=0;i<100;i++){
		din1[i] = 0;
		din2[i] = 0;
		sum_sw[i] = 0;
		sum_hw[i] = 0;	
	}	
	
	din1[10] = 3;
	din2[10] = -5;
	din1[20] = 3;
	din2[20] = 4;
	din1[88] = 1;
	din2[88] = -2;

	vector_operator_sw(din1, din2, sum_sw, &dot_sw);
	vector_operator(din1, din2, sum_hw, &dot_hw);
	
	printf("----------------------------------\r\n");
	printf("[i]    sum_sw    sum_hw           \r\n");
	printf("----------------------------------\r\n");

	for (i=0;i<100;i++){
		printf("%d         %d              %d     \r\n", i, sum_sw[i], sum_hw[i]);
	}
	printf("----------------------------------\r\n");
	printf("dot_sw     dot_hw                 \r\n");
	printf("----------------------------------\r\n");
	printf("%d         %d                     \r\n", dot_sw, dot_hw);
	printf("Good Bye.\r\n");
	return 0;

}
