#include "board.h"
#include "processor_hal.h"
#include "debug_log.h"
#include "audio.h"

int main(void){
    HAL_Init();
    BRD_debuguart_init();

    audio_init();

    while(1){
        audio_on();       
        HAL_Delay(500);

        audio_off();     
        HAL_Delay(500);
    }
}