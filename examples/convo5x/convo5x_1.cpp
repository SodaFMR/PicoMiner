#include <stdio.h>
#include "convo.h"


void convo5x_1(bool din_vld, int32 pixelIn, bool *dout_vld, int32 *pixelOut, int32 *pixelCentral)
{	
	// 5 line buffers needed for 5x5 convo
	static uint8 linebuffer0[IMCOLS]; //arrays defined as static initialize all elements to 0 if no initializer is specified
    static uint8 linebuffer1[IMCOLS]; //arrays defined as static initialize all elements to 0 if no initializer is specified
    static uint8 linebuffer2[IMCOLS]; //arrays defined as static initialize all elements to 0 if no initializer is specified
	static uint8 linebuffer3[IMCOLS]; //arrays defined as static initialize all elements to 0 if no initializer is specified
	static uint8 linebuffer4[IMCOLS]; //arrays defined as static initialize all elements to 0 if no initializer is specified
	
	// convolution mask is defined as a const,
	// use 5 buffers for mask data for column-level (5x) parallelism.
    
    const uint10 maskrow0[MSKCOLS]={100,180,220,180,100}; 
	const uint10 maskrow1[MSKCOLS]={180,324,396,324,180}; 
	const uint10 maskrow2[MSKCOLS]={220,396,484,396,220};
	const uint10 maskrow3[MSKCOLS]={180,324,396,324,180};
	const uint10 maskrow4[MSKCOLS]={100,180,220,180,100};
    /*
    const uint10 maskrow0[MSKCOLS]={0,0,0,0,0}; 
	const uint10 maskrow1[MSKCOLS]={0,0,0,0,0}; 
	const uint10 maskrow2[MSKCOLS]={0,0,1,0,0};
	const uint10 maskrow3[MSKCOLS]={0,0,0,0,0};
	const uint10 maskrow4[MSKCOLS]={0,0,0,0,0};
    */
	// pixelin's col and row indexes 
	static int32 pixelInCol=0;
	static int32 pixelInRow=0;
	// convolution central pixel's col and row indexes
	static int32 pixelConvoCol=0;
	static int32 pixelConvoRow=0;
    // delays between input pixel and convolution central pixel
	static int32 pixelConvoColDelay[MSKCOLS/2];
    static int32 pixelConvoRowDelay[MSKROWS/2];
	
	// other aux variables 
	int8 i;
	int8 hdisp;
    int8 vdisp;
    bool rowmask[MSKROWS]={0};
	int32 pout=0;
    
    if (din_vld==1){ // do not begin storing & convolving pixels until there are valid input pixels
        /////////////////////////////////////////////////////	
        // Ring buffer mechanishm: stores the last 5 lines	
        // for column-level (5x) parallelism.
        /////////////////////////////////////////////////////
        linebuffer0[pixelInCol]=linebuffer1[pixelInCol];
        linebuffer1[pixelInCol]=linebuffer2[pixelInCol];
        linebuffer2[pixelInCol]=linebuffer3[pixelInCol];
        linebuffer3[pixelInCol]=linebuffer4[pixelInCol];
        linebuffer4[pixelInCol]=pixelIn;
        // End of Ring buffer
        
        //////////////////////////////////////////////////////
        // calculate  col and row of the convo's central pixel 
        // not necessary, but easyer for determining convo at the left/right borders
        ///////////////////////////////////////////////////////

        
        //////////////////////////////////////////////////////////////////////////////////
        // row masx for top-down borders (avoids image overlaping for continous convolutions)
        /////////////////////////////////////////////////////////////////////////////////
        for (i=0;i<MSKROWS;i++){
            vdisp = i - MSKROWS/2; //vertical displacement goes from  -MSKROWS/2 to MSKROWS/2 
             if ((pixelConvoRow + vdisp >= 0) && (pixelConvoRow + vdisp <= IMROWS-1))  // check top-down borders
                rowmask[i]=1;
            else
                rowmask[i]=0;
        }
        
        //////////////////////////////////////////////////
        // 5x multipliers convolution
        ///////////////////////////////////////////////////
        col:for (i=0;i<MSKCOLS;i++) {// for every mask column
            hdisp = i - MSKCOLS/2; //horizontal displacement goes from  -MSKCOLS/2 to MSKCOLS/2     
            if ((pixelConvoCol + hdisp >= 0) && (pixelConvoCol + hdisp <= IMCOLS-1)) { // check left-right borders
                pout = pout + 	rowmask[0] * linebuffer0[pixelConvoCol + hdisp] * maskrow0[i]+
                                rowmask[1] * linebuffer1[pixelConvoCol + hdisp] * maskrow1[i]+
                                rowmask[2] * linebuffer2[pixelConvoCol + hdisp] * maskrow2[i]+
                                rowmask[3] * linebuffer3[pixelConvoCol + hdisp] * maskrow3[i]+
                                rowmask[4] * linebuffer4[pixelConvoCol + hdisp] * maskrow4[i];
            }
        }
        // End 5x multipliers convolution 
        
        ////////////////////////////////////////////////////
        // assign computed convolution to output variable,
        // divided by factor 6084 in order to normalize
        ////////////////////////////////////////////////////
        *pixelOut = pout/6084;
        // output also the input pixel, that is now synchronized with the output pixel
        *pixelCentral = linebuffer2[pixelConvoCol];
        // assert output data valid flag if more than buffer_length input pixels have been received
        //if (((pixelInRow+1)*IMCOLS+pixelInCol+1>BUFFER_LENGTH) && (pixelConvoRow<IMROWS)){
        if (((pixelInRow)*IMCOLS+pixelInCol+1>BUFFER_LENGTH) && (*dout_vld==0)){
            *dout_vld = 1;
        }
        
         ////////////////////////////////////////////////////////
        // increment or initialize input pixel's col an row counters
        ////////////////////////////////////////////////////////
        pixelInCol=pixelInCol+1;        
        if (pixelInCol==IMCOLS) {
            pixelInCol=0;
            pixelInRow=pixelInRow+1;            
            
            if (pixelInRow==IMROWS) {
                pixelInRow=0;            
            }            
        }
        
        pixelConvoCol = pixelConvoColDelay[0];
        for (i=0;i<MSKCOLS/2-1;i++){
            pixelConvoColDelay[i]=pixelConvoColDelay[i+1];
        }
        pixelConvoColDelay[MSKCOLS/2-1] = pixelInCol;
        if (pixelConvoCol == 0){
            //pixelConvoRow = pixelInRow - MSKROWS/2;
            
            pixelConvoRow = pixelConvoRowDelay[0];
            for (i=0;i<MSKCOLS/2-1;i++){
                pixelConvoRowDelay[i]=pixelConvoRowDelay[i+1];
            }
            pixelConvoRowDelay[MSKCOLS/2-1]=pixelInRow;
            
        }
    }
/*
    else{
        //////////////////////////////////////////////////////////////////
        // din_vld desasserted means new input image, so reset all counters
        //////////////////////////////////////////////////////////////////
        pixelInCol = 0;
        pixelInRow = 0;
        pixelConvoCol = 0;
        pixelConvoRow = 0;
        for(i=0;i<MSKCOLS/2;i++){
            pixelConvoColDelay[i] = 0;
            pixelConvoRowDelay[i] = 0;
        }        
    }

*/    
    
}
