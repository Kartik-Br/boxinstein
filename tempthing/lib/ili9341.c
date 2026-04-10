/**
 **************************************************************
 * File: mylib/ili9341.c
 * Brief: ILI9341 TFT LCD driver for STM32 NUCLEO-F429ZI
 *        Uses SPI1 in register-level polling mode (no HAL SPI).
 *        Provides: init, filled-rect, char, and string drawing.
 ***************************************************************
 * EXTERNAL FUNCTIONS
 ***************************************************************
 * ili9341_init()                              - Initialise display
 * ili9341_draw_filled_rect(x,y,w,h,color)     - Filled rectangle
 * ili9341_draw_char(x,y,c,color,bg)           - Single character
 * ili9341_draw_string(x,y,str,color,bg)       - Text string
 ***************************************************************
 */

#include "ili9341.h"

/* ------------------------------------------------------------------ */
/*  Built-in 5×7 font (ASCII 0x20–0x7E)                               */
/*  Each character is 5 columns × 7 rows, stored as 5 bytes where     */
/*  bit 0 = top row, bit 6 = bottom row.                              */
/* ------------------------------------------------------------------ */
#define FONT_WIDTH  5
#define FONT_HEIGHT 7

static const uint8_t font5x7[][FONT_WIDTH] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 0x20  ' ' */
    { 0x00, 0x00, 0x5F, 0x00, 0x00 }, /* 0x21  '!' */
    { 0x00, 0x07, 0x00, 0x07, 0x00 }, /* 0x22  '"' */
    { 0x14, 0x7F, 0x14, 0x7F, 0x14 }, /* 0x23  '#' */
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, /* 0x24  '$' */
    { 0x23, 0x13, 0x08, 0x64, 0x62 }, /* 0x25  '%' */
    { 0x36, 0x49, 0x55, 0x22, 0x50 }, /* 0x26  '&' */
    { 0x00, 0x05, 0x03, 0x00, 0x00 }, /* 0x27  ''' */
    { 0x00, 0x1C, 0x22, 0x41, 0x00 }, /* 0x28  '(' */
    { 0x00, 0x41, 0x22, 0x1C, 0x00 }, /* 0x29  ')' */
    { 0x08, 0x2A, 0x1C, 0x2A, 0x08 }, /* 0x2A  '*' */
    { 0x08, 0x08, 0x3E, 0x08, 0x08 }, /* 0x2B  '+' */
    { 0x00, 0x50, 0x30, 0x00, 0x00 }, /* 0x2C  ',' */
    { 0x08, 0x08, 0x08, 0x08, 0x08 }, /* 0x2D  '-' */
    { 0x00, 0x60, 0x60, 0x00, 0x00 }, /* 0x2E  '.' */
    { 0x20, 0x10, 0x08, 0x04, 0x02 }, /* 0x2F  '/' */
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, /* 0x30  '0' */
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, /* 0x31  '1' */
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, /* 0x32  '2' */
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, /* 0x33  '3' */
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, /* 0x34  '4' */
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, /* 0x35  '5' */
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, /* 0x36  '6' */
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, /* 0x37  '7' */
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, /* 0x38  '8' */
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, /* 0x39  '9' */
    { 0x00, 0x36, 0x36, 0x00, 0x00 }, /* 0x3A  ':' */
    { 0x00, 0x56, 0x36, 0x00, 0x00 }, /* 0x3B  ';' */
    { 0x00, 0x08, 0x14, 0x22, 0x41 }, /* 0x3C  '<' */
    { 0x14, 0x14, 0x14, 0x14, 0x14 }, /* 0x3D  '=' */
    { 0x41, 0x22, 0x14, 0x08, 0x00 }, /* 0x3E  '>' */
    { 0x02, 0x01, 0x51, 0x09, 0x06 }, /* 0x3F  '?' */
    { 0x32, 0x49, 0x79, 0x41, 0x3E }, /* 0x40  '@' */
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, /* 0x41  'A' */
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, /* 0x42  'B' */
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, /* 0x43  'C' */
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, /* 0x44  'D' */
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, /* 0x45  'E' */
    { 0x7F, 0x09, 0x09, 0x09, 0x01 }, /* 0x46  'F' */
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, /* 0x47  'G' */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, /* 0x48  'H' */
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, /* 0x49  'I' */
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, /* 0x4A  'J' */
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, /* 0x4B  'K' */
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, /* 0x4C  'L' */
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, /* 0x4D  'M' */
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, /* 0x4E  'N' */
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, /* 0x4F  'O' */
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, /* 0x50  'P' */
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, /* 0x51  'Q' */
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, /* 0x52  'R' */
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, /* 0x53  'S' */
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, /* 0x54  'T' */
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, /* 0x55  'U' */
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, /* 0x56  'V' */
    { 0x3F, 0x40, 0x38, 0x40, 0x3F }, /* 0x57  'W' */
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, /* 0x58  'X' */
    { 0x07, 0x08, 0x70, 0x08, 0x07 }, /* 0x59  'Y' */
    { 0x61, 0x51, 0x49, 0x45, 0x43 }, /* 0x5A  'Z' */
    { 0x00, 0x7F, 0x41, 0x41, 0x00 }, /* 0x5B  '[' */
    { 0x02, 0x04, 0x08, 0x10, 0x20 }, /* 0x5C  '\' */
    { 0x00, 0x41, 0x41, 0x7F, 0x00 }, /* 0x5D  ']' */
    { 0x04, 0x02, 0x01, 0x02, 0x04 }, /* 0x5E  '^' */
    { 0x40, 0x40, 0x40, 0x40, 0x40 }, /* 0x5F  '_' */
    { 0x00, 0x01, 0x02, 0x04, 0x00 }, /* 0x60  '`' */
    { 0x20, 0x54, 0x54, 0x54, 0x78 }, /* 0x61  'a' */
    { 0x7F, 0x48, 0x44, 0x44, 0x38 }, /* 0x62  'b' */
    { 0x38, 0x44, 0x44, 0x44, 0x20 }, /* 0x63  'c' */
    { 0x38, 0x44, 0x44, 0x48, 0x7F }, /* 0x64  'd' */
    { 0x38, 0x54, 0x54, 0x54, 0x18 }, /* 0x65  'e' */
    { 0x08, 0x7E, 0x09, 0x01, 0x02 }, /* 0x66  'f' */
    { 0x0C, 0x52, 0x52, 0x52, 0x3E }, /* 0x67  'g' */
    { 0x7F, 0x08, 0x04, 0x04, 0x78 }, /* 0x68  'h' */
    { 0x00, 0x44, 0x7D, 0x40, 0x00 }, /* 0x69  'i' */
    { 0x20, 0x40, 0x44, 0x3D, 0x00 }, /* 0x6A  'j' */
    { 0x7F, 0x10, 0x28, 0x44, 0x00 }, /* 0x6B  'k' */
    { 0x00, 0x41, 0x7F, 0x40, 0x00 }, /* 0x6C  'l' */
    { 0x7C, 0x04, 0x18, 0x04, 0x78 }, /* 0x6D  'm' */
    { 0x7C, 0x08, 0x04, 0x04, 0x78 }, /* 0x6E  'n' */
    { 0x38, 0x44, 0x44, 0x44, 0x38 }, /* 0x6F  'o' */
    { 0x7C, 0x14, 0x14, 0x14, 0x08 }, /* 0x70  'p' */
    { 0x08, 0x14, 0x14, 0x18, 0x7C }, /* 0x71  'q' */
    { 0x7C, 0x08, 0x04, 0x04, 0x08 }, /* 0x72  'r' */
    { 0x48, 0x54, 0x54, 0x54, 0x20 }, /* 0x73  's' */
    { 0x04, 0x3F, 0x44, 0x40, 0x20 }, /* 0x74  't' */
    { 0x3C, 0x40, 0x40, 0x20, 0x7C }, /* 0x75  'u' */
    { 0x1C, 0x20, 0x40, 0x20, 0x1C }, /* 0x76  'v' */
    { 0x3C, 0x40, 0x30, 0x40, 0x3C }, /* 0x77  'w' */
    { 0x44, 0x28, 0x10, 0x28, 0x44 }, /* 0x78  'x' */
    { 0x0C, 0x50, 0x50, 0x50, 0x3C }, /* 0x79  'y' */
    { 0x44, 0x64, 0x54, 0x4C, 0x44 }, /* 0x7A  'z' */
    { 0x00, 0x08, 0x36, 0x41, 0x00 }, /* 0x7B  '{' */
    { 0x00, 0x00, 0x7F, 0x00, 0x00 }, /* 0x7C  '|' */
    { 0x00, 0x41, 0x36, 0x08, 0x00 }, /* 0x7D  '}' */
    { 0x10, 0x08, 0x08, 0x10, 0x08 }, /* 0x7E  '~' */
};

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief  Transmit one byte over SPI1 using register-level polling.
 *         Caller must drive CS low before calling and high after.
 */
