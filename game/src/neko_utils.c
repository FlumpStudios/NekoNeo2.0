
#include "neko_utils.h"
#include "constants.h"
#include <math.h>


void GetEntityPositionFromPosition(Vector3 location, uint8_t* col, uint8_t* row)
{
    *col = (int)(floor(location.x)) + (MAP_DIMENSION / 2);
    *row = (int)(floor(location.z)) + (MAP_DIMENSION / 2);
}

int GetMapArrayIndex(uint8_t col, uint8_t row)
{
    int index = (row * 64) + col;
    return index;
}

int GetMapIndeFromPosition(Vector3 location)
{
    int col = (int)(floor(location.x)) + (MAP_DIMENSION / 2);
    int row = (int)(floor(location.z)) + (MAP_DIMENSION / 2);

    int index = (row * 64) + col;
    return index;
}

bool IsFullWall(uint8_t index)
{
    if (index == 8)
    {
        return true;
    }
    return false;
}

uint8_t GetMapArrayHeightFromIndex(uint8_t index, uint8_t baseValue, uint8_t stepSize)
{
    // Empty space
    if (index == 0) 
    { 
        return 0; 
    }
    
    // Doors and full walls
    if(index < 15)
    {
        return baseValue;
    }

    // Platforms
    return ((index - 15) / 7 + 1) * stepSize;
}

Vector3 NormalizeVector(Vector3 v) {
    float magnitude = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    Vector3 normalized;
    normalized.x = v.x / magnitude;
    normalized.y = v.y / magnitude;
    normalized.z = v.z / magnitude;

    return normalized;
}

Vector3 CalculateCameraRayDirection(Camera* cam) {
    Vector3 direction;
    direction.x = cam->target.x - cam->position.x;
    direction.y = cam->target.y - cam->position.y;
    direction.z = cam->target.z - cam->position.z;
    return direction;
}

uint8_t GetNextElementType(uint8_t currentItem)
{
    if (currentItem < 0x0f)
    {
        return ++currentItem;
    }

    if (currentItem == 0x0f)
    {
        return 0x13;
    }

    if (currentItem == 0x13)
    {
        return 0x20;
    }
    if (currentItem >= 0x20 && currentItem < 0x26)
    {
        return ++currentItem;
    }
    return 0x01;
}

uint8_t GetPreviousElementType(uint8_t currentItem)
{
    
    if (currentItem <= 0x01)
    {
        return 0x26;
    }

    if (currentItem == 0x13)
    {
        return 0x0f;
    }

    if (currentItem == 0x20)
    {
        return 0x13;
    }

    return --currentItem;
}

