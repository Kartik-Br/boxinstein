#ifndef COLLISIONS_H
#define COLLISIONS_H

#include <stdbool.h>
#include "sprite.h"

bool check_collide_block(HandSprite* hand, HandSprite hands[4]);

bool check_collide_head(HandSprite* hand, pSprite* opSprite);

bool check_collide_body(HandSprite* hand, pSprite* opSprite);

#endif
