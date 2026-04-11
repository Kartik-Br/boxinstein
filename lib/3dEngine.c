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

//V3
float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite** sprites, int numSprites)
{
    // 1) Build z-buffer for occlusion
    float zBuffer[cols];
    float playerRad = player->angle * (M_PI / 180.0f);
    float halfFovRad = (FOV * 0.5f) * (M_PI / 180.0f);
    float tanHalfFov = tanf(halfFovRad);

    for (int col = 0; col < cols; col++) {
        float rayAngle = (player->angle - (FOV / 2.0f)) + ((float)col / (float)cols) * FOV;
        zBuffer[col] = cast_ray(map, player, rayAngle, col, rows);
        if (zBuffer[col] <= 0.0f) zBuffer[col] = 1e9f;
    }

    // 2) Update sprite distances for sorting
    for (int i = 0; i < numSprites; i++) {
        HandSprite* s = &(*sprites)[i];
        if (!s->isExist) continue;
        s->distanceToPlayer = dist((float)player->x, (float)player->y, (float)s->x, (float)s->y, 0.0f);
    }

    // 3) Back-to-Front Sort
    qsort(*sprites, numSprites, sizeof(HandSprite), compar);

    // 4) Project and Draw
    for (int i = 0; i < numSprites; i++) {
        HandSprite* s = &(*sprites)[i];
        if (!s || !s->isExist) continue;

        // Relative vector
        float dx = (float)s->x - (float)player->x;
        float dy = (float)s->y - (float)player->y;

        // Angle to sprite
        float spriteAngleRad = atan2f(dy, dx);
        float angleDiffRad = spriteAngleRad - playerRad;

        // Normalize angle
        while (angleDiffRad >  M_PI) angleDiffRad -= 2.0f * M_PI;
        while (angleDiffRad < -M_PI) angleDiffRad += 2.0f * M_PI;

        // 1. Cull if clearly behind the camera plane
        float cosP = cosf(playerRad);
        float sinP = sinf(playerRad);
        float perpDist = dx * cosP + dy * sinP;

        if (perpDist < 1.0f) {
            render_erase_hand(s, COL_BG);
            continue;
        }

        // 2. PROJECT X using Tangent (Perspective Correct)
        // This maps the 3D angle to a flat screen plane correctly
        float screenXOffset = tanf(angleDiffRad) / tanHalfFov;
        int targetX = (int)((1.0f + screenXOffset) * 0.5f * (float)cols);

        // 3. CORRECT SIZE (Lens Distortion Fix)
        // We multiply by cos(angleDiff) to "squash" the sprite at the edges
        // This counteracts the stretching effect of a flat screen
        float cosDiff = cosf(angleDiffRad);
        int targetSize = (int)((64.0f * (float)rows) / perpDist * cosDiff);

        // Basic clipping
        if (targetX < -targetSize || targetX > cols + targetSize) {
            render_erase_hand(s, COL_BG);
            continue;
        }

        // 4. Occlusion Check
        if (targetX >= 0 && targetX < cols) {
            if (perpDist > zBuffer[targetX]) {
                render_erase_hand(s, COL_BG);
                continue;
            }
        }

        /* RENDER CYCLE */
        // Erase old pos
        render_erase_hand(s, COL_BG);

        // Assign new pos
        s->xScr = targetX;
        s->yScr = rows / 2;
        s->size = targetSize;

        // Draw new pos
        render_draw_hand(s, ILI9341_YELLOW);
    }
    
    return 0.0f;
}

