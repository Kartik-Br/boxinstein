#ifndef ENGINE
#define ENGINE
#include <math.h>
#include "render.h"

float dist(float ax, float ay, float bx, float by, float ang);

float draw_all_stuff(int (*map)[20], Player_info* player, int cols, int rows, Sprite** sprites, int numSprites);

#endif
