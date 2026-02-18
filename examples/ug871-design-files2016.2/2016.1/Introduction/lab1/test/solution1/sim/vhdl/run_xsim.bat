
call xelab xil_defaultlib.apatb_fir_top -prj fir.prj --lib "ieee_proposed=./ieee_proposed" -s fir 
call xsim --noieeewarnings fir -tclbatch fir.tcl

