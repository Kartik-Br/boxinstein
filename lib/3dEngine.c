#include "3dEngine.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ili9341.h"

#define FOV 70.0f
#define FOV_RAD (FOV * (M_PI / 180.f))

#define MAP_WIDTH 20
#define MAP_HEIGHT 20

int compar(const void* a, const void* b)
{
    HandSprite* aS = (HandSprite*) a;
    HandSprite* bS = (HandSprite*) b;
    if (aS->distanceToPlayer > bS->distanceToPlayer) {
        return -1;
    } else if (aS->distanceToPlayer < bS->distanceToPlayer) {
        return 1;
    } else {
        return 0;
    }
}

float dist(float ax, float ay, float bx, float by, float ang) {
    return (sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay)));
}

float cast_ray(uint8_t (*map)[20], Player_info* player, float angle, int col, int rows)
{
    return 10000.0f;
    const int MAP_W = MAP_WIDTH, MAP_H = MAP_HEIGHT;
    const float TILE = 64.0f;
    const float EPS = 0.0001f;

    int mapX, mapY;
    float rayX, rayY, xOffset, yOffset;
    int colourH = 0, colourV = 0;

    // Start with "no hit" distances very large
    float disH = 1e30f, hx = player->x, hy = player->y;
    float disV = 1e30f, vx = player->x, vy = player->y;

    // Angle in radians, normalized
    float rayAngle = angle * (M_PI / 180.0f);
    while (rayAngle >  M_PI) rayAngle -= 2.0f * M_PI;
    while (rayAngle <= -M_PI) rayAngle += 2.0f * M_PI;

    // -------- HORIZONTAL grid intersections --------
    float aTan = -1.0f / tanf(rayAngle == 0.0f ? 1e-6f : rayAngle);
    if (rayAngle < 0.0f) { // looking up
        rayY = (((int)player->y >> 6) << 6) - EPS;
        rayX = (player->y - rayY) * aTan + player->x;
        yOffset = -TILE;
        xOffset = -yOffset * aTan;
    } else if (rayAngle > 0.0f) { // looking down
        rayY = (((int)player->y >> 6) << 6) + TILE;
        rayX = (player->y - rayY) * aTan + player->x;
        yOffset = TILE;
        xOffset = -yOffset * aTan;
    } else { // exactly horizontal (no horizontal crossings)
        xOffset = yOffset = 0.0f;
    }

    for (int d = 0; d < 100; ++d) {
        mapX = (int)floorf(rayX / TILE);
        mapY = (int)floorf(rayY / TILE);
        if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H) break;
        if (map[mapY][mapX] > 0) {
            hx = rayX; hy = rayY;
            disH = dist(player->x, player->y, hx, hy, 0.0f);
            colourH = map[mapY][mapX];
            break;
        }
        rayX += xOffset; rayY += yOffset;
    }

    // -------- VERTICAL grid intersections --------
    float nTan = -tanf(rayAngle);
    if (rayAngle > -M_PI/2 && rayAngle < M_PI/2) { // looking right
        rayX = (((int)player->x >> 6) << 6) + TILE;
        rayY = (player->x - rayX) * nTan + player->y;
        xOffset = TILE;
        yOffset = -xOffset * nTan;
    } else if (rayAngle < -M_PI/2 || rayAngle > M_PI/2) { // looking left
        rayX = (((int)player->x >> 6) << 6) - EPS;
        rayY = (player->x - rayX) * nTan + player->y;
        xOffset = -TILE;
        yOffset = -xOffset * nTan;
    } else { // exactly vertical (no vertical crossings)
        xOffset = yOffset = 0.0f;
    }

    for (int d = 0; d < 100; ++d) {
        mapX = (int)floorf(rayX / TILE);
        mapY = (int)floorf(rayY / TILE);
        if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H) break;
        if (map[mapY][mapX] > 0) {
            vx = rayX; vy = rayY;
            disV = dist(player->x, player->y, vx, vy, 0.0f);
            colourV = map[mapY][mapX];
            break;
        }
        rayX += xOffset; rayY += yOffset;
    }

    // Pick the nearer hit AND its colour
    float disT, disX, disY;
    int texOffset;
    bool darker;
    if (disV < disH) {
	disX = vx;
	disY = vy;
        disT = disV;
	texOffset = ((int)disY % 64) / 2;
	darker = true;
    } else {
	disX = hx;
	disY = hy;
        disT = disH;
	texOffset = ((int)disX % 64) / 2;
	darker = false;
    }
    //debug_print("texOffset %d\n", texOffset);

    // Fish-eye correction
    float radPAngle = player->angle * (M_PI / 180.0f);
    float cAngle = radPAngle - rayAngle;
    while (cAngle >  M_PI) cAngle -= 2.0f * M_PI;
    while (cAngle < -M_PI) cAngle += 2.0f * M_PI;
    disT *= cosf(cAngle);

    // Project and draw
    float denom = (disT > 1e-6f) ? disT : 1e-6f;
    int lineH = (int)fabsf((TILE * rows) / denom);
    //float scaleFactor = (float)lineH / 32.0f;
    float scaleFactor = 32.0f / (float)lineH;
    float scaleOffset = 0;
    if (lineH > rows) {
	scaleOffset = (lineH - rows) / 2.0;
	lineH = rows;
    }

    int drawX = col;
    int lineOffset = (rows / 2) - (lineH >> 1);

    //render_line(drawX, lineOffset, lineH, '#', texOffset % 5 + 1);
    //render_line_texture(drawX, lineOffset, lineH, texOffset, 1, darker, scaleFactor, scaleOffset);
    //render_rect(); //NEED TO ADD PARAMS



    return disT;
}

