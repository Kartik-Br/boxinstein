#ifndef SPRITE_H
#define SPRITE_H

#include <stdbool.h>

#define S_GUY 0
#define S_SGUN 1
#define S_FISTLP 1
#define S_FISTRP 2
#define S_FISTLO 3
#define S_FISTRO 4



typedef struct {
    int x;
    int y;
    bool isAngled;
    int angle;
    int distanceToPlayer;
    int spriteType;
    bool isExist;
} Sprite;

#endif
