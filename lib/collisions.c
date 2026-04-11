#include "collisions.h"

bool check_collide_block(HandSprite* hand, HandSprite hands[4])
{
    if (hand->blockCounter > 0) {
        hand->blockCounter--;
        return false;
    }
    if (hand->y < hands[2].y) {
        //hand has passed, check for collision
        if ((hand->x - hand->size/2) < (hands[2].x + hand->size/2) && (hand->x + hand->size/2) > (hands[2].x - hand->size/2)) {
            if ((hand->z - hand->size/2) < (hands[2].z + hand->size/2) && (hand->z + hand->size/2) > (hands[2].z - hand->size/2)) {
                //collision, is blocked.
                hand->blockCounter = 30;
                return true;
            }
        }
    }
    if (hand->y < hands[3].y) {
        //hand has passed, check for collision
        if ((hand->x - hand->size/2) < (hands[3].x + hand->size/2) && (hand->x + hand->size/2) > (hands[3].x - hand->size/2)) {
            if ((hand->z - hand->size/2) < (hands[3].z + hand->size/2) && (hand->z + hand->size/2) > (hands[3].z - hand->size/2)) {
                //collision, is blocked.
                hand->blockCounter = 30;
                return true;
            }
        }
    }
    return false;
}

bool check_collide_head(HandSprite* hand, pSprite* opSprite)
{
    if (hand->blockCounter > 0) {
        return false;
    }
    if (hand->y < opSprite->yPos) {
        //hand has passed, check for collision
        if (hand->z < 20 && hand->z > -70) {
            if ((hand->x - hand->size/2) < (opSprite->xPos + hand->size/2) && (hand->x + hand->size/2) > (opSprite->xPos - hand->size/2)) {
                return true;
            }
        }
    }
    return false;
}

bool check_collide_body(HandSprite* hand, pSprite* opSprite)
{
    if (hand->blockCounter > 0) {
        return false;
    }
    if (hand->y < opSprite->yPos) {
        //hand has passed, check for collision
        if (hand->z > 50 && hand->z < 130) {
            if ((hand->x - hand->size/2) > (opSprite->xPos - hand->size) && (hand->x + hand->size/2) < (opSprite->xPos + hand->size)) {
                return true;
            }
        }
    }
    return false;
}
