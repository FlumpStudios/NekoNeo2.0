#include <assert.h>
#include "raylib.h"
#include "engine.h"
#include "level.h"
#include <stdint.h>
#include <stdio.h>
#include "rl_utils.h"
#include <stdint.h>
#include <stdlib.h>
#include "core.h"
#include "neko_utils.h"
#include "rcamera.h"
#include "editorUi.h"
#include <string.h>
#include "raymath.h"
#include <stdio.h>
#include "drawTextUtils.h"
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>  // For Windows-specific functions
#define stat _stat   // Windows uses `_stat` instead of `stat`
#else
#include <unistd.h>  // For POSIX functions (optional)
#endif

static Mode PreviousMode = Mode_Editor;
static bool _levelReady = false;
static bool _2D_Mode = false;
static bool _focusedMode = false;
static uint32_t _blockCount;
static uint16_t _elementCount;
static uint8_t _floorHeight;
static UiMode drawHelpText = 1;
static int finishScreen = 0;
static int _currentWallSelection = 1;
static int _currentWallHighlighted = 1;
static int _currentItemSelection = 1;
static int _currentWallHeight = 1;
static int cameraMode = CAMERA_FREE;
static SFG_Level* level;
static Camera3D camera = { 0 };
static Camera2D camera2d = { 0 };
static char currentLevel[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME] = { 0 };

static LevelHistory _levelHistory;
static size_t _historySize = INITIAL_HISTORY_BUFFER_SIZE;

Mode currentEditorMode = Mode_Console;
EditorRenderMode currentRenderMode = RenerMode_Textured;

Texture2D wallTextures[WALL_TEXTURE_COUNT];
Texture2D itemTextures[ITEM_COUNT];
Texture2D weaponsTextures[WEAPON_COUNT];
Texture2D spiderEnemy;
Texture2D destroyerEnemy;
Texture2D warriorEnemy;
Texture2D plasmaBotEnemy;
Texture2D enderEnemy;
Texture2D turretEnemy;
Texture2D exploderEnemy;
Texture2D blocker;
Texture2D lock;
Model playerMarker;
Shader alphaDiscard;
Element* items;
MapBlock* mapBlocks;
SelectedEntity selectionLocation = { 0 };

typedef struct {
    char Text[MAX_INPUT_CHARS];
    char ResponseMessage[MAX_INPUT_CHARS];
} ConsoleHistory;

//CONSOLE STUFF
char name[MAX_INPUT_CHARS + 1] = "\0";
int letterCount = 0;
ConsoleHistory _consoleHistory[CONSOLE_HISTORY_SIZE];


// Forward declarations
void InitWalls(bool saveOnComplete);
void InitElements(bool saveOnComplete);
void DrawPlayerStartPosition(void);
void RefreshMap(bool updateHistory);
void UnloadEngineScreen(void);

int directoryExists(const char* path) {
    struct stat info;

    // Use stat() to check if the path exists
    if (stat(path, &info) != 0) {
        // Path does not exist
        return 0;
    }

    // Check if it's a directory
    if (info.st_mode & S_IFDIR) {
        return 1; // It is a directory
    }

    return 0; // Path exists but is not a directory
}

void GetLevelFilePath(char* buffer, char* fileName)
{
    memset(buffer, NULL, MAX_SAVE_FILE_NAME);
    if (strcmp(levelPack, EMPTY) == 0)
    {   
        sprintf(buffer, "%s", fileName);
    }
    else
    {
        sprintf(buffer, "%s\\%s", levelPack, fileName);
    }
}

void ConsoleQuery(const char* query, char* responseBuffer, size_t size)
{
    const char* inputString = TextToUpper(query);
    if (strncmp(inputString, "LOAD", 4) == 0)
    {
        const char* spacePos = strstr(inputString, " ");

        if (spacePos != NULL) {
            char* path = spacePos + 1;
            if (strcmp(levelPack, EMPTY) != 0)
            {
                char tempPath[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME];

                GetLevelFilePath(tempPath, path);

                strcpy(path, tempPath);
            }
        
            if (!SFG_loadLevelFromFile(level, path))
            {
                sprintf(responseBuffer, "Error loading level '%s'", path);
            }
            else
            {    
                strcpy_s(currentLevel, MAX_SAVE_FILE_NAME, path);
                sprintf(responseBuffer, "level '%s' loaded successfully", path);
                RefreshMap(true);
            }
        }        
    }
    else if (strncmp(inputString, "FILE", 4) == 0)
    {
        if (strnlen(currentLevel, MAX_SAVE_FILE_NAME) == 0)
        {
            strcpy(responseBuffer, "No level file has been loaded");
        }
        else
        {        
            strcpy_s(responseBuffer, MAX_SAVE_FILE_NAME, currentLevel);
        }
    }
    else if (strncmp(inputString, "CEILHEIGHT", 10) == 0) {
        sprintf(responseBuffer, "Ceiling Height = %i", level->ceilingHeight);
    }
    else if (strncmp(inputString, "CEIL", 10) == 0) {
        sprintf(responseBuffer, "Ceiling Type = %i", level->ceilingType);
    }
    else if (strncmp(inputString, "FLOORTYPE", 9) == 0) {
        sprintf(responseBuffer, "Floor type = %i", level->floorType);
    }
    
    else if (strncmp(inputString, "SETCEILHEIGHT", 13) == 0)
    {
        if (strnlen(inputString, 100) < 15)
        {
            strcpy(responseBuffer, "Please enter a value for ceil height");
        }
        else
        {
            auto col = atoi(&inputString[14]);

            if (col < 0) 
            {
                strcpy(responseBuffer, "Please enter a valid value");
            }
           else if (col > MAX_CEIL_HEIGHT)
            {
                char tooBigbuffer[40];
                const char* maxSizeResponse = sprintf(tooBigbuffer, "Ceil height must be equel to or below %i", MAX_CEIL_HEIGHT);
                strcpy(responseBuffer, tooBigbuffer);
            }
            else {
                level->ceilingHeight = col;
                RefreshMap(true);
                sprintf(responseBuffer, "Ceiling height updated to %i", col);
            }
        }
    }
    else if (strncmp(inputString, "SAVE", 4) == 0)
    {   
        char* spacePos = strstr(inputString, " ");
        
        if (spacePos == 0 && strlen(currentLevel) == 0)
        {
            strcpy(responseBuffer, "There is no active save file! Please specify a name for the save file");
        }
        else if (spacePos > 0 && strnlen(spacePos, MAX_INPUT_CHARS) > MAX_SAVE_FILE_NAME)
        {
            sprintf(responseBuffer, "Error: Filename exceeded max character limit %i", MAX_SAVE_FILE_NAME);
        }
        else
        {
            if (spacePos > 0)
            {
                if (spacePos != NULL) {
                    char* path = spacePos + 1;
                    if (strcmp(levelPack, EMPTY) != 0)
                    {
                        char tempPath[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME];
                        GetLevelFilePath(tempPath, path);
                        strcpy(currentLevel, tempPath);
                    }
                }
            }

            if (SaveLevel(level, currentLevel))
            {
                sprintf(responseBuffer, "level '%s' saved successfully", currentLevel);
            }
            else
            {
                sprintf(responseBuffer, "Error saving level '%s'. Please ensure level pack exists", currentLevel);
            }
        }
    }
    else if (strncmp(inputString, "QUIT", 4) == 0)
    {
        // TODO: Properly release everything        
        UnloadEngineScreen();
        return;
    }
    else if (strncmp(inputString, "NUKE", 4) == 0)
    {
        memset(level, 0, sizeof(SFG_Level));
        SFG_loadLevelFromFile(level, BASE_LEVEL_NAME);
        initLevel(level);
        RefreshMap(true);
        strcpy(responseBuffer, "Map Nuked!!! Exit console and Ctrl + Z if you're feeling regret");
    }
    else if (strncmp(inputString, "CLOSE", 5) == 0)
    {
        currentEditorMode = PreviousMode;
        strcpy(responseBuffer, "Console closed");
    }
    else if (strncmp(inputString, "CLS", 3) == 0)
    {
        memset(_consoleHistory, 0, sizeof(ConsoleHistory) * CONSOLE_HISTORY_SIZE);
        memset(name, 0, MAX_INPUT_CHARS);
    }
    else if (strncmp(inputString, "SETLEVELPACK", 12) == 0)
    {
        char buf[MAX_LEVEL_PACK_NAME];

        sprintf(buf, "%s%s", saveLocation, query + 13);

        if (directoryExists(buf) != 1)  
        {
            sprintf(responseBuffer, "%s is not a valid level pack", query );
        }
        else
        {
            char buffer[MAX_INPUT_CHARS];
            memset(buffer, NULL, MAX_INPUT_CHARS);
            strcpy(buffer, inputString + 13);
            if (strnlen(buffer, MAX_LEVEL_PACK_NAME) >= MAX_LEVEL_PACK_NAME)
            {
                strcpy(responseBuffer, "Pack name too long");
            }
            else if (strnlen(buffer, MAX_LEVEL_PACK_NAME) < 1)
            {
                strcpy(responseBuffer, "Please enter a pack name");
            }
            else
            {
                strcpy(levelPack, inputString + 13);
                sprintf(responseBuffer, "Level pack updated to %s", levelPack);
            }
        }
    }
    else if (strncmp(inputString, "LEVELPACK", 9) == 0)
    {
        if (strcmp(levelPack, EMPTY) != 0)
        {
            sprintf(responseBuffer, "%s", levelPack);
        }
        else
        {
            strcpy(responseBuffer, "No level pack has been set");
        }
    }
    
    else if (strncmp(inputString, "TEST", 4) == 0)
    {    

#ifdef  DEBUG
        strcpy(responseBuffer, "Map testing current disabled in debug");
        return;
#endif
        if (IsWindowFullscreen)
        {
            SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        }

        char buffer[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME];
        GetLevelFilePath(buffer, DEBUG_LEVEL);

        if (SaveLevel(level, buffer))
        {
            TraceLog(LOG_INFO, "Level saved on test hd game");
        }
        else
        {
            TraceLog(LOG_ERROR, "Error saving level to file");
        }

        char exeLocation[MAX_LEVEL_PACK_NAME + 20];

        if (levelPack == EMPTY)
        {
            sprintf(exeLocation, "Ruyn.exe -w -d");
        }
        else
        {
             sprintf(exeLocation,"Ruyn.exe -p %s -w -d", levelPack);
        }
        

        system(exeLocation);
    }
    else
    {
        if (strnlen(inputString, MAX_INPUT_CHARS) > 0)
        {
            sprintf(responseBuffer, "'%s' is not recognsied as a command", query);
        }
        else
        {
            sprintf(responseBuffer, "Please enter a command");
        }
    }
}

