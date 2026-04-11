#ifndef ENGINE
#define ENGINE
#include <math.h>
#include "render.h"
#include "config.h"

#define COL_BG      ILI9341_BLACK
#define COL_BORDER  ILI9341_WHITE
#define COL_RECT    ILI9341_YELLOW
#define COL_SPRITE  ILI9341_CYAN
#define COL_LINE    ILI9341_MAGENTA

float dist(float ax, float ay, float bx, float by, float ang);

float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite* sprites, int numSprites);

#endif
