#include "debug_log.h"
#include "ili9341.h"
#include "board.h"

void hardware_init(void);
/*
 * Main program
 */
int main(void)  {


    int prev_tick;

    HAL_Init();			//Initalise Board
	hardware_init();
        // Main processing loop
   while (1) {

         ili9341_draw_filled_rect(0, 0, 240, 320, ILI9341_BLUE);
        HAL_Delay(500);
        ili9341_draw_filled_rect(0, 0, 240, 320, ILI9341_RED);
        HAL_Delay(500);


    }

      return 0;
    }

void hardware_init(void) {

    BRD_debuguart_init();  //Initialise UART for debug log output
    ili9341_init();



    //Set all sysmon outputs to low.
    BRD_sysmon_init();  //Initialise sysmon
   
}