void HandleConsoleInput(void)
{       
    int key = GetCharPressed();

    while (key > 0)
    {
        // NOTE: Only allow keys in range [32..125]
        if ((key >= 32) && (key <= 125) && (letterCount < MAX_INPUT_CHARS))
        {
            name[letterCount] = (char)key;
            name[letterCount + 1] = '\0';
            letterCount++;
        }

        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        letterCount--;
        if (letterCount < 0) letterCount = 0;
        name[letterCount] = '\0';
    }


    if (IsKeyPressed(KEY_UP))
    {
        if (_consoleHistory)
        {           
            strcpy(name, _consoleHistory[0].Text);
            letterCount = strlen(name);
        }
    }

    else if (IsKeyPressed(KEY_ENTER))
    {
        for (size_t i = CONSOLE_HISTORY_SIZE - 1; i > 0; i--)
        {
            if (i > 0)
            {
                _consoleHistory[i] = _consoleHistory[i - 1];
            }
        }

        ConsoleQuery(name, &_consoleHistory[0].ResponseMessage, MAX_INPUT_CHARS);
        strcpy(&_consoleHistory[0].Text, &name);

        memset(&name, 0, sizeof(char) * MAX_INPUT_CHARS);
        letterCount = 0;
        name[letterCount] = '\0';
    };
}

void RenderConsole(void)
{    
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    int inputY = screenHeight - (CONSOLE_FONT_SIZE * 2);
    int responseY = screenHeight - (CONSOLE_FONT_SIZE);

    Rectangle textBox = {0,  screenHeight / 1.5f, screenWidth, screenHeight / 3 };
    DrawRectangleRec(textBox, TRANS_RED);
    DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, WHITE);    
    drawTextWithfont(font, name, (int)textBox.x + 5, inputY, CONSOLE_FONT_SIZE, WHITE);

    for (size_t i = 0; i < CONSOLE_HISTORY_SIZE; i++)
    {
        auto y = (inputY - (CONSOLE_FONT_SIZE * 2) - (i * CONSOLE_FONT_SIZE * 2 + 5));

        drawTextWithfont(font, _consoleHistory[i].Text, (int)textBox.x + 5, y, CONSOLE_FONT_SIZE, LIGHTGRAY);
        char buffer[MAX_INPUT_CHARS + 2];
        sprintf(buffer, "\n%s", _consoleHistory[i].ResponseMessage);
        drawTextWithfont(font, buffer, (int)textBox.x + 5, y, CONSOLE_FONT_SIZE, YELLOW);
    }
        
    if (letterCount < MAX_INPUT_CHARS)
    {
        static int framesCounter = 0;
        framesCounter++;
        if (framesCounter > 40)
        {
            framesCounter = 0;
        }
        if (((framesCounter / 20) % 2) == 0) drawTextWithfont(font, "_", (int)textBox.x + 8 + MeasureTextEx(font, name, CONSOLE_FONT_SIZE, FONT_SPACING).x, inputY, CONSOLE_FONT_SIZE, WHITE);
    }
}

void UpdateHistory(void)
{
    if (_levelHistory.currentIndex >= _historySize - 2)
    {
        size_t originalSize = sizeof(SFG_Level) * _historySize;

        SFG_Level* buffer = MemAlloc(originalSize);
        assert(buffer);
        memcpy(buffer, _levelHistory.history, originalSize);
        MemFree(_levelHistory.history);

        _historySize *= 2;
        size_t newSize = sizeof(SFG_Level) * _historySize;

        _levelHistory.history = MemAlloc(newSize);
        assert(_levelHistory.history);

        memcpy(_levelHistory.history, buffer, originalSize);
        MemFree(buffer);
    }

    _levelHistory.currentIndex++;

    if (level)
    {
        _levelHistory.history[_levelHistory.currentIndex] = *level;
    }
    memset(&_levelHistory.history[_levelHistory.currentIndex + 1], 0, sizeof(SFG_Level));
}

void RefreshMap(bool updateHistory)
{
    free(mapBlocks);
    mapBlocks = NULL;
    _blockCount = 0;
    InitWalls(true);

    free(items);
    items = NULL;
    _elementCount = 0;
    InitElements(true);
    
    if (updateHistory)
    {    
        UpdateHistory();
    }    
}

