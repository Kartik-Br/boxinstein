#ifndef SPRITE_H
#define SPRITE_H

#include <stdbool.h>

#define S_GUY  0
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

typedef struct {
    int x_body;
    int y_body;   /* was y_boy (typo) — fixed */
    int x_head;
    int y_head;
    int x_neck;
    int y_neck;
} pSprite;

/*
 * HandSprite — a square centred on a screen position.
 * xScr/yScr are screen-space pixel coordinates.
 * size is the square's side length in pixels.
 *
 * If you compile with -DHAND_USE_XY the render functions will
 * read h->x and h->y instead of h->xScr and h->yScr.
 */
typedef struct {
    int   xScr;
    int   yScr;
    int x;
    int y;
    int distanceToPlayer;
    int   size;
    bool  isExist;
} HandSprite;

#endif /* SPRITE_H */
