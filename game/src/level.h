
#ifndef _SFG_LEVELS_H
#define _SFG_LEVELS_H
#define SFG_MAP_SIZE 64
#include "constants.h"
#include "neko_utils.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>


#define SFG_LEVEL_ELEMENT_NONE 0
#define SFG_LEVEL_ELEMENT_BARREL 0x01
#define SFG_LEVEL_ELEMENT_HEALTH 0x02
#define SFG_LEVEL_ELEMENT_BULLETS 0x03
#define SFG_LEVEL_ELEMENT_ROCKETS 0x04
#define SFG_LEVEL_ELEMENT_PLASMA 0x05
#define SFG_LEVEL_ELEMENT_TREE 0x06
#define SFG_LEVEL_ELEMENT_FINISH 0x07
#define SFG_LEVEL_ELEMENT_TELEPORTER 0x08
#define SFG_LEVEL_ELEMENT_TERMINAL 0x09
#define SFG_LEVEL_ELEMENT_COLUMN 0x0a
#define SFG_LEVEL_ELEMENT_RUIN 0x0b
#define SFG_LEVEL_ELEMENT_LAMP 0x0c
#define SFG_LEVEL_ELEMENT_CARD0 0x0d ///< Access card, unlocks doors with lock.
#define SFG_LEVEL_ELEMENT_CARD1 0x0e
#define SFG_LEVEL_ELEMENT_CARD2 0x0f
#define SFG_LEVEL_ELEMENT_LOCK0 0x10 /**< Special level element that must be
                                     then locked until taking the corresponding
                                     access card. */
#define SFG_LEVEL_ELEMENT_LOCK1 0x11
#define SFG_LEVEL_ELEMENT_LOCK2 0x12
#define SFG_LEVEL_ELEMENT_BLOCKER 0x13 ///< invisible wall

#define SFG_LEVEL_ELEMENT_MONSTER_SPIDER 0x20
#define SFG_LEVEL_ELEMENT_MONSTER_DESTROYER 0x21
#define SFG_LEVEL_ELEMENT_MONSTER_WARRIOR 0x22
#define SFG_LEVEL_ELEMENT_MONSTER_PLASMABOT 0x23
#define SFG_LEVEL_ELEMENT_MONSTER_ENDER 0x24
#define SFG_LEVEL_ELEMENT_MONSTER_TURRET 0x25
#define SFG_LEVEL_ELEMENT_MONSTER_EXPLODER 0x26


typedef struct
{
    uint8_t x;
    uint8_t y;

} SFG_Vector;

typedef struct
{
    SFG_Vector position;
    uint8_t type;

} SFG_Element;



typedef struct
{
    uint8_t doorPosition;
    uint8_t blockType;
    uint8_t height;
    uint8_t depth;
    SFG_Vector position;
    
} SFG_Block;


typedef struct
{
    SFG_Block blocks[MAP_DIMENSION * MAP_DIMENSION];
    SFG_Element elements[MAX_ELEMENTS];
    uint8_t ceilingHeight;
    SFG_Vector playerStart;
    uint8_t playerStartRotation;
    uint8_t floorType;
    uint8_t ceilingType;
    uint8_t maxWallheight;
} SFG_Level;


typedef struct {
    SFG_Level* history;
    int currentIndex;    
} LevelHistory;

#define SFG_NUMBER_OF_LEVELS 10


uint8_t SFG_loadLevelFromFile(SFG_Level* buffer, const char* level)
{
    if (buffer == NULL) {
        return 0;
    }

    char levelString[512];

#ifdef DEBUG
    
    snprintf(levelString, sizeof(levelString), "GameData\\levels\\%s.HAD", level);
#else
    snprintf(levelString, sizeof(levelString), "levels\\%s.HAD", level);
#endif
    
    FILE* file = fopen(levelString, "rb");
    if (file == NULL) {
         perror("Error opening file");
        return 0;
    }

    if (fread(buffer, sizeof(SFG_Level), 1, file) != 1) {
        fclose(file);
        // Handle the error
        return 0;
    }

    fclose(file);

    return 1;
}

void initLevel(SFG_Level* level)
{
    for (size_t i = 0; i < (MAP_DIMENSION * MAP_DIMENSION); i++)
    {
        level->blocks[i].blockType = 0;
        level->blocks[i].depth = 0;
        level->blocks[i].doorPosition = 0;
        level->blocks[i].position.x = 0;
        level->blocks[i].position.y = 0;
        level->blocks[i].height = 0;
        level->blocks[i].depth = 0;
    }

    for (size_t i = 0; i < MAX_ELEMENTS; i++)
    {
        level->elements[i].position.x = 0;
        level->elements[i].position.y = 0;
        level->elements[i].type = 0;
    }

    level->ceilingHeight = 0;
    level->floorType = 0;
    level->playerStart.x = 0;
    level->playerStart.y = 0;
    level->maxWallheight = DEFAULT_MAX_WALL_SIZE;
}


bool SaveLevel(SFG_Level *level, char* levelName)
{
    char levelStringBuffer[LEVEL_STRING_MAX_LENGTH];

    snprintf(levelStringBuffer, LEVEL_STRING_MAX_LENGTH, "%s%s.HAD", saveLocation, levelName);

    FILE* file = fopen(levelStringBuffer, "wb");

    if (file != NULL) {
        fwrite(level, sizeof(SFG_Level), 1, file);
        fclose(file);        
        // MemFree(level);
        return true;
    }
    else {
        // MemFree(level);
        return false;
    }
}
#endif // guard