void CheckPlayerBlockCollision(void)
{
    for (int i = 0; i < _blockCount; i++)
    {
        if (!mapBlocks[i].hasBlock) { continue; }
        float h = mapBlocks[i].position.y + (mapBlocks[i].drawHeight / 2);
        if (camera.position.y > (h + PLAYER_STAIR_HEIGHT))
        {
            continue;
        };

        if (camera.position.x - (PLAYER_SCALE / 2) < mapBlocks[i].position.x + (BLOCK_WIDTH / 2) &&
            camera.position.x + (PLAYER_SCALE / 2) > mapBlocks[i].position.x - (BLOCK_WIDTH / 2) &&
            camera.position.z - (PLAYER_SCALE / 2) < mapBlocks[i].position.z + (BLOCK_WIDTH / 2) &&
            camera.position.z + (PLAYER_SCALE / 2) > mapBlocks[i].position.z - (BLOCK_WIDTH / 2))
        {
            float dx = fabs(camera.position.x - mapBlocks[i].position.x);
            float dy = fabs(camera.position.z - mapBlocks[i].position.z);

            float overlapX = (PLAYER_SCALE + BLOCK_WIDTH) / 2.0f - dx;
            float overlapY = (PLAYER_SCALE + BLOCK_WIDTH) / 2.0f - dy;

            if (overlapX < overlapY)
            {
                if (camera.position.x < mapBlocks[i].position.x)
                    camera.position.x -= overlapX;
                else
                    camera.position.x += overlapX;
            }

            else
            {
                if (camera.position.z < mapBlocks[i].position.z)
                    camera.position.z -= overlapY;
                else
                    camera.position.z += overlapY;
            }
        }
    }
}

void SetSelectionBlockLocation(void)
{
        selectionLocation.itemIndex = -1;
        Ray ray = { 0 };
        RayCollision collision = { 0 };
        
        if (_focusedMode)
        {
            ray = GetScreenToWorldRay(GetMousePosition(), camera);
        }
        else
        {        
            Vector3 pos = { camera.position.x, camera.position.y, camera.position.z };
            ray.position = pos;
            ray.direction = CalculateCameraRayDirection(&camera);        
        }

        enum Entity_Type_Item foundEntityType = Entity_Type_None;
        float dist = -1;

        int nearestSelection = -1;

        // CHECK FOR ITEMS
        for (int i = 0; i < _elementCount; i++)
        {
            collision = GetRayCollisionBox(ray, items[i].boundingBox);
            if (collision.hit)
            {
                if (collision.distance < dist || dist < 0)
                {
                    dist = collision.distance;
                    nearestSelection = i;
                    foundEntityType = Entity_Type_Item;
                }
            }
        }

        // Check for walls
        for (int i = 0; i < _blockCount; i++)
        {
            collision = GetRayCollisionBox(ray, mapBlocks[i].boundingBox);
            if (collision.hit)
            {
                if (collision.distance < dist || dist < 0)
                {   dist = collision.distance;
                    nearestSelection = i;
                    bool hasBlock = mapBlocks[i].hasBlock;
                    foundEntityType = hasBlock ? Entity_Type_Wall : Entity_Type_Free;
                }
            }
        }

        if (foundEntityType == Entity_Type_None)
        {
            // DO NOTHING AT THE MOMENT
        }
        else if (foundEntityType == Entity_Type_Wall || foundEntityType == Entity_Type_Free)
        {
            selectionLocation.entityType = foundEntityType;
            selectionLocation.itemIndex = -1;
            selectionLocation.position = mapBlocks[nearestSelection].position;
            selectionLocation.mapArrayIndex = GetMapIndeFromPosition(selectionLocation.position);
            if (foundEntityType == Entity_Type_Wall)
            {
                _currentWallHighlighted = level->blocks[selectionLocation.mapArrayIndex].blockType;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    _currentWallSelection = _currentWallHighlighted;                    
                }
            }
        }
        else if (foundEntityType == Entity_Type_Item)
        {
            selectionLocation.entityType = Entity_Type_Item;
            selectionLocation.itemIndex = nearestSelection;
            selectionLocation.position = items[nearestSelection].position;
            selectionLocation.mapArrayIndex = GetMapIndeFromPosition(selectionLocation.position);            
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                _currentItemSelection = level->elements[nearestSelection].type;     
            }
        }
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && foundEntityType != Entity_Type_Item)
        {
            uint8_t col = 0;
            uint8_t row = 0;

            GetEntityPositionFromPosition(selectionLocation.position, &col, &row);
            level->elements[_elementCount].position.x = col;
            level->elements[_elementCount].position.y = row;
            level->elements[_elementCount].type = _currentItemSelection;
            RefreshMap(true);
            selectionLocation.itemIndex = _elementCount - 1;
        }

        if (foundEntityType != Entity_Type_Wall && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            level->blocks[selectionLocation.mapArrayIndex].blockType = _currentWallSelection;
            level->blocks[selectionLocation.mapArrayIndex].height = _currentWallHeight;

            RefreshMap(true);   
        }
}

void RemoveElement(uint16_t i)
{
    while (i < MAX_ELEMENTS - 1 && level->elements[i].type > 0)
    {
        uint16_t next = i + 1;
        level->elements[i].position.x = level->elements[next].position.x;
        level->elements[i].position.y = level->elements[next].position.y;
        level->elements[i].type = level->elements[next].type;
        i++;
    };  
    // Reset the final element
    level->elements[i].type = 0;
    level->elements[i].position.x = 0;
    level->elements[i].position.y = 0;
}

int DoesPositionHaveElement(Vector3 location)
{
    uint8_t x = 0;
    uint8_t y = 0;

    GetEntityPositionFromPosition(location, &x, &y);
    
    for (size_t i = 0; i < _elementCount; i++)
    {
        if (level->elements[i].position.x == x && level->elements[i].position.y == y)
        {
            return i;
        }
    }
    return -1;
}

Texture GetTextureFromElementType(uint8_t i)
{
    assert(i > 0);

    if (i <= ITEM_COUNT)
    {
        return itemTextures[i - 1];
    }

    else
    {
        switch (i)
        {
            case 0x0d:
            case 0x0e:
            case 0x0f:
                return itemTextures[12];

            case 0x13:
                return blocker;
            case 0x20:
                return spiderEnemy;
            case 0x21:
                return destroyerEnemy;
            case 0x22:
                return warriorEnemy;
            case 0x23:
                return plasmaBotEnemy;
            case 0x24:
                return enderEnemy;
            case 0x25:
                return turretEnemy;
            case 0x26:
                return exploderEnemy;
            case SFG_LEVEL_ELEMENT_LOCK0:
            case SFG_LEVEL_ELEMENT_LOCK1:
            case SFG_LEVEL_ELEMENT_LOCK2:
                return lock;
        }
    }
    return itemTextures[0];
}