#define M_PI 3.1415926535f

// Fast atan2 approximation (max error ~0.005 radians)
float fast_atan2(float y, float x) {
    float abs_y = fabsf(y) + 1e-10f; 
    float r, angle;
    if (x >= 0) {
        r = (x - abs_y) / (x + abs_y);
        angle = (0.1963f * r * r - 0.9817f) * r + (M_PI / 4.0f);
    } else {
        r = (x + abs_y) / (abs_y - x);
        angle = (0.1963f * r * r - 0.9817f) * r + (3.0f * M_PI / 4.0f);
    }
    return (y < 0) ? -angle : angle;
}

// Fast sine approximation using a parabola (Bhaskara I's approximation)
float fast_sin(float x) {
    // Wrap x to [-PI, PI]
    while (x > M_PI) x -= 2.0f * M_PI;
    while (x < -M_PI) x += 2.0f * M_PI;
    
    float sin_val = 1.27323954f * x + (x < 0 ? 0.405284735f : -0.405284735f) * x * x;
    return 0.225f * (sin_val * fabsf(sin_val) - sin_val) + sin_val;
}

// Cosine is just sine with an offset
float fast_cos(float x) {
    return fast_sin(x + (M_PI / 2.0f));
}

// Fast tan for FOV calculations
float fast_tan(float x) {
    // Very simple approximation for small angles (useful for FOV < 90 deg)
    // tan(x) ≈ x + x^3/3
    return x * (1.0f + 0.333333f * x * x);
}

