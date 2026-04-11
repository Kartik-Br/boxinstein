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

//V6
float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite* sprites, int numSprites, pSprite* opSprite)
{
    float playerRad = player->angle * (M_PI / 180.0f);
    float halfFovRad = (35.0f) * (M_PI / 180.0f); 
    float tanHalfFov = tanf(halfFovRad);

    // --- STEP 1: ERASE OLD POSITIONS ---
    // Only erase if they were drawn in the previous frame
    render_erase_psprite(opSprite, COL_BG); 
    for (int i = 0; i < numSprites; i++) {
        if (sprites[i].isExist) render_erase_hand(&sprites[i], COL_BG);
    }

    // --- STEP 2: CALCULATE PERSON (opSprite) ---
    float dxP = (float)opSprite->xPos - (float)player->x;
    float dyP = (float)opSprite->yPos - (float)player->y;
    float spriteAngleRadP = atan2f(dyP, dxP);
    float angleDiffRadP = spriteAngleRadP - playerRad;
    while (angleDiffRadP >  M_PI) angleDiffRadP -= 2.0f * M_PI;
    while (angleDiffRadP < -M_PI) angleDiffRadP += 2.0f * M_PI;

    // Is it in the 70-degree FOV?
    if (fabsf(angleDiffRadP) < halfFovRad) {
        float pDist = dxP * cosf(playerRad) + dyP * sinf(playerRad);
        if (pDist > 10.0f) { // Distance check to avoid huge sprites
            float screenXOffsetP = tanf(angleDiffRadP) / tanHalfFov;
            int targetXP = (int)((1.0f + screenXOffsetP) * 0.5f * (float)cols);
            int baseSize = (int)((64.0f * (float)rows) / pDist);

            // Update NEW positions
            opSprite->x_head = targetXP - (baseSize / 4);
            opSprite->y_head = (rows / 2) - (baseSize / 2);
            opSprite->x_neck = targetXP - (baseSize / 8);
            opSprite->y_neck = opSprite->y_head + (baseSize / 2);
            opSprite->x_body = targetXP - (baseSize / 2);
            opSprite->y_body = opSprite->y_neck + (baseSize / 4);
            
            // Save size for next frame's erasure
            opSprite->lastSize = baseSize; 

            // Draw Person FIRST
            render_draw_rect_outline(opSprite->x_head, opSprite->y_head, baseSize/2, baseSize/2, ILI9341_WHITE);
            render_draw_rect_outline(opSprite->x_neck, opSprite->y_neck, baseSize/4, baseSize/4, ILI9341_RED);
            render_draw_rect_outline(opSprite->x_body, opSprite->y_body, baseSize, rows - opSprite->y_body, ILI9341_BLUE);
        } else {
            opSprite->lastSize = 0; // Too close, don't draw/erase
        }
    } else {
        opSprite->lastSize = 0; // Off-screen, don't draw/erase
    }

    // --- STEP 3: DRAW HANDS SECOND (Always on top) ---
    for (int i = 0; i < numSprites; i++) {
        HandSprite* s = &sprites[i];
        if (!s->isExist) continue;

        float dx = (float)s->x - (float)player->x;
        float dy = (float)s->y - (float)player->y;
        float sDist = dx * cosf(playerRad) + dy * sinf(playerRad);
        
        float sAngle = atan2f(dy, dx) - playerRad;
        while (sAngle >  M_PI) sAngle -= 2.0f * M_PI;
        while (sAngle < -M_PI) sAngle += 2.0f * M_PI;

        if (cosf(sAngle) > 0 && sDist > 10.0f) {
            s->xScr = (int)((1.0f + (tanf(sAngle)/tanHalfFov)) * 0.5f * (float)cols);
            s->yScr = rows / 2 + s->z;
            s->size = (int)((64.0f * (float)rows) / sDist);
            render_draw_hand(s, ILI9341_YELLOW);
        }
    }
    return 0.0f;
}

//V5
//float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite* sprites, int numSprites, pSprite* opSprite)
//{
//    float playerRad = player->angle * (M_PI / 180.0f);
//    float halfFovRad = (35.0f) * (M_PI / 180.0f); 
//    float tanHalfFov = tanf(halfFovRad);
//
//    // --- STEP 1: ERASE OLD POSITIONS ---
//    // At this point, opSprite and sprites[i] still hold the coordinates 
//    // from the LAST frame. We erase them now.
//    render_erase_psprite(opSprite, COL_BG);
//    for (int i = 0; i < numSprites; i++) {
//        if (sprites[i].isExist) {
//            render_erase_hand(&sprites[i], COL_BG);
//        }
//    }
//
//    // --- STEP 2: CALCULATE NEW POSITION FOR THE PERSON (opSprite) ---
//    float dxP = (float)opSprite->xPos - (float)player->x;
//    float dyP = (float)opSprite->yPos - (float)player->y;
//    float spriteAngleRadP = atan2f(dyP, dxP);
//    float angleDiffRadP = spriteAngleRadP - playerRad;
//    while (angleDiffRadP >  M_PI) angleDiffRadP -= 2.0f * M_PI;
//    while (angleDiffRadP < -M_PI) angleDiffRadP += 2.0f * M_PI;
//
//    if (cosf(angleDiffRadP) > 0) {
//        float pDist = dxP * cosf(playerRad) + dyP * sinf(playerRad);
//        if (pDist < 1.0f) pDist = 1.0f; 
//
//        float screenXOffsetP = tanf(angleDiffRadP) / tanHalfFov;
//        int targetXP = (int)((1.0f + screenXOffsetP) * 0.5f * (float)cols);
//        int baseSize = (int)((64.0f * (float)rows) / pDist);
//
//        // NOW update the struct with NEW data
//        opSprite->x_head = targetXP - (baseSize / 4);
//        opSprite->y_head = (rows / 2) - (baseSize / 2);
//        opSprite->x_neck = targetXP - (baseSize / 8);
//        opSprite->y_neck = opSprite->y_head + (baseSize / 2);
//        opSprite->x_body = targetXP - (baseSize / 2);
//        opSprite->y_body = opSprite->y_neck + (baseSize / 4);
//        // Store the size so the NEXT frame's erase function knows how big it was
//        opSprite->lastSize = baseSize; 
//
//        // Draw the Person (Layer 0)
//        render_draw_rect_outline(opSprite->x_head, opSprite->y_head, baseSize/2, baseSize/2, ILI9341_WHITE);
//        render_draw_rect_outline(opSprite->x_neck, opSprite->y_neck, baseSize/4, baseSize/4, ILI9341_RED);
//        render_draw_rect_outline(opSprite->x_body, opSprite->y_body, baseSize, rows - opSprite->y_body, ILI9341_BLUE);
//    }
//
//    // --- STEP 3: CALCULATE & DRAW HANDS (Layer 1 - Always on top) ---
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
//        if (cosf(sAngle) > 0 && sDist > 1.0f) {
//            s->xScr = (int)((1.0f + (tanf(sAngle)/tanHalfFov)) * 0.5f * (float)cols);
//            s->yScr = rows / 2 + s->z;
//            s->size = (int)((64.0f * (float)rows) / sDist);
//            
//            // Draw Hand (Draws over the person if coordinates overlap)
//            render_draw_hand(s, ILI9341_YELLOW);
//        }
//    }
//    return 0.0f;
//}

