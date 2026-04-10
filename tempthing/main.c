#include "debug_log.h"
#include "ili9341.h"
#include "board.h"
// Define your scale factor (2 = half res, 4 = quarter res)
#define RES_SCALE 2 

void hardware_init(void);
/*
 * Main program
 */
int main(void)  {


    int prev_tick;

    HAL_Init();			//Initalise Board
	hardware_init();

    ili9341_fill_dma(0, 0, ILI9341_WIDTH, ILI9341_HEIGHT, ILI9341_BLACK);

        // Main processing loop
   while (1) {

        HAL_Delay(1000);

        ili9341_fill_dma(0, 0, ILI9341_WIDTH/2, ILI9341_HEIGHT/2, ILI9341_WHITE);


        ili9341_fill_dma(0, 0, 24, 32, ILI9341_BLUE);
        ili9341_fill_dma(50, 50, 24, 32, ILI9341_RED);
        ili9341_draw_string(10, 200, "GURTY BAYO!", ILI9341_BLACK, ILI9341_WHITE);

        HAL_Delay(1000);

        ili9341_fill_dma(0, 0, ILI9341_WIDTH/2, ILI9341_HEIGHT/2, ILI9341_BLACK);

        ili9341_fill_dma(0, 0, 24, 32, ILI9341_BLUE);
        ili9341_fill_dma(50, 50, 24, 32, ILI9341_RED);
        ili9341_draw_string(10, 200, "GURTY BAYO!", ILI9341_BLACK, ILI9341_WHITE);





    }

      return 0;
    }

void hardware_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    ili9341_init();



   
}