//V7
float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite* sprites, int numSprites, pSprite* opSprite)
{
    // Pre-calculate constants once
    static const float DEG_TO_RAD = M_PI / 180.0f;
    static const float HALF_FOV_RAD = 35.0f * (M_PI / 180.0f);
    static const float TAN_HALF_FOV = 0.7002f; // tan(35 degrees) pre-calculated
    static const float TWO_PI = 2.0f * M_PI;

    float playerRad = player->angle * DEG_TO_RAD;
    float pCos = fast_cos(playerRad);
    float pSin = fast_sin(playerRad);

    // --- STEP 1: ERASE OLD POSITIONS ---
    render_erase_psprite(opSprite, COL_BG); 
    for (int i = 0; i < numSprites; i++) {
        if (sprites[i].isExist) render_erase_hand(&sprites[i], COL_BG);
    }

    // --- STEP 2: CALCULATE PERSON (opSprite) ---
    float dxP = (float)opSprite->xPos - (float)player->x;
    float dyP = (float)opSprite->yPos - (float)player->y;
    
    // Calculate relative angle
    float spriteAngleRadP = fast_atan2(dyP, dxP);
    float angleDiffRadP = spriteAngleRadP - playerRad;
    
    // Faster normalization
    if (angleDiffRadP > M_PI) angleDiffRadP -= TWO_PI;
    if (angleDiffRadP < -M_PI) angleDiffRadP += TWO_PI;

    if (fabsf(angleDiffRadP) < HALF_FOV_RAD) {
        // Dot product for distance (Corrects fish-eye naturally)
        float pDist = dxP * pCos + dyP * pSin;
        
        if (pDist > 10.0f) { 
            float invDist = 1.0f / pDist; // One division only
            float screenXOffsetP = fast_tan(angleDiffRadP) / TAN_HALF_FOV;
            int targetXP = (int)((1.0f + screenXOffsetP) * 0.5f * (float)cols);
            int baseSize = (int)((64.0f * (float)rows) * invDist);

            opSprite->x_head = targetXP - (baseSize >> 2); // Shift instead of /4
            opSprite->y_head = (rows >> 1) - (baseSize >> 1);
            opSprite->x_neck = targetXP - (baseSize >> 3); // Shift instead of /8
            opSprite->y_neck = opSprite->y_head + (baseSize >> 1);
            opSprite->x_body = targetXP - (baseSize >> 1);
            opSprite->y_body = opSprite->y_neck + (baseSize >> 2);
            
            opSprite->lastSize = baseSize; 

            render_draw_rect_outline(opSprite->x_head, opSprite->y_head, baseSize >> 1, baseSize >> 1, ILI9341_WHITE);
            render_draw_rect_outline(opSprite->x_neck, opSprite->y_neck, baseSize >> 2, baseSize >> 2, ILI9341_RED);
            render_draw_rect_outline(opSprite->x_body, opSprite->y_body, baseSize, rows - opSprite->y_body, ILI9341_BLUE);
        } else {
            opSprite->lastSize = 0;
        }
    } else {
        opSprite->lastSize = 0;
    }

    // --- STEP 3: DRAW HANDS SECOND ---
    for (int i = 0; i < numSprites; i++) {
        HandSprite* s = &sprites[i];
        if (!s->isExist) continue;

        float dx = (float)s->x - (float)player->x;
        float dy = (float)s->y - (float)player->y;
        float sDist = dx * pCos + dy * pSin;
        
        float sAngle = fast_atan2(dy, dx) - playerRad;
        if (sAngle > M_PI) sAngle -= TWO_PI;
        if (sAngle < -M_PI) sAngle += TWO_PI;

        // Use fabsf check for FOV instead of cosf(sAngle) > 0 for speed
        if (fabsf(sAngle) < HALF_FOV_RAD && sDist > 10.0f) {
            float invSDist = 1.0f / sDist;
            s->xScr = (int)((1.0f + (fast_tan(sAngle) / TAN_HALF_FOV)) * 0.5f * (float)cols);
            s->yScr = (rows >> 1) + s->z;
            s->size = (int)((64.0f * (float)rows) * invSDist);
            s->size /= 2;
            render_draw_hand(s, ILI9341_YELLOW);
        }
    }
    return 0.0f;
}