void InitElements(bool saveOnComplete)
{
    size_t size = sizeof(Element) * (MAP_ARRAY_SIZE * MAX_CEIL_HEIGHT);
    if (!items)
    {
        items = MemAlloc(size);
        if (!items)
        {
            TraceLog(LOG_ERROR, "Memory allocation failed on items!");
            return;
        }
    }
    memset(items, 0, size);

    for (size_t i = 0; i < MAX_ELEMENTS; i++)
    {
        if (level->elements[i].type > 0)
        {
            float size = 1.f;

            int mapArrayindex = GetMapArrayIndex(level->elements[i].position.x, level->elements[i].position.y);

            uint8_t stepSize = level->blocks[mapArrayindex].height;

            Texture t = GetTextureFromElementType(level->elements[i].type);
            float h = 0.25f * stepSize;
            float x = level->elements[i].position.x + 0.5;
            
            float y = (size / 2) + h;
            float z = level->elements[i].position.y + 0.5;
            Vector3 pos = (Vector3){ x - MAP_DIMENSION / 2,y ,z - MAP_DIMENSION / 2 };
            
            Vector3 elementSize = (Vector3){ 0.95f, 0.95f, 0.95f };

            BoundingBox elementbox = {0};

            if (level->elements[i].type < SFG_LEVEL_ELEMENT_LOCK0 || level->elements[i].type > SFG_LEVEL_ELEMENT_LOCK2)
            {
                elementbox = (BoundingBox){ (Vector3) {
                            pos.x - elementSize.x / 2,
                            pos.y - elementSize.y / 2,
                            pos.z - elementSize.z / 2
                        },
                        (Vector3) {
                            pos.x + elementSize.x / 2,
                            pos.y + elementSize.y / 2,
                            pos.z + elementSize.z / 2
                        } 
                };                
            }

            items[i].type = level->elements[i].type;
            items[i].position = pos;
            items[i].boundingBox = elementbox;
            items[i].texture = t;
            items[i].size = size;
            _elementCount++;
        }
        else
        {
            break;
        }
    }

    char buffer[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME];
    GetLevelFilePath(buffer, DEBUG_LEVEL);

    if (saveOnComplete && SaveLevel(level, buffer))
    {
        TraceLog(LOG_INFO, "Level saved on wall update");
    }
    else
    {
        TraceLog(LOG_ERROR, "Error saving level to file");
    }
}

void InitWalls(bool saveOnComplete)
{
    size_t size = sizeof(MapBlock) * (MAP_ARRAY_SIZE * MAX_CEIL_HEIGHT);
    if (!mapBlocks)
    {
        mapBlocks = MemAlloc(size);
        if (!mapBlocks)
        {
            TraceLog(LOG_ERROR, "Memory allocation failed on mapblocks!");
            return;
        }
    }
    memset(mapBlocks, 0, size);

    uint8_t col = 0;
    uint8_t row = 0;

    for (size_t i = 0; i < MAP_ARRAY_SIZE; i++)
    {
        if (i > 0 && i % MAP_DIMENSION == 0)
        {
            row++;
            col = 0;
        }

        if (level->blocks[i].blockType > 0)
        {
            // uint8_t v = level->mapArray[i];
            uint8_t height = level->blocks[i].height;
            uint8_t textureIndex = level->blocks[i].blockType;

            for (uint8_t k = 0; k < height; k++)
            {
                uint8_t skipCount = 0;
                float drawHeight = BLOCK_HEIGHT;

                for (uint8_t j = 4; j > 1; j--)
                {
                    if ((height - k) >= j)
                    {
                        drawHeight = BLOCK_HEIGHT * j;
                        skipCount = j - 1;
                        break;
                    }
                }

                float x = (1.0f * col) - MAP_DIMENSION / 2;
                float y = 1.0f;
                float z = (1.0f * row) - MAP_DIMENSION / 2;

                bool isDoor = level->blocks[i].doorPosition > 0;

                mapBlocks[_blockCount].isDoor = isDoor;
                if (isDoor)
                {
                    mapBlocks[_blockCount].position = (Vector3){ x + 0.5f, (k * BLOCK_HEIGHT) + drawHeight / 2, z + 0.5f };
                    if (k == (level->blocks[i].doorPosition - 1) * 4)
                    {
                        mapBlocks[_blockCount].texture = wallTextures[DOOR_TEXTURE_INDEX];

                    }
                    else
                    {
                        mapBlocks[_blockCount].texture = wallTextures[textureIndex];
                    }
                }
                else
                {
                    mapBlocks[_blockCount].position = (Vector3){ x + 0.5f, (k * BLOCK_HEIGHT) + drawHeight / 2, z + 0.5f };
                    mapBlocks[_blockCount].texture = wallTextures[level->blocks[i].blockType];
                }

                mapBlocks[_blockCount].drawHeight = drawHeight;
                mapBlocks[_blockCount].textureIndex = textureIndex;
                Vector3 wallPos = mapBlocks[_blockCount].position;
                Vector3 wallSize = { 1.0f,mapBlocks[_blockCount].drawHeight,1.0f };

                BoundingBox levelBox = (BoundingBox){ (Vector3) {
                        wallPos.x - wallSize.x / 2,
                        wallPos.y - wallSize.y / 2,
                        wallPos.z - wallSize.z / 2
                    },
                    (Vector3) {
                        wallPos.x + wallSize.x / 2,
                        wallPos.y + wallSize.y / 2,
                        wallPos.z + wallSize.z / 2
                    } };

                
                mapBlocks[_blockCount].boundingBox = levelBox;
                mapBlocks[_blockCount].hasBlock = true;

                _blockCount++;
                k += skipCount;
            }
        }
        else
        {
            float x = (1.0f * col) - MAP_DIMENSION / 2;
            float y = 1.0f;
            float z = (1.0f * row) - MAP_DIMENSION / 2;
            mapBlocks[_blockCount].position = (Vector3){ x + 0.5f, BLOCK_HEIGHT / 2, z + 0.5f };
            mapBlocks[_blockCount].hasBlock = false;
            mapBlocks[_blockCount].drawHeight = 0.25f;

            Vector3 wallPos = mapBlocks[_blockCount].position;
            Vector3 wallSize = { 1.0f,mapBlocks[_blockCount].drawHeight,1.0f };

            BoundingBox levelBox = (BoundingBox){ (Vector3) {
                        wallPos.x - wallSize.x / 2,
                        wallPos.y - wallSize.y / 2,
                        wallPos.z - wallSize.z / 2
                    },
                    (Vector3) {
                        wallPos.x + wallSize.x / 2,
                        wallPos.y + wallSize.y / 2,
                        wallPos.z + wallSize.z / 2
                    } };

            mapBlocks[_blockCount].boundingBox = levelBox;

            _blockCount++;        
        }
        col++;
    }

    char buffer[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME];
    GetLevelFilePath(buffer, DEBUG_LEVEL);

    if (saveOnComplete && SaveLevel(level, buffer))
    {
        TraceLog(LOG_INFO, "Level saved on wall update");
    }
    else
    {
        TraceLog(LOG_ERROR, "Error saving level to file");
    }
}

