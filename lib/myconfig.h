 /** 
 **************************************************************
 * File: mylib/myconfig.h
 * Author: JIA SHEN PANG - 49026849
 * Date: 13052025
 * Brief: MyConfig Mylib Register Driver
 * REFERENCE: DON'T JUST COPY THIS BLINDLY.pdf 
 ***************************************************************
 * EXTERNAL FUNCTIONS 
 ***************************************************************
 * 
 *************************************************************** 
 */

 #ifndef MYCONFIG_H
 #define MYCONFIG_H

 #include<stdint.h>

 #define MYRADIOCHAN 40

 uint8_t myradiotxaddr[5] = {0x12,0x34,0x56,0x78,0x90};
 
 #endif