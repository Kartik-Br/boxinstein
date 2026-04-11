#ifndef SPRITE_H
#define SPRITE_H

#include <stdbool.h>
#include <stdint.h> 
#define S_GUY 0
#define S_SGUN 1


typedef struct {int x;
    int y;
    bool isAngled;
    int angle;
    int distanceToPlayer;
    int spriteType;
    bool isExist;
} Sprite;


typedef struct {
    int x_body;
    int y_body;
    int x_head;
    int y_head;
    int x_neck;
    int y_neck;
    int x_larm;
    int y_larm;
    int x_rarm;
    int y_rarm;
    int x_lfist;
    int y_lfist;
    int x_rfist;
    int y_rfist;
    uint16_t lfist_sz;
    uint16_t rfist_sz;
} pSprite;

#endif