void InitGameplayScreen(void)
{

#ifdef DEBUG
    alphaDiscard = LoadShader(NULL, "game/src/shaders/alphaDiscard.frag");
    
    level = MemAlloc(sizeof(SFG_Level));

    if (level == NULL) 
{
        TraceLog(LOG_ERROR, "Memory allocation failed on level!");
    }
    else {
        initLevel(level);
        char buffer[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME];
        GetLevelFilePath(buffer, DEBUG_LEVEL);

        if (!SFG_loadLevelFromFile(level, buffer))
        {
            TraceLog(LOG_ERROR, "Error Loading debug level from file");

            if (!SFG_loadLevelFromFile(level, BASE_LEVEL_NAME))
            {
                TraceLog(LOG_ERROR, "Error Loading base level from file");
            }

        }
        else
        {
            TraceLog(LOG_INFO, "Level loaded OK");
        }
    }

    for (uint8_t i = 1; i <= WALL_TEXTURE_COUNT; i++)
    {
        char fullPath[256];
        sprintf(fullPath, "GameData/WallTextures/o_%u.png", i);
        wallTextures[i] = LoadTexture(fullPath);
    }

    for (uint8_t i = 0; i < ITEM_COUNT; i++)
    {
        char fullPath[256];
        sprintf(fullPath, "GameData/Items/o_%u.png", i);
        itemTextures[i] = LoadTexture(fullPath);
    }

    for (uint8_t i = 0; i < WEAPON_COUNT; i++)
    {
        char fullPath[256];
        sprintf(fullPath, "GameData/Weapons/o_%u.png", i);
        weaponsTextures[i] = LoadTexture(fullPath);
    }

    spiderEnemy = LoadTexture("GameData/Enemies/o_0.png");
    destroyerEnemy = LoadTexture("GameData/Enemies/o_3.png");
    warriorEnemy = LoadTexture("GameData/Enemies/o_6.png");
    plasmaBotEnemy = LoadTexture("GameData/Enemies/o_9.png");
    enderEnemy = LoadTexture("GameData/Enemies/o_12.png");
    turretEnemy = LoadTexture("GameData/Enemies/o_15.png");
    exploderEnemy = LoadTexture("GameData/Enemies/o_18.png");
    blocker = LoadTexture("GameData/assets/Blocker.png");
    lock = LoadTexture("GameData/assets/lock.png");
    playerMarker = LoadModel("GameData/assets/PlayerStartPosition.obj");

#else
    alphaDiscard = LoadShader(NULL, "data/alphaDiscard.frag");

    level = MemAlloc(sizeof(SFG_Level));

    if (level == NULL) {
        TraceLog(LOG_ERROR, "Memory allocation failed on level!");
    }
    else {
        memset(level, 0, sizeof(SFG_Level));
        initLevel(level);
        char buffer[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME];
        if (LOAD_DEBUG_LEVEL_ON_LOAD)
        {        
            GetLevelFilePath(buffer, DEBUG_LEVEL);
        }

        if (!SFG_loadLevelFromFile(level, buffer))
        {
            TraceLog(LOG_ERROR, "Error Loading level from file");        
        }
        else
        {
            TraceLog(LOG_INFO, "Level loaded OK");
        }
    }

    for (uint8_t i = 0; i < WALL_TEXTURE_COUNT; i++)
    {
        char fullPath[256];
        sprintf(fullPath, "WallTextures/o_%u.png", i);
        wallTextures[i] = LoadTexture(fullPath);
    }

    for (uint8_t i = 0; i < ITEM_COUNT; i++)
    {
        char fullPath[256];
        sprintf(fullPath, "Items/o_%u.png", i);
        itemTextures[i] = LoadTexture(fullPath);
    }

    for (uint8_t i = 0; i < WEAPON_COUNT; i++)
    {
        char fullPath[256];
        sprintf(fullPath, "Weapons/o_%u.png", i);
        weaponsTextures[i] = LoadTexture(fullPath);
    }

    spiderEnemy = LoadTexture("Enemies/o_0.png");
    destroyerEnemy = LoadTexture("Enemies/o_3.png");
    warriorEnemy = LoadTexture("Enemies/o_6.png");
    plasmaBotEnemy = LoadTexture("Enemies/o_9.png");
    enderEnemy = LoadTexture("Enemies/o_12.png");
    turretEnemy = LoadTexture("Enemies/o_15.png");
    exploderEnemy = LoadTexture("Enemies/o_18.png");
    blocker = LoadTexture("assets/Blocker.png");
    playerMarker = LoadModel("assets/PlayerStartPosition.obj");
    lock = LoadTexture("assets/lock.png");
#endif
    finishScreen = 0;

    _levelHistory.history = MemAlloc(sizeof(SFG_Level) * _historySize);
    assert(_levelHistory.history);
    memset(_levelHistory.history, 0, sizeof(SFG_Level)* _historySize);
    
    if (level)
    {
        camera.position = (Vector3){ level->playerStart.x - (MAP_DIMENSION / 2) , 1.0f, level->playerStart.y - (MAP_DIMENSION / 2) };
    }

    InitWalls(false);
    InitElements(false);

    camera.target = (Vector3){ 0.0f, 2.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          
    camera.fovy = DEFAULT_FOV;
    camera.projection = CAMERA_PERSPECTIVE;
    currentEditorMode = Mode_Editor;
    DisableCursor();
    
    // Initial level history
    if (level)
    {       
        memcpy(&_levelHistory.history[0], level, sizeof(SFG_Level));
    }

    // Set up the initial console screen
    strcpy(_consoleHistory[2].Text,"Welome to Neko Neo!");
    
	if (strcmp(levelPack, EMPTY) == 0)
	{
		strcpy(_consoleHistory[1].Text, "No level pack has been set");
        strcpy(_consoleHistory[0].Text, "use \"setlevelpack [pack name]\" to set you level pack");
	}
    else
    {
	    char levelpackBuffer[MAX_LEVEL_PACK_NAME  + 50];
        sprintf(levelpackBuffer,"The current level pack is set to %s", levelPack);
        strcpy(_consoleHistory[1].Text, levelpackBuffer);
        strcpy(_consoleHistory[0].Text, "Use \"loadlevel [levelname]\" to load a level or press escape to close the console and start making a new one.");
    }

    currentEditorMode = Mode_Console;

    _levelReady = true;
}

void ScrollUpEntities(void)
{
    if (selectionLocation.entityType == Entity_Type_Wall)
    {
        _currentWallHighlighted--;

        if (_currentWallHighlighted < 1)
        {
            _currentWallHighlighted = WALL_TEXTURE_COUNT;
        }

        _currentWallSelection = _currentWallHighlighted;
        

        level->blocks[selectionLocation.mapArrayIndex].blockType = _currentWallHighlighted;
        RefreshMap(true);
    }
    else if (selectionLocation.entityType == Entity_Type_Item)
    {
        uint8_t t = _currentItemSelection;
        uint8_t nextElement = GetPreviousElementType(t);
        level->elements[selectionLocation.itemIndex].type = nextElement;
        _currentItemSelection = nextElement;
        RefreshMap(true);
    }
}

void ScrollDownEntities(void)
{
    if (selectionLocation.entityType == Entity_Type_Wall)
    {
        _currentWallHighlighted++;

        if(_currentWallHighlighted > WALL_TEXTURE_COUNT)
        {
            _currentWallHighlighted = 1;
        }   

        _currentWallSelection = _currentWallHighlighted;


        level->blocks[selectionLocation.mapArrayIndex].blockType = _currentWallHighlighted;
        RefreshMap(true);
    }
    else if (selectionLocation.entityType == Entity_Type_Item)
    {
        uint8_t t = _currentItemSelection;
        uint8_t nextElement = GetNextElementType(t);
        level->elements[selectionLocation.itemIndex].type = nextElement;
        _currentItemSelection = nextElement;
        RefreshMap(true);
    }
}

// Gameplay Screen Update logic
void UpdateGameplayScreen(void)
{
    if (currentEditorMode == Mode_Console)
    {
        HandleConsoleInput();
    }
    if (!_levelReady)
    {
        return;
    }

    if ((IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || IsKeyPressed(KEY_F)) && currentEditorMode == Mode_Editor)
    {        
        _focusedMode = !_focusedMode;

        if (_focusedMode)
        {
            EUI_DrawStatusUpdate("Cursor unlocked", WHITE);
            EnableCursor();
        }
        else
        {
            EUI_DrawStatusUpdate("Cursor locked", WHITE);        
            DisableCursor();
        }
    }


    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (currentEditorMode == Mode_Console)
        {
            currentEditorMode = PreviousMode;
        }
    }

    if (IsKeyPressed(KEY_TAB))
    {                    
        if (currentEditorMode != Mode_Console)
        {
            PreviousMode = currentEditorMode;
            currentEditorMode = Mode_Console;
        }
    }

    if (currentEditorMode == Mode_Console) { return; }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z))
    {
        if (_levelHistory.currentIndex > 0)
        {
            _levelHistory.currentIndex--;
            memcpy(level, &_levelHistory.history[_levelHistory.currentIndex], sizeof(SFG_Level));
            RefreshMap(false);            
        }
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y))
    {
        SFG_Level a = _levelHistory.history[_levelHistory.currentIndex + 1];
        if (a.maxWallheight > 0)
        {
            _levelHistory.currentIndex++;
            memcpy(level, &_levelHistory.history[_levelHistory.currentIndex], sizeof(SFG_Level));
            RefreshMap(false);
        }
    }
    

    static bool isPlayerRotatingRight = false;
    if (IsKeyDown(KEY_F6))
    {
        level->playerStartRotation++;
        if (level->playerStartRotation >= 360)
        {
            level->playerStartRotation = 0;
        }
        isPlayerRotatingRight = true;
    }
    else
    {
        // Save on key up
        if (isPlayerRotatingRight == true)
        {
            RefreshMap(true);
            isPlayerRotatingRight = false;
        }
    }

    static bool isPlayerRotatingLeft = false;
    if (IsKeyDown(KEY_F5))
    {
        level->playerStartRotation--;
        if (level->playerStartRotation <= 0)
        {
            level->playerStartRotation = 360;
        }
        isPlayerRotatingLeft = true;
    }
    else
    {
        // Save on key up
        if (isPlayerRotatingLeft == true)
        {
            RefreshMap(true);
            isPlayerRotatingLeft = false;
        }
    }

    if (IsKeyPressed(KEY_ZERO))
    {
        if (selectionLocation.entityType == Entity_Type_Wall && level->blocks[selectionLocation.mapArrayIndex].doorPosition > 0)
        {
            int index = DoesPositionHaveElement(selectionLocation.position);

            if (index >= 0)
            {
                RemoveElement(index);
                RefreshMap(true);
            }
        }
    }

    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_THREE))
    {
        if (selectionLocation.entityType == Entity_Type_Wall)
        {
            if (level->blocks[selectionLocation.mapArrayIndex].doorPosition > 0)
            {
                uint8_t x = 0;
                uint8_t y = 0;

                GetEntityPositionFromPosition(selectionLocation.position, &x, &y);

                int index = DoesPositionHaveElement(selectionLocation.position);
                bool incCounter = false;

                if (index < 0)
                {
                    index = _elementCount;
                    incCounter = true;
                }

                level->elements[_elementCount].position.x = x;
                level->elements[_elementCount].position.y = y;


                if (IsKeyPressed(KEY_ONE))
                {
                    level->elements[index].type = SFG_LEVEL_ELEMENT_LOCK0;
                }
                else if (IsKeyPressed(KEY_TWO))
                {
                    level->elements[index].type = SFG_LEVEL_ELEMENT_LOCK1;
                }
                else if (IsKeyPressed(KEY_THREE))
                {
                    level->elements[index].type = SFG_LEVEL_ELEMENT_LOCK2;
                }
                if (incCounter)
                {
                    _elementCount++;
                }

                RefreshMap(true);
            }
        }
    }

    if (IsKeyPressed(KEY_T))
    {
        if (selectionLocation.entityType == Entity_Type_Wall)
        {
            if (level->blocks[selectionLocation.mapArrayIndex].doorPosition == 0)
            {
                if(level->blocks[selectionLocation.mapArrayIndex].height < 4)
                {
                    EUI_DrawStatusUpdate("Block height must be set to at least 4 to create a door", WHITE);
                }
                else
                {
                    level->blocks[selectionLocation.mapArrayIndex].doorPosition = 1;
                }
            }
            else
            {
                level->blocks[selectionLocation.mapArrayIndex].doorPosition = 0;
                int k = DoesPositionHaveElement(selectionLocation.position);
                if (k >= 0)
                {
                    RemoveElement(k);
                }
            }
            // _currentWallSelection = level->mapArray[selectionLocation.mapArrayIndex];
            RefreshMap(true);
        }
    }

    if (IsKeyPressed(KEY_P))
    {
        if (selectionLocation.entityType != Entity_Type_Item)
        {
            uint8_t col = 0;
            uint8_t row = 0;

            GetEntityPositionFromPosition(selectionLocation.position, &col, &row);

            level->playerStart.x = col;
            level->playerStart.y = row;
            
            char startBuffer[100];
            sprintf(startBuffer, "Player start position set to X:%u, Y:%u", col, row);
            EUI_DrawStatusUpdate(startBuffer, WHITE);

            char buffer[MAX_LEVEL_PACK_NAME + MAX_SAVE_FILE_NAME];
            GetLevelFilePath(buffer, DEBUG_LEVEL);

            if (SaveLevel(level, buffer))
            {
				TraceLog(LOG_INFO, "Level saved on player position update");

            }
            else
            {
                TraceLog(LOG_ERROR, "Error saving level to file");
            }
        }
        RefreshMap(true);
    }


    if (IsKeyPressed(KEY_M))
    {
        static bool was_orignally_focusedmode;        
        static Vector3 orignal_cam_pos;
        static Vector3 orignal_cam_target;
        static bool was_cursor_hidden;  

        _2D_Mode = !_2D_Mode;
        if (_2D_Mode)
        {   
            was_orignally_focusedmode = _focusedMode;
            orignal_cam_pos = camera.position;
            orignal_cam_target = camera.target;
            was_cursor_hidden = IsCursorHidden();
            
            ShowCursor();
            camera.position = (Vector3){ 0.0f, 2.0f, -100.0f };
            camera.target = (Vector3){ 0.0f, 2.0f, 0.0f };
            camera.projection = CAMERA_ORTHOGRAPHIC;
            CameraPitch(&camera, -90 * DEG2RAD, true, true, false);
            CameraYaw(&camera, 180 * DEG2RAD, false);
            CameraRoll(&camera, 0);
            _focusedMode = true;
            camera.fovy = 65.f;
            PreviousMode = currentEditorMode;
            currentEditorMode = Mode_Editor;
            EUI_DrawStatusUpdate("Map View", WHITE);
        }
        else
        {
            _focusedMode = was_orignally_focusedmode;
            camera.position = orignal_cam_pos;
            camera.target = orignal_cam_target;
            camera.projection = CAMERA_PERSPECTIVE;            
            camera.fovy = 65.f;
            was_cursor_hidden ? HideCursor() : ShowCursor();
            currentEditorMode = PreviousMode;
            EUI_DrawStatusUpdate("3D View", WHITE);
        }
    }

    if (IsKeyPressed(KEY_F11) ||
        ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_ENTER)) && IsKeyPressed(KEY_ENTER))
        )
    {
        if(IsWindowFullscreen)            
        { 
            SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        }
        else
        {
            int display = GetCurrentMonitor();
            SetWindowSize(GetMonitorWidth(display), GetMonitorHeight(display));
        }
        ToggleFullscreen();
    }

    if (IsKeyPressed(KEY_G))
    {
        currentEditorMode = (currentEditorMode == Mode_Editor ? Mode_Game : Mode_Editor);        
        if (currentEditorMode == Mode_Editor)
        {
			EUI_DrawStatusUpdate("Editor Mode", WHITE);
		}
		else
		{
			EUI_DrawStatusUpdate("Game Mode", WHITE);
        }
    }

    if (IsKeyPressed(KEY_H))
    {         
        drawHelpText++;
        if (drawHelpText > DebugAndHelp)
        {
            drawHelpText = 0;
        }
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
    {
		Color c = WHITE;
        char buffer[MAX_STATUS_MESSAGE_SIZE + MAX_SAVE_FILE_NAME];

        if (strlen(currentLevel) == 0)
        {
            strcpy(buffer, "Error: No level has been loaded");            
			c = RED;            
        }
        else if (strlen(levelPack) == 0)
        {   
            strcpy(buffer, "Error: No level pack has been loaded");
            c = RED;            
        }

        else if (SaveLevel(level, currentLevel))
        {
            sprintf(buffer, "Level saved  at: %s", currentLevel);            
			c = WHITE;
        }
        else
        {
            sprintf(buffer, "Error saving level");
            c = RED;
        }
        EUI_DrawStatusUpdate(buffer, WHITE);
    }
    
    if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_PERIOD))
    {
        if (selectionLocation.entityType == Entity_Type_Wall)
        {
            if (level->blocks[selectionLocation.mapArrayIndex].doorPosition > 0)
            {
                uint8_t s = level->blocks[selectionLocation.mapArrayIndex].height / 4;
                if ((level->blocks[selectionLocation.mapArrayIndex].doorPosition) < s)
                {
                    level->blocks[selectionLocation.mapArrayIndex].doorPosition++;
                    RefreshMap(true);
                }
            }
            // _currentWallSelection = level->mapArray[selectionLocation.mapArrayIndex];
        }
    }
    else if (IsKeyPressed(KEY_PERIOD))
    {   
        if (selectionLocation.entityType == Entity_Type_Wall) 
        { 
            level->blocks[selectionLocation.mapArrayIndex].height++;

            if (level->blocks[selectionLocation.mapArrayIndex].height > level->maxWallheight)
            {
                level->blocks[selectionLocation.mapArrayIndex].height = 1;
            }
            _currentWallHeight = level->blocks[selectionLocation.mapArrayIndex].height;
            RefreshMap(true);
        }        
    }

    if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_COMMA))
    {
        if (selectionLocation.entityType == Entity_Type_Wall)
        {
            if (level->blocks[selectionLocation.mapArrayIndex].doorPosition > 1)
            {
                level->blocks[selectionLocation.mapArrayIndex].doorPosition--;
            }
            // _currentWallSelection = level->mapArray[selectionLocation.mapArrayIndex];
            RefreshMap(true);
        }
    }
    else if (IsKeyPressed(KEY_COMMA))
    {     
        if (selectionLocation.entityType == Entity_Type_Wall)
        {
            if (level->blocks[selectionLocation.mapArrayIndex].height < 5 && level->blocks[selectionLocation.mapArrayIndex].doorPosition > 0)
            {
                EUI_DrawStatusUpdate("Doors must have a height value of 4 or higher", WHITE);
            }
            else
            {
                level->blocks[selectionLocation.mapArrayIndex].height --;

                if((level->blocks[selectionLocation.mapArrayIndex].doorPosition * 4) > level->blocks[selectionLocation.mapArrayIndex].height)
                {
                    level->blocks[selectionLocation.mapArrayIndex].doorPosition--;
                }

                if (level->blocks[selectionLocation.mapArrayIndex].height < 1)
                {
                    level->blocks[selectionLocation.mapArrayIndex].height = level->maxWallheight;
                }
                _currentWallHeight = level->blocks[selectionLocation.mapArrayIndex].height;
                RefreshMap(true);
            }
        }
    }

    if (IsKeyPressed(KEY_DELETE))
    {       
        if (selectionLocation.entityType == Entity_Type_Wall)
        {            
            SFG_resetBlock(&level->blocks[selectionLocation.mapArrayIndex]);

            RefreshMap(true);
        }
        else if (selectionLocation.entityType == Entity_Type_Item)
        {
            RemoveElement(selectionLocation.itemIndex);
            RefreshMap(true);
        }
        selectionLocation.entityType = Entity_Type_None;
    }

    if (IsKeyPressed(KEY_RIGHT_BRACKET))
    {   
        ScrollDownEntities();
    }

    if (IsKeyPressed(KEY_LEFT_BRACKET))
    {   
        ScrollUpEntities();
    }

    if (IsKeyPressed(KEY_R))
    {
        if (currentEditorMode == Mode_Editor)
        {
            currentRenderMode++;
            if (currentRenderMode >= 3)
            {
                currentRenderMode = 0;
            }
        }
        else
        {
            currentRenderMode = 0;
        }
        switch (currentRenderMode)
        {
		case 0:
			EUI_DrawStatusUpdate("Render Mode: Textured", WHITE);
			break;
		case 1:                
			EUI_DrawStatusUpdate("Render Mode: Wireframe", WHITE);
            break;
		case 2:
			EUI_DrawStatusUpdate("Render Mode: Entity types", WHITE);
			break;        
        }
    }

    if (currentEditorMode == Mode_Editor)
    {
        SetSelectionBlockLocation();
        cameraMode = CAMERA_FREE;
        camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; 
    }
    else if (currentEditorMode == Mode_Game)
    {
       /* cameraMode = CAMERA_FIRST_PERSON;
        camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };

        int arrayPos = GetMapIndeFromPosition(camera.position);
        auto e = level->mapArray[arrayPos];
        uint8_t h = GetMapArrayHeightFromIndex(e, 0, level->stepSize);
        
        camera.position.y = PLAYER_HEIGHT + (BLOCK_HEIGHT * (h));*/
    }
    else if (currentEditorMode == Mode_Scene)
    {
        cameraMode = CAMERA_ORBITAL;
        camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    }

    int wheelMove = GetMouseWheelMove();

    // Override the default key behavior
    if (IsKeyDown(KEY_E))
    {
        if (currentEditorMode == Mode_Editor)
        {        
            camera.position.y += 0.1f;
        }
    }
    else if (IsKeyDown(KEY_Q))
    {
        if (currentEditorMode == Mode_Editor)
        {
            camera.position.y -= 0.1f;
        }
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL))
    {
        
    }
    else if (wheelMove > 0.5f)
    {
        ScrollDownEntities();
    }
    else if (wheelMove < -0.5f)
    {
        ScrollUpEntities();
    }
    else
    {
        if (!_focusedMode)
        {
            UpdateCamera(&camera, cameraMode);
        }
        
        if(currentEditorMode == Mode_Game)
        {
            CheckPlayerBlockCollision();
        }
    }    
}

