/**
 **************************************************************
 * File: mylib/ili9341.c
 * Brief: ILI9341 TFT LCD driver — optimised for STM32F429ZI
 *
 * Key optimisations vs the previous version
 * ------------------------------------------
 *
 * 1. SPI clock doubled to 45 MHz (BR=0, fPCLK/2)
 *    APB2 = 90 MHz on the F429 at 180 MHz sysclk.
 *    BR=0 gives 45 MHz — within the ILI9341's ~66 MHz write limit.
 *    This alone halves fill time from ~55 ms to ~27 ms.
 *
 * 2. 2-launch MINC=0 fill (replaces 320 per-row launches)
 *    DMA NDTR is 16-bit, max 65535 transfers.  A full screen is
 *    76800 pixels so it needs two launches (65535 + 11265).
 *    With MINC=0 the DMA reads the same single-pixel source
 *    address on every transfer — no buffer needed, no per-row
 *    setup cost, zero row-boundary stalls.
 *
 * 3. Async per-column DMA for raycasting
 *    ili9341_column_start_dma() sets the column address window,
 *    kicks off the DMA, and returns immediately.  The caller can
 *    raytrace the next column while this one is being sent.
 *    ili9341_column_wait() polls TCIF before the caller touches
 *    the buffer again.  With double-buffered column arrays the
 *    CPU and DMA run fully in parallel.
 *
 * Why double-buffering the fill doesn't help
 * -------------------------------------------
 * DMA setup overhead per row is ~111 ns.
 * One row DMA transfer takes ~171 us.
 * Overlap would save 0.065% — unmeasurable.
 * The wire IS the bottleneck for fills.
 * Double buffering only pays off when the CPU work per unit
 * is comparable to the DMA transfer time — which IS true for
 * per-column raycasting (see point 3).
 ***************************************************************
 */

#include "ili9341.h"

/* ------------------------------------------------------------------ */
/*  5x7 bitmap font (ASCII 0x20–0x7E)                                 */
/*  font5x7[c - 0x20][col]: bit 0 = top row, bit 6 = bottom row      */
/* ------------------------------------------------------------------ */
#define FONT_W  5
#define FONT_H  7

static const uint8_t font5x7[][FONT_W] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 0x20 ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* 0x21 '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* 0x22 '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 0x23 '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 0x24 '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* 0x25 '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* 0x26 '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* 0x27 ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 0x28 '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* 0x29 ')' */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 0x2A '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* 0x2B '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* 0x2C ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* 0x2D '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* 0x2E '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* 0x2F '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0x30 '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* 0x31 '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* 0x32 '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* 0x33 '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* 0x34 '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* 0x35 '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 0x36 '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* 0x37 '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* 0x38 '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* 0x39 '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* 0x3A ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* 0x3B ';' */
    {0x00,0x08,0x14,0x22,0x41}, /* 0x3C '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* 0x3D '=' */
    {0x41,0x22,0x14,0x08,0x00}, /* 0x3E '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* 0x3F '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* 0x40 '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 0x41 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 0x42 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 0x43 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 0x44 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 0x45 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 0x46 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 0x47 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 0x48 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 0x49 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 0x4A 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 0x4B 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 0x4C 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 0x4D 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 0x4E 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 0x4F 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 0x50 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 0x51 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 0x52 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 0x53 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 0x54 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 0x55 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 0x56 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 0x57 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 0x58 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 0x59 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 0x5A 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* 0x5B '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* 0x5C '\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* 0x5D ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* 0x5E '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* 0x5F '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* 0x60 '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 0x61 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 0x62 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 0x63 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 0x64 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 0x65 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 0x66 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 0x67 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 0x68 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 0x69 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 0x6A 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 0x6B 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 0x6C 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 0x6D 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 0x6E 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 0x6F 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 0x70 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 0x71 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 0x72 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 0x73 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 0x74 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 0x75 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 0x76 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 0x77 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 0x78 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 0x79 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 0x7A 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* 0x7B '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* 0x7C '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* 0x7D '}' */
    {0x10,0x08,0x08,0x10,0x08}, /* 0x7E '~' */
};