//V2
//float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite** sprites, int numSprites)
//{
//    // 1) Build z-buffer by casting rays per column
//    // Even if you don't draw walls, the zBuffer is needed to hide sprites behind "invisible" walls
//    float zBuffer[cols];
//    for (int col = 0; col < cols; col++) {
//        float rayAngle = (player->angle - (FOV / 2.0f)) + ((float)col / (float)cols) * FOV;
//        zBuffer[col] = cast_ray(map, player, rayAngle, col, rows);
//        
//        // Ensure zBuffer is valid for depth testing
//        if (zBuffer[col] <= 0.0f) zBuffer[col] = 1e9f;
//    }
//
//    // 2) Update sprite distances for sorting
//    for (int i = 0; i < numSprites; i++) {
//        HandSprite* s = &(*sprites)[i];
//        s->distanceToPlayer = dist((float)player->x, (float)player->y, (float)s->x, (float)s->y, 0.0f);
//    }
//
//    // 3) Sort sprites Back-to-Front
//    // Note: Used HandSprite size to prevent memory corruption
//    qsort(*sprites, numSprites, sizeof(HandSprite), compar);
//
//    // 4) Project and Draw Sprites
//    for (int i = 0; i < numSprites; i++) {
//        HandSprite* s = &(*sprites)[i];
//        
//        if (!s || !s->isExist) continue;
//
//        // Vector from player to sprite
//        float dx = (float)s->x - (float)player->x;
//        float dy = (float)s->y - (float)player->y;
//
//        // Angle to sprite in degrees
//        float spriteAngle = atan2f(dy, dx) * (180.0f / M_PI);
//
//        // Difference from player facing
//        float angleDiff = spriteAngle - player->angle;
//        while (angleDiff > 180.0f)  angleDiff -= 360.0f;
//        while (angleDiff < -180.0f) angleDiff += 360.0f;
//
//        // 1. Cull if completely behind the player or way outside FOV
//        // We allow a bit of padding (FOV * 0.8) so the rectangle doesn't pop out of existence 
//        // while the edge of it is still visible.
//        if (fabsf(angleDiff) > (FOV * 0.8f)) {
//            render_erase_hand(s, COL_BG);
//            continue;
//        }
//
//        // 2. Fish-eye correction and distance clamping
//        float angleDiffRad = angleDiff * (M_PI / 180.0f);
//        float spriteDist = s->distanceToPlayer * cosf(angleDiffRad);
//        if (spriteDist < 1.0f) spriteDist = 1.0f; 
//
//        // 3. Simple projection to screen coordinates
//        // screenXRatio maps -FOV/2...FOV/2 to 0...1
//        float screenXRatio = (angleDiff + (FOV * 0.5f)) / FOV;
//        int targetX = (int)(screenXRatio * cols);
//        int targetY = rows / 2;
//
//        // 4. Calculate projected size (standard raycast scaling)
//        int targetSize = (int)((64.0f * rows) / spriteDist);
//
//        // 5. Depth Test (Simplified for "all in one go")
//        // We check if the center of the sprite is occluded by a wall.
//        // For a more advanced version, you'd check zBuffer[targetX].
//        if (targetX >= 0 && targetX < cols) {
//            if (spriteDist > zBuffer[targetX]) {
//                // Sprite is hidden behind a wall
//                render_erase_hand(s, COL_BG);
//                continue;
//            }
//        }
//
//        // 6. DRAWING LOGIC
//        // Erase the OLD position/size stored in the struct
//        render_erase_hand(s, COL_BG);
//
//        // Update struct with NEW position/size
//        s->xScr = targetX;
//        s->yScr = targetY;
//        s->size = targetSize;
//
//        // Draw the NEW position
//        render_draw_hand(s, ILI9341_YELLOW);
//    }
//    
//    return 0.0f;
//}