// Gameplay Screen Draw logic
void DrawGameplayScreen(void)
{
    ClearBackground(SKYBLUE);
    if (!_levelReady)
    {
        return;
    }
    
    BeginMode3D(camera);
    DrawPlane((Vector3) { 0.0f, -0.1f, 0.0f }, (Vector2) { 256.0f, 256.0f }, GRAY);
    DrawWalls();

    if (currentEditorMode == Mode_Editor)
    {
        int mod = _floorHeight % 4 > 0;
        
        if (selectionLocation.entityType == Entity_Type_Wall)
        {
            size_t range = ((_floorHeight / 4) + mod);
            for (size_t i = 0; i < range; i++)
            {
                float h = SELECTION_BOX_SIZE;
                float y = (float)i;
                
                if (i == range - 1)
                {                
                    float floor_mod = 0.25f * (_floorHeight % 4);

                    if (floor_mod == 0) { floor_mod = 1; }

                    h = SELECTION_BOX_SIZE * floor_mod;
                    y += h / 2;
                }
                else                    
                {
                    y += 0.5f;
                }
                                
                Vector3 vec = { selectionLocation.position.x,y, selectionLocation.position.z };
                if (selectionLocation.entityType == Entity_Type_Wall)
                {
                    DrawCube(vec, SELECTION_BOX_SIZE, h, SELECTION_BOX_SIZE, TRANS_BLUE);
                    DrawCubeWires(vec, SELECTION_BOX_SIZE, h, SELECTION_BOX_SIZE, BLACK);                    
                }                
            }
        }
        else if (selectionLocation.entityType == Entity_Type_Item)
        {
            Vector3 vec = { selectionLocation.position.x,selectionLocation.position.y, selectionLocation.position.z };
            DrawCubeWires(vec, SELECTION_BOX_SIZE, SELECTION_BOX_SIZE, SELECTION_BOX_SIZE, BLACK);
        }
        else if (selectionLocation.entityType == Entity_Type_Free)
        {
            Vector3 vec = { selectionLocation.position.x, 0.5f , selectionLocation.position.z };
            DrawCubeWires(vec, SELECTION_BOX_SIZE, SELECTION_BOX_SIZE, SELECTION_BOX_SIZE, WHITE);
        }

        DrawGrid(MAP_DIMENSION, 1.0f);
        DrawPlayerStartPosition();
    }

    BeginShaderMode(alphaDiscard);
    DrawElements();    
    EndShaderMode();   
    EndMode3D(); 

    if (currentEditorMode == Mode_Console)
    {
        RenderConsole();
    }   


    if (currentEditorMode == Mode_Game)
    {
        float x = (GetScreenWidth() / 2.0f) - ((weaponsTextures[1].width * GUN_SCALE) / 2);
        float y = GetScreenHeight() - (weaponsTextures[1].height * GUN_SCALE);
        DrawTextureEx(weaponsTextures[1], (Vector2) { x, y}, 0.0f, GUN_SCALE, WHITE);
    }

    if (level)
    {    
       Texture2D itemTexture = GetTextureFromElementType(_currentItemSelection);
       DebugInfo d = { &camera,selectionLocation.mapArrayIndex, GetFPS(), strcmp(levelPack, EMPTY) == 0 ? "None set": levelPack, MAX_ELEMENTS - _elementCount, wallTextures[_currentWallSelection], itemTexture};
       EUI_DrawDebugData(&d, drawHelpText);
    }
    

    if (!_focusedMode)
    {    
        DrawCrossHair();
    }
}