/* ------------------------------------------------------------------ */
/*  Internal state                                                     */
/* ------------------------------------------------------------------ */

/* Tracks whether an async column DMA is in flight */
static volatile uint8_t dma_busy = 0;

/* ------------------------------------------------------------------ */
/*  SPI helpers (8-bit, used for commands and init)                   */
/* ------------------------------------------------------------------ */

static inline void spi_write8(uint8_t b) {
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = b;
    while (SPI1->SR & SPI_SR_BSY);
}

static void write_cmd(uint8_t cmd) {
    ILI9341_DC_CMD();
    ILI9341_CS_LOW();
    spi_write8(cmd);
    ILI9341_CS_HIGH();
}

static void write_data8(uint8_t d) {
    ILI9341_DC_DATA();
    ILI9341_CS_LOW();
    spi_write8(d);
    ILI9341_CS_HIGH();
}

/* ------------------------------------------------------------------ */
/*  SPI frame-width switching                                         */
/*  Must only be called when SPI is idle (not mid-transfer)           */
/* ------------------------------------------------------------------ */

static inline void spi_16bit(void) {
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 |=  SPI_CR1_DFF;
    SPI1->CR1 |=  SPI_CR1_SPE;
}

static inline void spi_8bit(void) {
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 &= ~SPI_CR1_DFF;
    SPI1->CR1 |=  SPI_CR1_SPE;
}

/* ------------------------------------------------------------------ */
/*  Address window                                                     */
/* ------------------------------------------------------------------ */

static void set_window(uint16_t x1, uint16_t y1,
                        uint16_t x2, uint16_t y2) {
    write_cmd(ILI9341_CMD_CASET);
    write_data8((uint8_t)(x1 >> 8)); write_data8((uint8_t)x1);
    write_data8((uint8_t)(x2 >> 8)); write_data8((uint8_t)x2);

    write_cmd(ILI9341_CMD_PASET);
    write_data8((uint8_t)(y1 >> 8)); write_data8((uint8_t)y1);
    write_data8((uint8_t)(y2 >> 8)); write_data8((uint8_t)y2);

    write_cmd(ILI9341_CMD_RAMWR);
}

/* ------------------------------------------------------------------ */
/*  Core DMA launch                                                    */
/*                                                                     */
/*  Sends n_pixels 16-bit words from src to SPI1->DR.                */
/*  SPI1 must already be in 16-bit mode.                              */
/*  CS must be LOW, DC must be DATA.                                  */
/*  n_pixels <= 65535.                                                */
/*                                                                     */
/*  minc=0: DMA re-reads the same address on every transfer.          */
/*          Used for solid fills — src is a single uint16_t.         */
/*  minc=1: DMA increments src address per transfer.                  */
/*          Used for column rendering — src is uint16_t[].            */
/*                                                                     */
/*  blocking=1: poll TCIF and wait before returning.                  */
/*  blocking=0: return immediately after enabling the stream.         */
/* ------------------------------------------------------------------ */