static void spi_write_byte(uint8_t data) {
    /* Wait until TXE (Tx buffer empty) */
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = data;
    /* Wait until not busy */
    while (SPI1->SR & SPI_SR_BSY);
}

/** @brief Send a command byte to the ILI9341 (DC low). */
static void ili9341_write_cmd(uint8_t cmd) {
    ILI9341_DC_CMD();
    ILI9341_CS_LOW();
    spi_write_byte(cmd);
    ILI9341_CS_HIGH();
}

/** @brief Send one data byte to the ILI9341 (DC high). */
static void ili9341_write_data8(uint8_t data) {
    ILI9341_DC_DATA();
    ILI9341_CS_LOW();
    spi_write_byte(data);
    ILI9341_CS_HIGH();
}

/** @brief Send a 16-bit data word (big-endian) to the ILI9341. */
static void ili9341_write_data16(uint16_t data) {
    ILI9341_DC_DATA();
    ILI9341_CS_LOW();
    spi_write_byte((uint8_t)(data >> 8));
    spi_write_byte((uint8_t)(data & 0xFF));
    ILI9341_CS_HIGH();
}

/**
 * @brief  Set the pixel address window (column × row range) for RAMWR.
 *         After calling this, send (x2-x1+1)*(y2-y1+1) 16-bit colour words.
 */