void DrawCrossHair(void)
{
    DrawText("+", (GetScreenWidth() / 2 ) -  10, GetScreenHeight() / 2, 24, WHITE);
}

void DrawPlayerStartPosition(void)
{
    float size = 1.f;

    int mapArrayindex = GetMapArrayIndex(level->playerStart.x, level->playerStart.y);

    uint8_t stepSize = level->blocks[mapArrayindex].height;

    float h = 0.25f * stepSize;
    float x = level->playerStart.x + 0.5;
    float y = (size / 2) + h;
    float z = level->playerStart.y + 0.5;
    Vector3 pos = (Vector3){ x - MAP_DIMENSION / 2, y ,z - MAP_DIMENSION / 2 };

    Vector3 source = { 0.f,-1.f,0.f };
    Vector3 scale = { 0.01f, 0.01f,0.01f };
    float rotationSource = level->playerStartRotation;
    if (currentRenderMode == RenerMode_Textured)
    {   
        DrawModelEx(playerMarker, pos, source, rotationSource, scale, WHITE);         
    }
    else if (currentRenderMode == RenerMode_Colored)
    {
        DrawCube(pos, 1.0f, 1, 1.0f, YELLOW);
        DrawCubeWires(pos, 1.0f, 1.0f, 1.0f, WHITE);
    }
    else if (currentRenderMode == RenerMode_CollisionBlock)
    {
        DrawCubeWires(pos, 1.0f, 1.0f, 1.0f, YELLOW);
    }
}

