
################################################################
# This is a generated script based on design: RealFFT
#
# Though there are limitations about the generated script,
# the main purpose of this utility is to make learning
# IP Integrator Tcl commands easier.
################################################################

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2015.1
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   puts "ERROR: This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."

   return 1
}

################################################################
# START
################################################################

# To test this script, run the following commands from Vivado Tcl console:
# source RealFFT_script.tcl

# If you do not already have a project created,
# you can create a project using the following command:
#    create_project project_1 myproj -part xc7z020clg484-1
#    set_property BOARD_PART xilinx.com:zc702:part0:1.2 [current_project]

# CHECKING IF PROJECT EXISTS
if { [get_projects -quiet] eq "" } {
   puts "ERROR: Please open or create a project!"
   return 1
}



# CHANGE DESIGN NAME HERE
set design_name RealFFT

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "ERROR: Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne $design_name } {
      puts "INFO: Changing value of <design_name> from <$design_name> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   puts "INFO: Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq $design_name } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "ERROR: Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "ERROR: Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   puts "INFO: Currently there is no design <$design_name> in project, so creating one..."

   create_bd_design $design_name

   puts "INFO: Making design <$design_name> as current_bd_design."
   current_bd_design $design_name

}

puts "INFO: Currently the variable <design_name> is equal to \"$design_name\"."

if { $nRet != 0 } {
   puts $errMsg
   return $nRet
}

##################################################################
# DESIGN PROCs
##################################################################



# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     puts "ERROR: Unable to find parent cell <$parentCell>!"
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     puts "ERROR: Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set real2xfft_din [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 real2xfft_din ]
  set_property -dict [ list CONFIG.FREQ_HZ {100000000} CONFIG.HAS_TKEEP {0} CONFIG.HAS_TLAST {0} CONFIG.HAS_TREADY {1} CONFIG.HAS_TSTRB {0} CONFIG.LAYERED_METADATA {xilinx.com:interface:datatypes:1.0 {TDATA {datatype {name {attribs {resolve_type immediate dependency {} format string minimum {} maximum {}} value {}} bitwidth {attribs {resolve_type immediate dependency {} format long minimum {} maximum {}} value 16} bitoffset {attribs {resolve_type immediate dependency {} format long minimum {} maximum {}} value 0} real {fixed {fractwidth {attribs {resolve_type immediate dependency {} format long minimum {} maximum {}} value 15} signed {attribs {resolve_type immediate dependency {} format bool minimum {} maximum {}} value true}}}}}}} CONFIG.PHASE {0.000} CONFIG.TDATA_NUM_BYTES {2} CONFIG.TDEST_WIDTH {0} CONFIG.TID_WIDTH {0} CONFIG.TUSER_WIDTH {0}  ] $real2xfft_din
  set xfft2real_dout [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 xfft2real_dout ]

  # Create ports
  set aclk [ create_bd_port -dir I -type clk aclk ]
  set aresetn [ create_bd_port -dir I -type rst aresetn ]

  # Create instance: hls_real2xfft_0, and set properties
  set hls_real2xfft_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:hls_real2xfft:1.0 hls_real2xfft_0 ]

  # Create instance: hls_xfft2real_0, and set properties
  set hls_xfft2real_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:hls_xfft2real:1.0 hls_xfft2real_0 ]

  # Create instance: xfft_0, and set properties
  set xfft_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xfft:9.0 xfft_0 ]
  set_property -dict [ list CONFIG.aresetn {true} CONFIG.implementation_options {pipelined_streaming_io} CONFIG.number_of_stages_using_block_ram_for_data_and_phase_factors {2} CONFIG.transform_length {512}  ] $xfft_0

  # Create instance: xlconstant_0, and set properties
  set xlconstant_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0 ]

  # Create interface connections
  connect_bd_intf_net -intf_net din_V_V_1 [get_bd_intf_ports real2xfft_din] [get_bd_intf_pins hls_real2xfft_0/din_V_V]
  connect_bd_intf_net -intf_net hls_real2xfft_0_dout_V [get_bd_intf_pins hls_real2xfft_0/dout_V] [get_bd_intf_pins xfft_0/S_AXIS_DATA]
  connect_bd_intf_net -intf_net hls_xfft2real_0_dout_V [get_bd_intf_ports xfft2real_dout] [get_bd_intf_pins hls_xfft2real_0/dout_V]
  connect_bd_intf_net -intf_net xfft_0_M_AXIS_DATA [get_bd_intf_pins hls_xfft2real_0/din_V] [get_bd_intf_pins xfft_0/M_AXIS_DATA]

  # Create port connections
  connect_bd_net -net aclk_1 [get_bd_ports aclk] [get_bd_pins hls_real2xfft_0/ap_clk] [get_bd_pins hls_xfft2real_0/ap_clk] [get_bd_pins xfft_0/aclk]
  connect_bd_net -net aresetn_1 [get_bd_ports aresetn] [get_bd_pins hls_real2xfft_0/ap_rst_n] [get_bd_pins hls_xfft2real_0/ap_rst_n] [get_bd_pins xfft_0/aresetn]
  connect_bd_net -net xlconstant_0_dout [get_bd_pins hls_real2xfft_0/ap_start] [get_bd_pins hls_xfft2real_0/ap_start] [get_bd_pins xlconstant_0/dout]

  # Create address segments
  

  # Restore current instance
  current_bd_instance $oldCurInst

  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