static void ili9341_set_window(uint16_t x1, uint16_t y1,
                                uint16_t x2, uint16_t y2) {
    ili9341_write_cmd(ILI9341_CMD_CASET);
    ili9341_write_data8((uint8_t)(x1 >> 8));
    ili9341_write_data8((uint8_t)(x1 & 0xFF));
    ili9341_write_data8((uint8_t)(x2 >> 8));
    ili9341_write_data8((uint8_t)(x2 & 0xFF));

    ili9341_write_cmd(ILI9341_CMD_PASET);
    ili9341_write_data8((uint8_t)(y1 >> 8));
    ili9341_write_data8((uint8_t)(y1 & 0xFF));
    ili9341_write_data8((uint8_t)(y2 >> 8));
    ili9341_write_data8((uint8_t)(y2 & 0xFF));

    ili9341_write_cmd(ILI9341_CMD_RAMWR);
}

/* ------------------------------------------------------------------ */
/*  Public: init                                                       */
/* ------------------------------------------------------------------ */

void ili9341_init(void) {

    /* --- Enable peripheral clocks --- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    /* SPI1 AF pins: SCK (PA5), MOSI (PA7), MISO (PA6) */
    gpio.Pin       = ILI9341_SCK_PIN | ILI9341_MOSI_PIN | ILI9341_MISO_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = ILI9341_SCK_AF;   /* GPIO_AF5_SPI1 */
    HAL_GPIO_Init(GPIOA, &gpio);

    /* CS (PB6) — software-controlled output, idle high */
    gpio.Pin   = ILI9341_CS_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(ILI9341_CS_PORT, &gpio);
    ILI9341_CS_HIGH();

    /* RESET (PA9) — output, idle high */
    gpio.Pin = ILI9341_RST_PIN;
    HAL_GPIO_Init(ILI9341_RST_PORT, &gpio);
    ILI9341_RST_HIGH();

    /* DC (PC7) — output */
    gpio.Pin = ILI9341_DC_PIN;
    HAL_GPIO_Init(ILI9341_DC_PORT, &gpio);
    ILI9341_DC_DATA();

    /* --- Configure SPI1 --- */
    /* CR1: master mode, CPOL=0, CPHA=0, MSB first, 8-bit, BR = fPCLK/4 */
    SPI1->CR1 = SPI_CR1_MSTR       /* Master mode                  */
              | SPI_CR1_SSM        /* Software slave management    */
              | SPI_CR1_SSI        /* Internal slave select high   */
              | (0 << SPI_CR1_BR_Pos);  /* fPCLK / 4 (~22.5 MHz @ 90 MHz APB2) */
    SPI1->CR2 = 0;
    SPI1->CR1 |= SPI_CR1_SPE;      /* Enable SPI                   */

    /* --- Hardware reset sequence --- */
    ILI9341_RST_HIGH();
    HAL_Delay(5);
    ILI9341_RST_LOW();
    HAL_Delay(20);
    ILI9341_RST_HIGH();
    HAL_Delay(150);

    /* --- ILI9341 initialisation sequence --- */
    ili9341_write_cmd(ILI9341_CMD_SWRESET);
    HAL_Delay(100);

    ili9341_write_cmd(0xEF);
    ili9341_write_data8(0x03);
    ili9341_write_data8(0x80);
    ili9341_write_data8(0x02);

    ili9341_write_cmd(0xCF);
    ili9341_write_data8(0x00);
    ili9341_write_data8(0xC1);
    ili9341_write_data8(0x30);

    ili9341_write_cmd(0xED);
    ili9341_write_data8(0x64);
    ili9341_write_data8(0x03);
    ili9341_write_data8(0x12);
    ili9341_write_data8(0x81);

    ili9341_write_cmd(0xE8);
    ili9341_write_data8(0x85);
    ili9341_write_data8(0x00);
    ili9341_write_data8(0x78);

    ili9341_write_cmd(0xCB);
    ili9341_write_data8(0x39);
    ili9341_write_data8(0x2C);
    ili9341_write_data8(0x00);
    ili9341_write_data8(0x34);
    ili9341_write_data8(0x02);

    ili9341_write_cmd(0xF7);
    ili9341_write_data8(0x20);

    ili9341_write_cmd(0xEA);
    ili9341_write_data8(0x00);
    ili9341_write_data8(0x00);

    /* Power control */
    ili9341_write_cmd(ILI9341_CMD_PWCTR1);
    ili9341_write_data8(0x23);

    ili9341_write_cmd(ILI9341_CMD_PWCTR2);
    ili9341_write_data8(0x10);

    /* VCOM control */
    ili9341_write_cmd(ILI9341_CMD_VMCTR1);
    ili9341_write_data8(0x3E);
    ili9341_write_data8(0x28);

    ili9341_write_cmd(ILI9341_CMD_VMCTR2);
    ili9341_write_data8(0x86);

    /* Memory access control — portrait orientation */
    ili9341_write_cmd(ILI9341_CMD_MADCTL);
    ili9341_write_data8(0x48);

    /* 16-bit colour (RGB565) */
    ili9341_write_cmd(ILI9341_CMD_COLMOD);
    ili9341_write_data8(0x55);

    /* Frame rate: ~60 Hz */
    ili9341_write_cmd(ILI9341_CMD_FRMCTR1);
    ili9341_write_data8(0x00);
    ili9341_write_data8(0x18);

    /* Display function control */
    ili9341_write_cmd(ILI9341_CMD_DFUNCTR);
    ili9341_write_data8(0x08);
    ili9341_write_data8(0x82);
    ili9341_write_data8(0x27);

    /* Gamma function disable */
    ili9341_write_cmd(0xF2);
    ili9341_write_data8(0x00);

    /* Gamma curve */
    ili9341_write_cmd(0x26);
    ili9341_write_data8(0x01);

    /* Positive gamma correction */
    ili9341_write_cmd(ILI9341_CMD_GMMACP);
    ili9341_write_data8(0x0F); ili9341_write_data8(0x31);
    ili9341_write_data8(0x2B); ili9341_write_data8(0x0C);
    ili9341_write_data8(0x0E); ili9341_write_data8(0x08);
    ili9341_write_data8(0x4E); ili9341_write_data8(0xF1);
    ili9341_write_data8(0x37); ili9341_write_data8(0x07);
    ili9341_write_data8(0x10); ili9341_write_data8(0x03);
    ili9341_write_data8(0x0E); ili9341_write_data8(0x09);
    ili9341_write_data8(0x00);

    /* Negative gamma correction */
    ili9341_write_cmd(ILI9341_CMD_GMMANP);
    ili9341_write_data8(0x00); ili9341_write_data8(0x0E);
    ili9341_write_data8(0x14); ili9341_write_data8(0x03);
    ili9341_write_data8(0x11); ili9341_write_data8(0x07);
    ili9341_write_data8(0x31); ili9341_write_data8(0xC1);
    ili9341_write_data8(0x48); ili9341_write_data8(0x08);
    ili9341_write_data8(0x0F); ili9341_write_data8(0x0C);
    ili9341_write_data8(0x31); ili9341_write_data8(0x36);
    ili9341_write_data8(0x0F);

    // 1. Enable DMA2 Clock
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    // 2. Configure DMA2 Stream 3 (Serves SPI1_TX)
    DMA2_Stream3->CR = 0; // Reset
    while (DMA2_Stream3->CR & DMA_SxCR_EN); // Ensure it's off

    DMA2_Stream3->PAR = (uint32_t)&(SPI1->DR);    // Point to SPI Data Register
    DMA2_Stream3->CR |= (3 << DMA_SxCR_CHSEL_Pos); // Select Channel 3
    DMA2_Stream3->CR |= DMA_SxCR_DIR_0;            // Memory-to-Peripheral
    DMA2_Stream3->CR |= DMA_SxCR_PL_1;             // High Priority
    DMA2_Stream3->CR |= DMA_SxCR_MSIZE_0;          // 16-bit Memory size
    DMA2_Stream3->CR |= DMA_SxCR_PSIZE_0;          // 16-bit Peripheral size
    // Note: We leave MINC (Memory Increment) off for a solid fill

    /* Exit sleep, then turn display on */
    ili9341_write_cmd(ILI9341_CMD_SLPOUT);
    HAL_Delay(120);
    ili9341_write_cmd(ILI9341_CMD_DISPON);
}



