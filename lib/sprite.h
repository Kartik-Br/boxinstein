#ifndef SPRITE_H
#define SPRITE_H

#include <stdbool.h>

#define S_GUY 0
#define S_SGUN 1

typedef struct {
    int x;
    int y;
    bool isAngled;
    int angle;
    int distanceToPlayer;
    int spriteType;
    bool isExist;
} Sprite;

/* Hands are separate: size represents Z-axis depth */
typedef struct {
    int x;
    int y;
    int z;
    int xScr;
    int yScr;
    int size;
    bool isExist;
    int distanceToPlayer;
} HandSprite;

typedef struct {
    int xPos;
    int yPos;
    int x_body;
    int y_body;
    int x_head;
    int y_head;
    int x_neck;
    int y_neck;
    int lastSize;
    bool dLeft;
    bool dRight;
    bool dDuck;
    /* Arms removed as requested */
} pSprite;

#endif