static void dma_launch(const uint16_t *src, uint16_t n_pixels,
                        uint8_t minc, uint8_t blocking) {

    DMA_Stream_TypeDef *s = ILI9341_DMA_STREAM;

    /* Disable stream and wait */
    s->CR &= ~DMA_SxCR_EN;
    while (s->CR & DMA_SxCR_EN);

    /* Clear all stream-3 flags */
    ILI9341_DMA->LIFCR = ILI9341_DMA_CLR;

    s->PAR  = (uint32_t)&SPI1->DR;
    s->M0AR = (uint32_t)src;
    s->NDTR = n_pixels;

    /* FIFO mode, full threshold: DMA bursts into the FIFO before
     * SPI reads it, maximising bus efficiency */
    s->FCR  = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH;

    s->CR   = ILI9341_DMA_CHANNEL    /* CH3 = SPI1_TX              */
            | DMA_SxCR_DIR_0         /* memory → peripheral        */
            | (minc ? DMA_SxCR_MINC : 0u)  /* increment or fixed  */
            | DMA_SxCR_MSIZE_0       /* memory width:  16-bit      */
            | DMA_SxCR_PSIZE_0       /* periph width:  16-bit      */
            | DMA_SxCR_PL_1;         /* priority: high             */

    SPI1->CR2 |= SPI_CR2_TXDMAEN;
    s->CR     |= DMA_SxCR_EN;

    if (blocking) {
        while (!(ILI9341_DMA->LISR & ILI9341_DMA_TCIF));
        while (SPI1->SR & SPI_SR_BSY);
        SPI1->CR2 &= ~SPI_CR2_TXDMAEN;
        s->CR     &= ~DMA_SxCR_EN;
        ILI9341_DMA->LIFCR = ILI9341_DMA_CLR;
    }
    /* non-blocking: caller must call ili9341_column_wait() later */
}

/* ------------------------------------------------------------------ */
/*  Solid-colour fill via 2-launch MINC=0 DMA                        */
/*                                                                     */
/*  A full 240x320 screen = 76800 pixels > 65535 (NDTR max).         */
/*  We split into two launches from the same single-pixel source.     */
/*  MINC=0 means the DMA re-reads &pixel on every transfer — no       */
/*  buffer needed, and zero overhead between the two launches.        */
/* ------------------------------------------------------------------ */

#define DMA_MAX_PIXELS  65535u

static void fill_dma(uint16_t width, uint16_t height, uint16_t color) {

    /* Byte-swap once: SPI sends MSB first, F4 is little-endian */
    static uint16_t pixel;
    pixel = ILI9341_SWAP(color);

    ILI9341_DC_DATA();
    ILI9341_CS_LOW();
    spi_16bit();

    uint32_t total = (uint32_t)width * height;

    if (total <= DMA_MAX_PIXELS) {
        dma_launch(&pixel, (uint16_t)total, 0 /*MINC=0*/, 1 /*blocking*/);
    } else {
        /* Two launches */
        dma_launch(&pixel, DMA_MAX_PIXELS, 0, 1);
        dma_launch(&pixel, (uint16_t)(total - DMA_MAX_PIXELS), 0, 1);
    }

    spi_8bit();
    ILI9341_CS_HIGH();
}

/* ------------------------------------------------------------------ */
/*  Public: ili9341_init                                               */
/* ------------------------------------------------------------------ */

