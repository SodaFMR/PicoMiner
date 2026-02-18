############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 2015 Xilinx Inc. All rights reserved.
############################################################
open_project test
set_top fir
add_files fir.c
add_files fir.c
add_files -tb fir_test.c
add_files -tb out.gold.dat
add_files -tb fir_test.c
add_files -tb out.gold.dat
open_solution "solution1"
set_part {xq7vx690trf1930-1i}
create_clock -period 10 -name default
#source "./test/solution1/directives.tcl"
csim_design
csynth_design
cosim_design -rtl vhdl
export_design -format ip_catalog