void DrawElements(void)
{
    for (size_t i = 0; i < _elementCount; i++)
    {
        if (currentRenderMode == RenerMode_Textured)
        {
            if (items[i].type == SFG_LEVEL_ELEMENT_CARD0 || items[i].type == SFG_LEVEL_ELEMENT_LOCK0)
            {
                DrawBillboard(camera, items[i].texture, items[i].position, items[i].size, RED);
            }
            else if (items[i].type == SFG_LEVEL_ELEMENT_CARD1 || items[i].type == SFG_LEVEL_ELEMENT_LOCK1)
            {
                DrawBillboard(camera, items[i].texture, items[i].position, items[i].size, GREEN);
            }
            else if (items[i].type == SFG_LEVEL_ELEMENT_CARD2 || items[i].type == SFG_LEVEL_ELEMENT_LOCK2)
            {
                DrawBillboard(camera, items[i].texture, items[i].position, items[i].size, BLUE);
            }
            else
            {            
                DrawBillboard(camera, items[i].texture, items[i].position, items[i].size, WHITE);
            }
        }
        else if (currentRenderMode == RenerMode_Colored)
        {
            DrawCube(items[i].position, 1.0f, 1, 1.0f, RED);
            DrawCubeWires(items[i].position, 1.0f, 1.0f, 1.0f, WHITE);
        }
        else if (currentRenderMode == RenerMode_CollisionBlock)
        {
            DrawBoundingBox(items[i].boundingBox, RED);
        }
    }
}

void DrawWalls(void)
{   
    if (mapBlocks)
    {
        for (size_t i = 0; i < _blockCount; i++)
        {
            if (mapBlocks[i].hasBlock &&  mapBlocks[i].drawHeight > 0)
            {            
                if (currentRenderMode == RenerMode_Textured)
                {                   
                    UTL_DrawCubeTexture(mapBlocks[i].texture, mapBlocks[i].position, 1.0f, mapBlocks[i].drawHeight, 1.0f, WHITE);
                }
                else if (currentRenderMode == RenerMode_Colored)
                {
                    if (mapBlocks[i].isDoor)
                    {
                        DrawCube(mapBlocks[i].position, 1.0f, mapBlocks[i].drawHeight, 1.0f, GREEN);
                    }
                    else
                    {
                        DrawCube(mapBlocks[i].position, 1.0f,mapBlocks[i].drawHeight, 1.0f, BLUE);
                    }
                    DrawCubeWires(mapBlocks[i].position, 1.0f, mapBlocks[i].drawHeight, 1.0f, WHITE);
                }
                else if (currentRenderMode == RenerMode_CollisionBlock)
                {   
                    //if(mapBlocks[i].textureIndex)
                    DrawBoundingBox(mapBlocks[i].boundingBox, BLUE);
                }
            }
        }
    }
}

// Gameplay Screen Unload logic
void UnloadEngineScreen(void)
{
    readyToExit = 1;
}