//V1
//float draw_all_stuff(uint8_t (*map)[20], Player_info* player, int cols, int rows, HandSprite** sprites, int numSprites)
//{
//    // 1) Build z-buffer by casting rays per column
//    float zBuffer[cols];
//    int dir = 0;
//    int startx = 0;
//    int starty = 0;
//    int endx = 0;
//    int endy = 0;
//    for (int col = 0; col < cols; col++) {
//        float rayAngle = (player->angle - (FOV / 2.0f)) + ((float)col / (float)cols) * FOV;
//        zBuffer[col] = cast_ray(map, player, rayAngle, col, rows);
//        continue;
//
//        //float denom = (zBuffer[col] > 1e-6f) ? zBuffer[col] : 1e-6f;
//        //int lineH = (int)fabsf((TILE * rows) / denom);
//        ////float scaleFactor = (float)lineH / 32.0f;
//        //float scaleFactor = 32.0f / (float)lineH;
//        //float scaleOffset = 0;
//        //if (lineH > rows) {
//        //scaleOffset = (lineH - rows) / 2.0;
//        //lineH = rows;
//        //}
//
//        //int drawX = col;
//        //int lineOffset = (rows / 2) - (lineH >> 1);
//
//        if (col > 0) {
//            if (zBuffer[col] > zBuffer[col - 1]) {
//                //GET END POINT
//                //DRAW LINE FROM START TO END POINT FOR TOP AND BOTTOM
//                //DRAW VERTICAL LINE ONLY AT THIS POINT
//                //CHANGE DIR
//                //
//            } else if (zBuffer[col] < zBuffer[col - 1]) {
//
//            } else {
//
//            }
//        }
//        // Cast may return 0 on tie — treat as “very far” so sprites in front can still draw
//        if (zBuffer[col] <= 0.0f) zBuffer[col] = 1e9f;
//    }
//
//    // 2) Compute sprite distances (ALWAYS use (*sprites)[i])
//    for (int i = 0; i < numSprites; i++) {
//        //Sprite* s = &(*sprites)[i];
//        HandSprite* s = &(*sprites)[i];
//        s->distanceToPlayer = dist((float)player->x, (float)player->y, (float)s->x, (float)s->y, 0.0f);
//    }
//
//    // TODO: sort sprites back-to-front by distanceToPlayer if you want proper overlap.
//
//    // 3) Project and draw sprites with clipping & depth test
//
//    qsort(*sprites, numSprites, sizeof(Sprite), compar);
//
//    for (int i = 0; i < numSprites; i++) {
//        //Sprite* s = &(*sprites)[i];
//        HandSprite* s = &(*sprites)[i];
//	if (!s->isExist) {
//	    continue;
//	}
//
//        // Vector from player to sprite
//        float dx = (float)s->x - (float)player->x;
//        float dy = (float)s->y - (float)player->y;
//
//        // Angle to sprite in degrees
//        float spriteAngle = atan2f(dy, dx) * (180.0f / M_PI);
//
//        // Difference from player facing
//        float angleDiff = spriteAngle - player->angle;
//        while (angleDiff > 180.0f)  angleDiff -= 360.0f;
//        while (angleDiff < -180.0f) angleDiff += 360.0f;
//
//        // Cull if outside FOV
//        if (fabsf(angleDiff) > (FOV * 0.5f)) {
//            render_erase_hand(s, COL_BG);
//            continue;
//        }
//
//        // Distance (avoid zero to prevent div-by-zero)
//        float spriteDist = sqrtf(dx*dx + dy*dy);
//	// correct fish-eye
//	float playerRad = player->angle * (M_PI / 180.0f);
//	float spriteAngleRad = atan2f(dy, dx);
//	float angleDiffRad = spriteAngleRad - playerRad;
//
//	while (angleDiffRad > M_PI)  angleDiffRad -= 2*M_PI;
//	while (angleDiffRad < -M_PI) angleDiffRad += 2*M_PI;
//
//	spriteDist *= cosf(angleDiffRad);
//        if (spriteDist < 1e-3f) spriteDist = 1e-3f;
//
//        // Simple projection to screen X
//        float screenXRatio = (angleDiff + (FOV * 0.5f)) / FOV;   // 0..1 across FOV
//        int spriteScreenX = (int)(screenXRatio * cols);
//        int spriteScreenY = rows / 2;  // center vertically for now
//
//        // Apparent sprite size (scaled like your wall height calc)
//        int spriteHeight = (int)((rows * 64.0f) / spriteDist);
//        int spriteWidth  = spriteHeight;
//
//        // Clip width/height to sane range
//        if (spriteWidth  < 1) spriteWidth  = 1;
//        if (spriteHeight < 1) spriteHeight = 1;
//        if (spriteHeight > rows) spriteHeight = rows;  // vertical clamp
//
//        // Horizontal span (clipped)
//        int startCol = spriteScreenX - (spriteWidth / 2);
//        int endCol   = spriteScreenX + (spriteWidth / 2);
//        if (startCol < 0)    startCol = 0;
//        if (endCol   > cols) endCol   = cols;
//
//        // Vertical start for render_line (clipped so it doesn’t go negative)
//        int startRow = spriteScreenY - (spriteHeight / 2);
//        if (startRow < 0) startRow = 0;
//        if (startRow + spriteHeight > rows) {
//            spriteHeight = rows - startRow;
//            if (spriteHeight <= 0) continue;
//        }
//
//        // Draw columns that pass the z-test
//	int totalCols = endCol - startCol;
//        for (int col = startCol; col < endCol; col++) {
//            // Depth test: sprite must be in front of wall slice
//            //if (spriteDist < zBuffer[col]) {
//            if (1) {
//                // Render a vertical slice of the sprite. Here we just reuse render_line as a solid column.
//                // If you later texture sprites, you’ll sample the correct column instead of drawing a solid.
//                //render_line(col, startRow, spriteHeight, 'B', 1);
//		//render_player(col, startRow, col*(14.0f / (float)totalCols), spriteHeight);
//		int texW = 12;                      // your sprite texture width in columns (was hard-coded 14.0)
//		//switch(s->spriteType) {
//		//    case S_GUY:
//		//	texW = 14;
//		//	break;
//		//    case S_FISTLP:
//		//	texW = 12;
//		//	break;
//		//    case S_FISTRP:
//		//	texW = 12;
//		//	break;
//		//    case S_FISTLO:
//		//	texW = 12;
//		//	break;
//		//    case S_FISTRO:
//		//	texW = 12;
//		//	break;
//		//    case S_SGUN:
//		//	texW = 49;
//		//	break;
//		//}
//		int rel = col - startCol;           // 0 .. totalCols-1
//		// Map 0..totalCols-1 -> 0..texW-1 with rounding & clamping
//		int texX = (int)roundf(rel * (texW - 1) / (float)(totalCols - 1));
//		if (texX < 0) texX = 0;
//		if (texX >= texW) texX = texW - 1;
//
//		//switch(s->spriteType) {
//		//   case S_GUY: {
//		//	// normal projected size
//		//	//int spriteHeight = (int)((rows * 64.0f) / spriteDist);
//		//	//int spriteWidth  = spriteHeight;
//		//	//if (spriteWidth  < 1) spriteWidth  = 1;
//		//	//if (spriteHeight < 1) spriteHeight = 1;
//		//	//if (spriteHeight > rows) spriteHeight = rows;
//
//		//	//int startRow = rows/2 - spriteHeight/2;
//		//	//if (startRow < 0) startRow = 0;
//
//		//	//int rel = col - startCol;
//		//	//int texX = (int)roundf(rel * (texW - 1) / (float)(totalCols - 1));
//		//	//if (texX < 0) texX = 0;
//		//	//if (texX >= texW) texX = texW - 1;
//
//		//	//render_player(col, startRow, texX, spriteHeight);
//
//
//		//	break;
//		//   }
//        //   //case S_FISTLP: {
//        //   //     int left = s->x - s->size;
//        //   //     int top = s->y - s->size;
//        //   //     //int size = s->size;
//        //   //     int width = s->size;
//        //   //     int height = s->size;
//        //   //     //DRAW A BLOCK OF A CERTAIN COLOUR
//        //   //               }
//        //   //case S_FISTRP: {
//        //   //     int left = s->x - s->size;
//        //   //     int top = s->y - s->size;
//        //   //     //int size = s->size;
//        //   //     int width = s->size;
//        //   //     int height = s->size;
//        //   //     //DRAW A BLOCK OF A CERTAIN COLOUR
//        //   //               }
//        //   //case S_FISTLO: {
//        //   //     int left = s->x - s->size;
//        //   //     int top = s->y - s->size;
//        //   //     //int size = s->size;
//        //   //     int width = s->size;
//        //   //     int height = s->size;
//        //   //     //DRAW A BLOCK OF A CERTAIN COLOUR
//        //   //               }
//        //   //case S_FISTRO: {
//        //   //     int left = s->x - s->size;
//        //   //     int top = s->y - s->size;
//        //   //     //int size = s->size;
//        //   //     int width = s->size;
//        //   //     int height = s->size;
//        //   //     //DRAW A BLOCK OF A CERTAIN COLOUR
//        //   //               }
//		//}
//        render_erase_hand(s, COL_BG);
//        s->xScr = col;
//        s->yScr = startRow;
//        s->size = spriteHeight;
//        render_draw_hand(s, ILI9341_YELLOW);
//        break; //DRAW WHOLE SPRITE IN ONE GO
//
//            }
//        }
//    }
//    return 0.0f; // function is declared float; return something (unused)
//}
