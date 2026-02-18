// #define STREAMOUTWIDTH 32
// #define STREAMDEPTH 16	// buffer size for FIFO in hardware 
// #define STREAMWIDTH 32  // buffer width for FIFO in hardware 
#define MSKCOLS 5    	// conv mask cols 
#define MSKROWS 5    	// conv mask rows 
#define CMPROWS 3		// comparator kernel rows
#define CMPCOLS 3		// comparator kernel cols

//#define IMROWS  12 // 		 image rows 
//#define IMCOLS  10 // 		 image cols 
//#define INPUT_FILE "imagein12x10.txt" 	// input data file 
#define IMROWS  240 // 		 image rows 
#define IMCOLS  320 // 		 image cols 
#define INPUT_FILE "img3_QVGA.txt" 	// input data file 

#define BUFFER_LENGTH ((MSKROWS/2)*IMCOLS + MSKCOLS/2)

// #include "ap_int.h"
/* typedef ap_uint<16> uint16;
typedef ap_int<32> int32;
typedef ap_uint<1> uint1;
typedef ap_int<18> int18;
typedef ap_int<8> int8;
typedef ap_int<16> int16;
typedef ap_uint<8> uint8;
typedef ap_uint<10> uint10;
typedef ap_int<48> int48;
typedef ap_uint<32> uint32; */

typedef int uint16;
typedef int int32;
typedef int uint1;
typedef int int18;
typedef int int8;
typedef int int16;
typedef int uint8;
typedef int uint10;
typedef int int10;
typedef int int48;
typedef int uint32;
