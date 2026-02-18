
///////////////////////////////////////////////////////////////////////////////////////////
// Javier Garrigós 
// 14/11/2016:
// - This example could replace labs 1 and 2 of the Embedded System Design Xilinx Workshop (2015.2). 
// - This example is used as a recapitulation after labs 1 and 2 in CoDiseño de Sistemas Digitales (2016/2017). 
// 11/02/2022: 
// - Improved syntax and general comments. 
// - Added section: Design Procedure with Vivado 2016.2
///////////////////////////////////////////////////////////////////////////////////////////

This is a basic ZYNQ design example: apart from the ZYNQ module, it instantiates 2 AXI GPIO modules in the PL to control switches and leds in the Zedboard. NO custom peripheral is created. 

The sw (standalone, no OS) first configures both axi peripherals and then enter an infinite loop that every second reads from the sws and writes their positioning both on the leds and on the user console. 

The sw uses the C sleep command (or usleep) to wait for 1 second between reads. 

To print sws data with better formating, use:
---------------------------------------------------
printf("sws_data: 0x%02x\n\r", (uint)sws_data);
---------------------------------------------------

You could use also the following code to wait for user prompt before a new read, instead of waiting for a fixed time:
---------------------------
char keypress;
...
printf("Continue y/n: ");
scanf("%s", &keypress);
printf("%c\n\r", keypress);
if (keypress == 'n')
	break;
----------------------------

/////////////////////////////////////////////////////////////////////////////////////////////////
// Design procedure using Vivado 2016.2
/////////////////////////////////////////////////////////////////////////////////////////////////
==============================================
Part One: Define block diagram in Vivado 
==============================================
# Create a new Vivado project. 
-	Select Do not specify sources at this time
-	Select VHDL instead of Verilog as the preferred design language
-	Select Zedboard as the target board
# Add a new block diagram
# Add a zynq block to the diagram
	Run designer assistance /connection automation to setup default configuration. 
# Add 2 axi_gpios and configure them for the switches and leds respectively.	
	Run designer assistance /connection automation to setup default configuration. 
# Validate design. Check that there are no errors in the block diagram. The BD should be as this one: 
# Select .bd file and click on Generate output products in the floating menu. 
# Select .bd file and click on Generate HDL wrapper in the floating menu. 
# Select Generate bitstream. Synthesis and implementation (translate, map, place&route) will be run if they don’t exist or are not up-to-date. 
# Export hardware design to SDK (Menu > File > Export > Export hardware). Do not forget to select “Include Bitstream” option.
# Launch SDK.

==================================================================================
Part Two: Design the application software to execute on the microprocessor in SDK
==================================================================================
# Create app for the zynq arm core0: 
-	Select Menu > File > New application project
-	Enter the desired name for the app
-	Check that the hw platform selected is the correct one if there is more than one
-	Check that the OS is configured as standalone (no OS in fact)
-	Check that a new BSP will be created for the hw platform
-	Select a “helloworld” template for your app

# Modify template with the code provided in helloworld_sws2_leds.c. 
# Save your app file. This will also run compile. Check that no errors are shown in the log console window and that an executable (.elf) file is produced correctly. 
# Go to Menu > Xilinx Tools and select Program FPGA. 
-	Of course, at this time, you should have connected and turned on the board. 
-	Do not forget to connect both cables: usb-jtag to program the fpga and usb-uart for the console for the microprocessor standard I/O. 
-	Check that a blue led on the board lights on, this means programming has been successful. 
# Open the Teraterm utility and config a connection to the serial COM6 port (or whichever assigned by de OS to the usb-uart) with 115200bps. Let the rest of parameters with their default values. 
-	The COM6 port will not be present if the ZedBoard is not powered. 
# Select your app project and, in the floating menu, select Run As> launch on hardware (GDB)
# Check the teraterm console and see your app executing on the board. 
# CONGRATULATIONS!! 