void ili9341_init(void) {

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* SPI1 AF: SCK (PA5), MISO (PA6), MOSI (PA7) */
    g.Pin       = ILI9341_SCK_PIN | ILI9341_MISO_PIN | ILI9341_MOSI_PIN;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = ILI9341_SPI_AF;
    HAL_GPIO_Init(GPIOA, &g);

    /* CS (PB6) — output, idle high */
    g.Pin = ILI9341_CS_PIN; g.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(ILI9341_CS_PORT, &g);
    ILI9341_CS_HIGH();

    /* RST (PA9) — output, idle high */
    g.Pin = ILI9341_RST_PIN;
    HAL_GPIO_Init(ILI9341_RST_PORT, &g);
    ILI9341_RST_HIGH();

    /* DC (PC7) — output */
    g.Pin = ILI9341_DC_PIN;
    HAL_GPIO_Init(ILI9341_DC_PORT, &g);
    ILI9341_DC_DATA();

    /*
     * SPI1: master, CPOL=0 CPHA=0, 8-bit, MSB first, software NSS.
     * BR=0 => fPCLK/2 = 45 MHz  (APB2 = 90 MHz on F429 @ 180 MHz sysclk)
     * This is double the previous /4 speed and within the ILI9341
     * write limit of ~66 MHz.
     */
    SPI1->CR1 = SPI_CR1_MSTR
              | SPI_CR1_SSM
              | SPI_CR1_SSI;
              /* BR[2:0] = 000 => /2, no bits to set */
    SPI1->CR2 = 0;
    SPI1->CR1 |= SPI_CR1_SPE;

    /* Hardware reset */
    ILI9341_RST_HIGH(); HAL_Delay(5);
    ILI9341_RST_LOW();  HAL_Delay(20);
    ILI9341_RST_HIGH(); HAL_Delay(150);

    /* ILI9341 power-on sequence */
    write_cmd(ILI9341_CMD_SWRESET); HAL_Delay(100);

    write_cmd(0xEF);
    write_data8(0x03); write_data8(0x80); write_data8(0x02);
    write_cmd(0xCF);
    write_data8(0x00); write_data8(0xC1); write_data8(0x30);
    write_cmd(0xED);
    write_data8(0x64); write_data8(0x03); write_data8(0x12); write_data8(0x81);
    write_cmd(0xE8);
    write_data8(0x85); write_data8(0x00); write_data8(0x78);
    write_cmd(0xCB);
    write_data8(0x39); write_data8(0x2C); write_data8(0x00);
    write_data8(0x34); write_data8(0x02);
    write_cmd(0xF7);  write_data8(0x20);
    write_cmd(0xEA);  write_data8(0x00); write_data8(0x00);

    write_cmd(ILI9341_CMD_PWCTR1); write_data8(0x23);
    write_cmd(ILI9341_CMD_PWCTR2); write_data8(0x10);
    write_cmd(ILI9341_CMD_VMCTR1); write_data8(0x3E); write_data8(0x28);
    write_cmd(ILI9341_CMD_VMCTR2); write_data8(0x86);
    write_cmd(ILI9341_CMD_MADCTL); write_data8(0x48);   /* portrait */
    write_cmd(ILI9341_CMD_COLMOD); write_data8(0x55);   /* RGB565 */
    write_cmd(ILI9341_CMD_FRMCTR1); write_data8(0x00); write_data8(0x18);
    write_cmd(ILI9341_CMD_DFUNCTR);
    write_data8(0x08); write_data8(0x82); write_data8(0x27);
    write_cmd(0xF2); write_data8(0x00);
    write_cmd(0x26);  write_data8(0x01);

    write_cmd(ILI9341_CMD_GMMACP);
    write_data8(0x0F); write_data8(0x31); write_data8(0x2B); write_data8(0x0C);
    write_data8(0x0E); write_data8(0x08); write_data8(0x4E); write_data8(0xF1);
    write_data8(0x37); write_data8(0x07); write_data8(0x10); write_data8(0x03);
    write_data8(0x0E); write_data8(0x09); write_data8(0x00);

    write_cmd(ILI9341_CMD_GMMANP);
    write_data8(0x00); write_data8(0x0E); write_data8(0x14); write_data8(0x03);
    write_data8(0x11); write_data8(0x07); write_data8(0x31); write_data8(0xC1);
    write_data8(0x48); write_data8(0x08); write_data8(0x0F); write_data8(0x0C);
    write_data8(0x31); write_data8(0x36); write_data8(0x0F);

    write_cmd(ILI9341_CMD_SLPOUT); HAL_Delay(120);
    write_cmd(ILI9341_CMD_DISPON);
}

/* ------------------------------------------------------------------ */
/*  Public: ili9341_fill_screen                                        */
/* ------------------------------------------------------------------ */