/* ------------------------------------------------------------------ */
/*  Public: draw filled rectangle                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief  Fill the rectangular region [x, x+width) × [y, y+height)
 *         with a solid RGB565 colour.
 *
 * @param  x      Top-left column
 * @param  y      Top-left row
 * @param  width  Width  in pixels  (clamped to display boundary)
 * @param  height Height in pixels  (clamped to display boundary)
 * @param  color  RGB565 fill colour
 */
void ili9341_draw_filled_rect(uint16_t x, uint16_t y,
                               uint16_t width, uint16_t height,
                               uint16_t color) {

    /* Boundary guard */
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT || width == 0 || height == 0)
        return;
    if (x + width  > ILI9341_WIDTH)  width  = ILI9341_WIDTH  - x;
    if (y + height > ILI9341_HEIGHT) height = ILI9341_HEIGHT - y;

    /* Set address window */
    ili9341_set_window(x, y, x + width - 1, y + height - 1);

    /* Stream pixel data — keep CS low for the entire burst */
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    ILI9341_DC_DATA();
    ILI9341_CS_LOW();

    uint32_t total = (uint32_t)width * height;
    for (uint32_t i = 0; i < total; i++) {
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = hi;
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = lo;
    }
    /* Wait for last transfer to finish before releasing CS */
    while (SPI1->SR & SPI_SR_BSY);

    ILI9341_CS_HIGH();
}

