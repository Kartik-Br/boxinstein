/**
  ******************************************************************************
  * @file    adc/main.c
  * @author  MDS
  * @brief   2-axis analog joystick reading ADC1 on PA3 (X) and PA0 (Y).
  *          Joystick button on PC0.
  *          Controls onboard LEDs:
  *            Up    -> Green LED
  *            Down  -> Red LED
  *            Left  -> Blue LED
  *            Right -> Orange LED
  *            Press -> All LEDs
  *
  *          Wiring:
  *            VRX (X-axis) -> PA3  (ADC1 Channel 3)
  *            VRY (Y-axis) -> PA0  (ADC1 Channel 0)
  *            SW  (button) -> PC0  (digital input, internal pull-up)
  *            VCC          -> 3.3V
  *            GND          -> GND
  ******************************************************************************
  */

#include "board.h"
#include "processor_hal.h"

/* ── ADC handles ─────────────────────────────────────────── */
ADC_HandleTypeDef AdcHandle;
ADC_ChannelConfTypeDef AdcChanConfig;

/* ── Deadzone: values within this range of centre = neutral ─ */
#define ADC_CENTRE     0x7FF   /* 2047 – midpoint of 12-bit range */
#define ADC_DEADZONE   0x2FF   /* ~750 counts either side         */

/* ── Prototypes ─────────────────────────────────────────────  */
void hardware_init(void);
unsigned int read_adc_channel(uint32_t channel);

/* ─────────────────────────────────────────────────────────── */
int main(void)
{
    unsigned int adc_x, adc_y;
    int joy_x, joy_y;           /* signed displacement from centre */

    HAL_Init();
    hardware_init();

    while (1)
    {
        /* ── Read X axis (PA3, channel 3) ── */
        adc_x = read_adc_channel(ADC_CHANNEL_3);

        /* ── Read Y axis (PA0, channel 0) ── */
        adc_y = read_adc_channel(ADC_CHANNEL_0);

        /* Signed displacement from centre */
        joy_x = (int)adc_x - ADC_CENTRE;
        joy_y = (int)adc_y - ADC_CENTRE;

        /* ── Turn off all LEDs before deciding which to light ── */
        BRD_LEDGreenOff();
        BRD_LEDRedOff();
        BRD_LEDBlueOff();

        /* ── Button press: all LEDs on ── */
        if (!(GPIOC->IDR & GPIO_PIN_0))
        {
            BRD_LEDGreenOn();
            BRD_LEDRedOn();
            BRD_LEDBlueOn();
        }
        else
        {
            /* ── Y axis: Up / Down ── */
            if (joy_y > (int)ADC_DEADZONE)
            {
                BRD_LEDGreenOn();   /* Up   */
            }
            else if (joy_y < -(int)ADC_DEADZONE)
            {
                BRD_LEDRedOn();     /* Down */
            }

            /* ── X axis: Left / Right ── */
            if (joy_x < -(int)ADC_DEADZONE)
            {
                BRD_LEDBlueOn();    /* Left  */
            }
            else if (joy_x > (int)ADC_DEADZONE)
            {
            }
        }

        HAL_Delay(50);      }
}

/* ─────────────────────────────────────────────────────────── */
/*  Read a single ADC channel and return the 12-bit result     */
/* ─────────────────────────────────────────────────────────── */
unsigned int read_adc_channel(uint32_t channel)
{
    AdcChanConfig.Channel      = channel;
    AdcChanConfig.Rank         = 1;
    AdcChanConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    AdcChanConfig.Offset       = 0;
    HAL_ADC_ConfigChannel(&AdcHandle, &AdcChanConfig);

    HAL_ADC_Start(&AdcHandle);
    while (HAL_ADC_PollForConversion(&AdcHandle, 10) != HAL_OK);
    return AdcHandle.Instance->DR;
}

/* ─────────────────────────────────────────────────────────── */
/*  Hardware initialisation                                     */
/* ─────────────────────────────────────────────────────────── */
void hardware_init(void)
{
    BRD_LEDInit();
    BRD_LEDGreenOff();
    BRD_LEDRedOff();
    BRD_LEDBlueOff();

    /* ── PA3 (VRX) and PA0 (VRY) as Analog inputs ── */
    __GPIOA_CLK_ENABLE();

    /* PA3 */
    GPIOA->MODER   |=  (0x03 << (3 * 2));  /* Analog mode   */
    GPIOA->OSPEEDR &= ~(0x03 << (3 * 2));
    GPIOA->OSPEEDR |=  (0x02 << (3 * 2));  /* Fast speed    */
    GPIOA->PUPDR   &= ~(0x03 << (3 * 2));  /* No pull       */

    /* PA0 */
    GPIOA->MODER   |=  (0x03 << (0 * 2));
    GPIOA->OSPEEDR &= ~(0x03 << (0 * 2));
    GPIOA->OSPEEDR |=  (0x02 << (0 * 2));
    GPIOA->PUPDR   &= ~(0x03 << (0 * 2));

    /* ── PC0 (SW button) as digital input with internal pull-up ── */
    __GPIOC_CLK_ENABLE();

    GPIOC->MODER   &= ~(0x03 << (0 * 2));  /* Input mode    */
    GPIOC->PUPDR   &= ~(0x03 << (0 * 2));
    GPIOC->PUPDR   |=  (0x01 << (0 * 2));  /* Pull-up       */

    /* ── ADC1 setup ── */
    __ADC1_CLK_ENABLE();

    AdcHandle.Instance                   = (ADC_TypeDef *)(ADC1_BASE);
    AdcHandle.Init.ClockPrescaler        = ADC_CLOCKPRESCALER_PCLK_DIV2;
    AdcHandle.Init.Resolution            = ADC_RESOLUTION12b;
    AdcHandle.Init.ScanConvMode          = DISABLE;
    AdcHandle.Init.ContinuousConvMode    = DISABLE;
    AdcHandle.Init.DiscontinuousConvMode = DISABLE;
    AdcHandle.Init.NbrOfDiscConversion   = 0;
    AdcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    AdcHandle.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T1_CC1;
    AdcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    AdcHandle.Init.NbrOfConversion       = 1;
    AdcHandle.Init.DMAContinuousRequests = DISABLE;
    AdcHandle.Init.EOCSelection          = DISABLE;

    HAL_ADC_Init(&AdcHandle);
}
