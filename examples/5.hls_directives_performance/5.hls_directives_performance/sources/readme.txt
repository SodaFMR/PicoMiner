01/09/2020: v1.0. jgarrigos.
Performance estimation project: hls_directive_performance
-------------------------------------------------------------------
This project is to demonstrate the influence of main directives (pipeline, unroll, array_partition) in the circuit performance (latency and throughput parameters).

The module computes the sum and the dot-product of two 100-element vectors. 

The project is defined just at the level of vivado_hls, the implementation (vivado) is not discusssed here. 

The project compiles and synthesizes correctly. The results are coherent and in agreement with the expected performance, as detailed below. 
 
Definitions: 
------------
- Data input: two 100-elements vectors of 16-bit integers

- Data output: element-wise sum (100-elements vector of 16bit int) and dot product (scalar, 16bit int)

- Data rate is then defined as:
	DR(or throughput in Gbps) = datain_wide * clk freq/throughput(ii in clks)

Performance results for project: hls_directive_performance
----------------------------------------------------------
- Base design: latency=701, ii=702
	this is 7clk per loop iteration, repeated 100 times 
	
	DR = (32*100)*100e6/702 = 455 Mbps     -->> not very exciting...
	
- loop pipelining: latency=107, ii=108
	computations inside the loop are 7-stage pipelined, thus it takes basically 
	100clks to compute the full loop, plus 8 clks to fill in the pipeline
	
	DR = (32*100)*100e6/108 = 2.9 Gbps     -->> 7x throughput with roughly the same area!
	
- array partition and loop unrolling: latency=8, ii=9
	all loop iterations are executed in parallel, thus, the loop is executed in the same 8clks that it take to execute a single iteration in the base design.
	
	DR = (32*100)*100e6/9 = 35.5 Gbps     -->> 100x throughput with roughly 100x area!!

- array partition and function pipelining: L=8, ii=1 (loops autom. unrolled)
	when the whole function is pipelined, all loops inside must be also unrolled, and then the design is pipelined from inputs to outputs. In this case, latency is still 8clks, but througput is just 1clk!!! This means you can put a new couple of data 
	vectors at the input at every single clk cycle!!!
	
	DR = (32*100)*100e6/1 = 320 Gbps      -->> 700x throughput with roughly 100x area!!!

Directives used for each experiment: 
-------------------------------
Common to all (included in source file):

	void vector_operator(int din1[100], int din2[100], int sum[100], int *dot){
	#pragma HLS INTERFACE s_axilite port=dot bundle=myaxi
	#pragma HLS INTERFACE s_axilite port=sum bundle=myaxi
	#pragma HLS INTERFACE s_axilite port=din2 bundle=myaxi
	#pragma HLS INTERFACE s_axilite port=din1 bundle=myaxi
	#pragma HLS INTERFACE s_axilite port=return bundle=myaxi

Additional directives.tcl file contents per design experiment: 
- Base design: latency=701, ii=702

	nothing added

- loop pipelining: latency=107, ii=108

	set_directive_pipeline "vector_operator/loop1"

- array partition and loop unrolling: latency=8, ii=9

	set_directive_array_partition -type complete -dim 1 "vector_operator" din1
	set_directive_array_partition -type complete -dim 1 "vector_operator" din2
	set_directive_array_partition -type complete -dim 1 "vector_operator" sum
	set_directive_unroll "vector_operator/loop1"

- array partition and function pipelining: L=8, ii=1 (loops autom. unrolled)

	set_directive_array_partition -type complete -dim 1 "vector_operator" din1
	set_directive_array_partition -type complete -dim 1 "vector_operator" din2
	set_directive_array_partition -type complete -dim 1 "vector_operator" sum
	set_directive_pipeline "vector_operator"