/* ------------------------------------------------------------------ */
/*  Public: draw character                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief  Render a single printable ASCII character (0x20–0x7E) at
 *         pixel position (x, y) using the embedded 5×7 font.
 *         Each cell is (FONT_WIDTH+1) × (FONT_HEIGHT+1) pixels to
 *         include a 1-pixel spacing column and row.
 *
 * @param  x      Top-left column of the character cell
 * @param  y      Top-left row    of the character cell
 * @param  c      Character to draw
 * @param  color  Foreground colour (RGB565)
 * @param  bg     Background colour (RGB565)
 */
void ili9341_draw_char(uint16_t x, uint16_t y, char c,
                        uint16_t color, uint16_t bg) {

    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
        return;

    /* Clamp to printable range */
    if (c < 0x20 || c > 0x7E)
        c = '?';

    const uint8_t *glyph = font5x7[c - 0x20];

    /* Cell dimensions include 1-pixel right/bottom padding */
    uint16_t cell_w = FONT_WIDTH + 1;
    uint16_t cell_h = FONT_HEIGHT + 1;

    ili9341_set_window(x, y,
                       x + cell_w - 1,
                       y + cell_h - 1);

    uint8_t fg_hi = (uint8_t)(color >> 8);
    uint8_t fg_lo = (uint8_t)(color & 0xFF);
    uint8_t bg_hi = (uint8_t)(bg >> 8);
    uint8_t bg_lo = (uint8_t)(bg & 0xFF);

    ILI9341_DC_DATA();
    ILI9341_CS_LOW();

    /* Iterate rows (bits) top→bottom, columns left→right */
    for (uint8_t row = 0; row < cell_h; row++) {
        for (uint8_t col = 0; col < cell_w; col++) {

            /* Padding column (right) or padding row (bottom) → background */
            uint8_t pixel_on = 0;
            if (row < FONT_HEIGHT && col < FONT_WIDTH) {
                pixel_on = (glyph[col] >> row) & 0x01;
            }

            uint8_t hi = pixel_on ? fg_hi : bg_hi;
            uint8_t lo = pixel_on ? fg_lo : bg_lo;

            while (!(SPI1->SR & SPI_SR_TXE));
            SPI1->DR = hi;
            while (!(SPI1->SR & SPI_SR_TXE));
            SPI1->DR = lo;
        }
    }
    while (SPI1->SR & SPI_SR_BSY);

    ILI9341_CS_HIGH();
}