//V6
//float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite* sprites, int numSprites, pSprite* opSprite)
//{
//    float playerRad = player->angle * (M_PI / 180.0f);
//    float halfFovRad = (35.0f) * (M_PI / 180.0f); 
//    float tanHalfFov = tanf(halfFovRad);
//
//    // --- STEP 1: ERASE OLD POSITIONS ---
//    // Only erase if they were drawn in the previous frame
//    render_erase_psprite(opSprite, COL_BG); 
//    for (int i = 0; i < numSprites; i++) {
//        if (sprites[i].isExist) render_erase_hand(&sprites[i], COL_BG);
//    }
//
//    // --- STEP 2: CALCULATE PERSON (opSprite) ---
//    float dxP = (float)opSprite->xPos - (float)player->x;
//    float dyP = (float)opSprite->yPos - (float)player->y;
//    float spriteAngleRadP = atan2f(dyP, dxP);
//    float angleDiffRadP = spriteAngleRadP - playerRad;
//    while (angleDiffRadP >  M_PI) angleDiffRadP -= 2.0f * M_PI;
//    while (angleDiffRadP < -M_PI) angleDiffRadP += 2.0f * M_PI;
//
//    // Is it in the 70-degree FOV?
//    if (fabsf(angleDiffRadP) < halfFovRad) {
//        float pDist = dxP * cosf(playerRad) + dyP * sinf(playerRad);
//        if (pDist > 10.0f) { // Distance check to avoid huge sprites
//            float screenXOffsetP = tanf(angleDiffRadP) / tanHalfFov;
//            int targetXP = (int)((1.0f + screenXOffsetP) * 0.5f * (float)cols);
//            int baseSize = (int)((64.0f * (float)rows) / pDist);
//
//            // Update NEW positions
//            opSprite->x_head = targetXP - (baseSize / 4);
//            opSprite->y_head = (rows / 2) - (baseSize / 2);
//            opSprite->x_neck = targetXP - (baseSize / 8);
//            opSprite->y_neck = opSprite->y_head + (baseSize / 2);
//            opSprite->x_body = targetXP - (baseSize / 2);
//            opSprite->y_body = opSprite->y_neck + (baseSize / 4);
//            
//            // Save size for next frame's erasure
//            opSprite->lastSize = baseSize; 
//
//            // Draw Person FIRST
//            render_draw_rect_outline(opSprite->x_head, opSprite->y_head, baseSize/2, baseSize/2, ILI9341_WHITE);
//            render_draw_rect_outline(opSprite->x_neck, opSprite->y_neck, baseSize/4, baseSize/4, ILI9341_RED);
//            render_draw_rect_outline(opSprite->x_body, opSprite->y_body, baseSize, rows - opSprite->y_body, ILI9341_BLUE);
//        } else {
//            opSprite->lastSize = 0; // Too close, don't draw/erase
//        }
//    } else {
//        opSprite->lastSize = 0; // Off-screen, don't draw/erase
//    }
//
//    // --- STEP 3: DRAW HANDS SECOND (Always on top) ---
//    for (int i = 0; i < numSprites; i++) {
//        HandSprite* s = &sprites[i];
//        if (!s->isExist) continue;
//
//        float dx = (float)s->x - (float)player->x;
//        float dy = (float)s->y - (float)player->y;
//        float sDist = dx * cosf(playerRad) + dy * sinf(playerRad);
//        
//        float sAngle = atan2f(dy, dx) - playerRad;
//        while (sAngle >  M_PI) sAngle -= 2.0f * M_PI;
//        while (sAngle < -M_PI) sAngle += 2.0f * M_PI;
//
//        if (cosf(sAngle) > 0 && sDist > 10.0f) {
//            s->xScr = (int)((1.0f + (tanf(sAngle)/tanHalfFov)) * 0.5f * (float)cols);
//            s->yScr = rows / 2 + s->z;
//            s->size = (int)((64.0f * (float)rows) / sDist);
//            render_draw_hand(s, ILI9341_YELLOW);
//        }
//    }
//    return 0.0f;
//}