//V4
//float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite* sprites, int numSprites, pSprite* opSprite)
//{
//    float playerRad = player->angle * (M_PI / 180.0f);
//    float halfFovRad = (35.0f) * (M_PI / 180.0f); 
//    float tanHalfFov = tanf(halfFovRad);
//
//    // 1. BUILD Z-BUFFER (Walls)
//    float zBuffer[cols];
//    for (int col = 0; col < cols; col++) {
//        float rayAngle = (player->angle - 35.0f) + ((float)col / (float)cols) * 70.0f;
//        zBuffer[col] = cast_ray(map, player, rayAngle, col, rows);
//        if (zBuffer[col] <= 0.0f) zBuffer[col] = 1e9f;
//    }
//
//    // 2. PHASE 1: ERASE ALL OLD POSITIONS
//    // We do this BEFORE any new drawing to prevent "clobbering" closer sprites
//    for (int i = 0; i < numSprites; i++) {
//        if (sprites[i].isExist) {
//            render_erase_hand(&sprites[i], COL_BG);
//        }
//    }
//
//    // 3. PHASE 2: CALCULATE DISTANCES & SORT (Furthest First)
//    for (int i = 0; i < numSprites; i++) {
//        if (!sprites[i].isExist) {
//            sprites[i].distanceToPlayer = -1.0f;
//            continue;
//        }
//        float dx = (float)sprites[i].x - (float)player->x;
//        float dy = (float)sprites[i].y - (float)player->y;
//        
//        // Using Perpendicular distance for sorting to match projection
//        sprites[i].distanceToPlayer = dx * cosf(playerRad) + dy * sinf(playerRad);
//    }
//
//    // Simple Bubble Sort (Furthest distance at index 0)
//    for (int i = 0; i < numSprites - 1; i++) {
//        for (int j = 0; j < numSprites - i - 1; j++) {
//            if (sprites[j].distanceToPlayer < sprites[j + 1].distanceToPlayer) {
//                HandSprite temp = sprites[j];
//                sprites[j] = sprites[j + 1];
//                sprites[j + 1] = temp;
//            }
//        }
//    }
//
//    // 4. PHASE 3: PROJECT AND DRAW (Back-to-Front)
//    for (int i = 0; i < numSprites; i++) {
//        HandSprite* s = &sprites[i];
//        if (!s->isExist || s->distanceToPlayer < 1.0f) continue;
//
//        float dx = (float)s->x - (float)player->x;
//        float dy = (float)s->y - (float)player->y;
//
//        // Relative Angle
//        float spriteAngleRad = atan2f(dy, dx);
//        float angleDiffRad = spriteAngleRad - playerRad;
//        while (angleDiffRad >  M_PI) angleDiffRad -= 2.0f * M_PI;
//        while (angleDiffRad < -M_PI) angleDiffRad += 2.0f * M_PI;
//
//        // Tangent Projection (X)
//        float screenXOffset = tanf(angleDiffRad) / tanHalfFov;
//        int targetX = (int)((1.0f + screenXOffset) * 0.5f * (float)cols);
//
//        // Cosine Correction (Size)
//        float cosDiff = cosf(angleDiffRad);
//        int targetSize = (int)((64.0f * (float)rows) / s->distanceToPlayer * cosDiff);
//
//        // Occlusion Check (Wall test)
//        // If the center is hidden by a wall, don't draw
//        if (targetX >= 0 && targetX < cols) {
//            if (s->distanceToPlayer > zBuffer[targetX]) continue;
//        }
//
//        // UPDATE Struct with NEW data
//        s->xScr = targetX;
//        s->yScr = rows / 2 + s->z;
//        s->size = targetSize;
//
//        render_draw_hand(s, ILI9341_YELLOW);
//
//        // DRAW NEW POSITION
//        // Because we sorted furthest-to-closest, closer sprites will 
//        // naturally draw over the top of further ones.
//        if (targetX > -targetSize && targetX < cols + targetSize) {
//            render_draw_hand(s, ILI9341_YELLOW);
//        }
//    }
//
//    return 0.0f;
//}