/* ------------------------------------------------------------------ */
/*  Public: draw string                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief  Render a null-terminated ASCII string starting at (x, y).
 *         Characters advance left-to-right; the function stops at the
 *         right edge of the display without wrapping.
 *
 * @param  x      Starting column
 * @param  y      Starting row
 * @param  str    Null-terminated string
 * @param  color  Foreground colour (RGB565)
 * @param  bg     Background colour (RGB565)
 */
void ili9341_draw_string(uint16_t x, uint16_t y, const char *str,
                          uint16_t color, uint16_t bg) {

    if (!str) return;

    uint16_t cursor_x = x;
    uint16_t char_advance = FONT_WIDTH + 1;  /* includes 1-pixel spacing */

    while (*str) {
        if (cursor_x + char_advance > ILI9341_WIDTH)
            break;  /* No room for another character — stop */

        ili9341_draw_char(cursor_x, y, *str, color, bg);
        cursor_x += char_advance;
        str++;
    }
}

void ili9341_fill_dma(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // 1. Set the window as usual
    ili9341_set_window(x, y, x + w - 1, y + h - 1);

    ILI9341_DC_DATA();
    ILI9341_CS_LOW();

    // 2. Switch SPI to 16-bit mode (Instant speed boost)
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 |= SPI_CR1_DFF;  // 16-bit data frame
    SPI1->CR1 |= SPI_CR1_SPE;

    uint32_t total_pixels = (uint32_t)w * h;
    static uint16_t dma_color;
    dma_color = color; // Note: Ensure endianness matches your display setup

    // 3. DMA Transfer Loop (Chunked for > 65k pixels)
    while (total_pixels > 0) {
        uint16_t chunk = (total_pixels > 65535) ? 65535 : (uint16_t)total_pixels;

        // Disable DMA Stream to configure
        DMA2_Stream3->CR &= ~DMA_SxCR_EN;
        while (DMA2_Stream3->CR & DMA_SxCR_EN);

        // Clear all interrupt flags
        DMA2->LIFCR = DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3;

        // Configure DMA for SPI1 TX
        DMA2_Stream3->PAR = (uint32_t)&SPI1->DR;
        DMA2_Stream3->M0AR = (uint32_t)&dma_color;
        DMA2_Stream3->NDTR = chunk;

        // CR Settings: Channel 3, Memory-to-Peripheral, 16-bit size, NO Memory Increment
        DMA2_Stream3->CR = (3 << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_DIR_0 |
        DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 | DMA_SxCR_TCIE;

        SPI1->CR2 |= SPI_CR2_TXDMAEN; // Enable SPI DMA Request
        DMA2_Stream3->CR |= DMA_SxCR_EN; // Start DMA

        // Wait for Transfer Complete
        while (!(DMA2->LISR & DMA_LISR_TCIF3));
        total_pixels -= chunk;
    }

    // 4. Cleanup
    while (SPI1->SR & SPI_SR_BSY);
    ILI9341_CS_HIGH();

    // Revert SPI to 8-bit for commands/text
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 &= ~SPI_CR1_DFF;
    SPI1->CR1 |= SPI_CR1_SPE;
}