void ili9341_fill_screen(uint16_t color) {
    set_window(0, 0, ILI9341_WIDTH - 1u, ILI9341_HEIGHT - 1u);
    fill_dma(ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

/* ------------------------------------------------------------------ */
/*  Public: ili9341_draw_filled_rect                                   */
/* ------------------------------------------------------------------ */

void ili9341_draw_filled_rect(uint16_t x, uint16_t y,
                               uint16_t width, uint16_t height,
                               uint16_t color) {
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT || !width || !height)
        return;
    if (x + width  > ILI9341_WIDTH)  width  = (uint16_t)(ILI9341_WIDTH  - x);
    if (y + height > ILI9341_HEIGHT) height = (uint16_t)(ILI9341_HEIGHT - y);

    set_window(x, y, (uint16_t)(x + width - 1u), (uint16_t)(y + height - 1u));
    fill_dma(width, height, color);
}

/* ------------------------------------------------------------------ */
/*  Public: ili9341_draw_char                                          */
/* ------------------------------------------------------------------ */

void ili9341_draw_char(uint16_t x, uint16_t y, char c,
                        uint16_t color, uint16_t bg) {
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT) return;
    if (c < 0x20 || c > 0x7E) c = '?';

    const uint8_t *glyph = font5x7[c - 0x20];
    const uint16_t cw = FONT_W + 1;
    const uint16_t ch = FONT_H + 1;

    /* Build each row into a small stack buffer and DMA it */
    uint16_t row_buf[FONT_W + 1];
    uint16_t fg_sw = ILI9341_SWAP(color);
    uint16_t bg_sw = ILI9341_SWAP(bg);

    set_window(x, y, (uint16_t)(x + cw - 1u), (uint16_t)(y + ch - 1u));

    ILI9341_DC_DATA();
    ILI9341_CS_LOW();
    spi_16bit();

    for (uint8_t row = 0; row < ch; row++) {
        for (uint8_t col = 0; col < cw; col++) {
            uint8_t on = (row < FONT_H && col < FONT_W)
                       ? ((glyph[col] >> row) & 1u) : 0u;
            row_buf[col] = on ? fg_sw : bg_sw;
        }
        dma_launch(row_buf, cw, 1 /*MINC=1*/, 1 /*blocking*/);
    }

    spi_8bit();
    ILI9341_CS_HIGH();
}

/* ------------------------------------------------------------------ */
/*  Public: ili9341_draw_string                                        */
/* ------------------------------------------------------------------ */

void ili9341_draw_string(uint16_t x, uint16_t y, const char *str,
                          uint16_t color, uint16_t bg) {
    if (!str) return;
    uint16_t cx = x;
    while (*str) {
        if (cx + FONT_W + 1u > ILI9341_WIDTH) break;
        ili9341_draw_char(cx, y, *str++, color, bg);
        cx = (uint16_t)(cx + FONT_W + 1u);
    }
}

/* ------------------------------------------------------------------ */
/*  Public: async column DMA (for raycasting)                         */
/* ------------------------------------------------------------------ */

void ili9341_column_wait(void) {
    if (!dma_busy) return;

    /* Wait for Transfer Complete on stream 3 */
    while (!(ILI9341_DMA->LISR & ILI9341_DMA_TCIF));

    /* Drain the SPI shift register */
    while (SPI1->SR & SPI_SR_BSY);

    /* Teardown */
    SPI1->CR2                &= ~SPI_CR2_TXDMAEN;
    ILI9341_DMA_STREAM->CR  &= ~DMA_SxCR_EN;
    ILI9341_DMA->LIFCR       = ILI9341_DMA_CLR;

    spi_8bit();
    ILI9341_CS_HIGH();

    dma_busy = 0;
}

void ili9341_column_start_dma(uint16_t x, const uint16_t *buf) {

    /* Set column address window: x..x, rows 0..319 */
    set_window(x, 0u, x, ILI9341_HEIGHT - 1u);

    ILI9341_DC_DATA();
    ILI9341_CS_LOW();
    spi_16bit();

    dma_busy = 1;

    /* Non-blocking: returns immediately after enabling DMA */
    dma_launch(buf, ILI9341_HEIGHT, 1 /*MINC=1*/, 0 /*non-blocking*/);
}
