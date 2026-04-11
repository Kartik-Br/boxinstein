#ifndef SPRITE_H
#define SPRITE_H

#include <stdbool.h>
#include <stdint.h>

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
    int16_t x;
    int16_t y;
    int16_t z;
    bool isHit;
    int16_t xScr;
    int16_t yScr;
    int16_t size;
    bool isExist;
    int16_t distanceToPlayer;
    int8_t blockCounter;

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
