#include "audio.h"
 #include "board.h"
 #include "processor_hal.h"

// ─── CHANGE THESE TO SET YOUR TONE ───────────────────────────────
#define AUDIO_FREQUENCY_HZ     440       // Tone frequency in Hz (440 = A4, 262 = C4, 523 = C5)
#define AUDIO_DUTY_CYCLE_PCT   50        // Duty cycle % (50 is standard for buzzers)
// ─────────────────────────────────────────────────────────────────

#define TIMER_COUNTER_FREQ          1000000UL                               // Timer counter frequency (1MHz for fine resolution)
#define PWM_PULSE_WIDTH_TICKS       (TIMER_COUNTER_FREQ / AUDIO_FREQUENCY_HZ)   // Period in ticks
#define PWM_PERCENT2TICKS(value)    ((uint16_t)(value * PWM_PULSE_WIDTH_TICKS / 100))


 /* Set pins 50(F14), 56(E11), 60(F15) for Output push pull configuration.
    Set pin 52(E9) for Alternate function push pull configuration to output PWM signal.
    Configure Timer 1 to generate PWM signal with frequency of 5000Hz and 100% duty cycle. */
 void audio_init() {
 
    // Enable GPIO Clock for pin E
    __GPIOE_CLK_ENABLE();

    //Set pins as an output.
    GPIOE->MODER &= ~(0x03 << (9 * 2));     //Clear bits
    GPIOE->MODER |=  (0x02 << (9 * 2));   //Set pin E9 for Alternate Function Push Pull Mode

    //Set pins for fast speed
    GPIOE->OSPEEDR &= ~(0x03<<(9 * 2));  //Clear bits
    GPIOE->OSPEEDR |= 0x02 << (9 * 2); //Set pin E9 fast speed

    //Clear pins bit for push pull output
    GPIOE->OTYPER &= ~(0x01 << 9);
    GPIOE->PUPDR  &= ~(0x03 << (9 * 2));
    
    GPIOE->AFR[1] &= ~((0x0F) << ((9-8) * 4));  //Clear Alternate Function for pin E9 (lower ARF register)
    GPIOE->AFR[1] |= (GPIO_AF1_TIM1 << ((9-8) * 4));	//Set Alternate Function for pin E9 (lower ARF register) 

    // Enable TIM1 clock
    __TIM1_CLK_ENABLE();

    // Prescaler: divide system clock down to TIMER_COUNTER_FREQ (1MHz)
    TIM1->PSC = ((SystemCoreClock / 2) / TIMER_COUNTER_FREQ) - 1;

    // Up-counting
    TIM1->CR1 &= ~TIM_CR1_DIR;

    // Set period and duty cycle from defines above
    TIM1->ARR  = PWM_PULSE_WIDTH_TICKS - 1;
    TIM1->CCR1 = PWM_PERCENT2TICKS(AUDIO_DUTY_CYCLE_PCT);

    // PWM Mode 1 on Channel 1, enable preload
    TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM1->CCMR1 |=  (0x6 << 4);
    TIM1->CCMR1 |=  TIM_CCMR1_OC1PE;

    // Auto-reload preload, output enable, active high
    TIM1->CR1  |=  TIM_CR1_ARPE;
    TIM1->CCER |=  TIM_CCER_CC1E;
    TIM1->CCER &= ~TIM_CCER_CC1NE;
    TIM1->CCER &= ~TIM_CCER_CC1P;

    // Main output enable
    TIM1->BDTR |= TIM_BDTR_MOE | TIM_BDTR_OSSR | TIM_BDTR_OSSI;

    // Start the counter
    TIM1->CR1 |= TIM_CR1_CEN;
 }

void audio_on(void) {
    TIM1->BDTR |= TIM_BDTR_MOE;   // Enable main output
    TIM1->CR1  |= TIM_CR1_CEN;    // Enable counter
}

void audio_off(void) {
    TIM1->BDTR &= ~TIM_BDTR_MOE;  // Disable main output (pin goes idle)
    TIM1->CR1  &= ~TIM_CR1_CEN;   // Stop counter
}