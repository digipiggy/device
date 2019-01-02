#include "math.h"
#include "neopixel.h"

#define CODE_PIN D0
#define CODE_WIFI 2
#define PIXEL_PIN D3
#define PIXEL_COUNT 32
#define PIXEL_BRIGHTNESS 50
#define PIXEL_TYPE WS2812B
#define ST_1B_EMPTY 0xFF
#define ST_4B_EMPTY 0xFFFFFFFF
#define ST_OFFSET 200
#define ST_OFFSET_VALUE 1
#define ST_OFFSET_PROMISE 5
#define ST_OFFSET_COLOR 9
#define PIXEL_COLOR_DARK 0
#define PIXEL_COLOR_DEFAULT 2677760

//this needs to be used for firmware upload
//PRODUCT_ID();
PRODUCT_VERSION(1);
SYSTEM_THREAD(ENABLED);

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

bool pixelDisplay[PIXEL_COUNT];
bool pixelDimmed[PIXEL_COUNT];
uint32_t pixelColor[PIXEL_COUNT];

bool goalReached;
bool showingRainbow;

// System Functions

void setup()
{
    System.set(SYSTEM_CONFIG_SOFTAP_PREFIX, "DIGIPIGGY");
    
    Particle.function("clear", goalClear);
    Particle.function("reset", goalReset);
    Particle.function("toggle", goalToggle);
    Particle.function("update", goalUpdate);
    Particle.function("color", goalColor);
    
    pinMode(CODE_PIN, OUTPUT);
    updateDisplay();
    
    strip.begin();
    strip.show();
}

void loop()
{
    if (!Particle.connected())
    {
        showWarning(CODE_WIFI);
    }
    else
    {
        if (goalReached && !showingRainbow)
        {
            showRainbow();
        }
        else
        {
            showGoals();
        }
    }
}

// Display Functions

void showWarning(int code)
{
    switch (code)
    {
        case CODE_WIFI: // WiFi connectivity
            for (int i = 0; i < CODE_WIFI; i++)
            {
                digitalWrite(CODE_PIN, HIGH);
                delay(250);
                digitalWrite(CODE_PIN, LOW);
                delay(250);
            }
            break;
    }
    
    delay(2000);
}

void showGoals()
{
    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        if (pixelDisplay[i] || pixelDimmed[i])
        {
            uint8_t
              r = (uint8_t)(pixelColor[i] >> 16),
              g = (uint8_t)(pixelColor[i] >> 8),
              b = (uint8_t)pixelColor[i];
            
            if (pixelDimmed[i])
            {
                strip.setColorDimmed(i, r, g, b, 180);
            }
            else
            {
                strip.setColor(i, r, g, b);
            }
        }
        else 
        {
            strip.setPixelColor(i, (uint32_t)PIXEL_COLOR_DARK);
        }
    }
    
    strip.setBrightness(PIXEL_BRIGHTNESS);
    strip.show();
}

void showRainbow()
{
    showingRainbow = true;
    
    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        strip.setPixelColor(i, (uint32_t)PIXEL_COLOR_DARK);
    }
    
    strip.show();
    
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 256; j++)
        {
            for (int k = 0; k < PIXEL_COUNT; k++)
            {
                strip.setPixelColor(k, colorWheel((k + j) & 255));
            }
        
            strip.show();
            strip.setBrightness(PIXEL_BRIGHTNESS);
            delay(20);
        }
    }
    
    showingRainbow = false;
    goalReached = false;
}

uint32_t colorWheel(byte pos)
{
    if (pos < 85)
    {
        return strip.Color(pos * 3, 255 - pos * 3, 0);
    }
    else if (pos < 170)
    {
        pos -= 85;
        return strip.Color(255 - pos * 3, 0, pos * 3);
    }
    else
    {
        pos -= 170;
        return strip.Color(0, pos * 3, 255 - pos * 3);
    }
}

void updateDisplay()
{
    // clear all pixels
    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        pixelDisplay[i] = false;
        pixelDimmed[i] = false;
        pixelColor[i] = (uint32_t)PIXEL_COLOR_DARK;
    } 
    
    // determine which goals and how many are enabled
    int8_t totalGoalsEnabled = 0;
    int8_t enabledGoals[4];
    for (int i = 0; i < 4; i++)
    {
        int addr = i * ST_OFFSET;
        int8_t enabled = EEPROM.read(addr);
        enabledGoals[i] = enabled;
        if (enabled == 1)
        {
            totalGoalsEnabled++;    
        }
    }

    // set appropriate pixel 'step'
    int step = 32;
    if (totalGoalsEnabled == 2)
    {
        step = 16;
    }
    else if (totalGoalsEnabled > 2)
    {
        step = 8;
    }
    
    // populate display arrays
    int stepCount = 0;
    for (int i = 0; i < 4; i++)
    {
        if (enabledGoals[i] == 1)
        {
            uint32_t color = getGoalColor(i);
    
            int level = 0;
            float value = getGoalValue(i);
            if (value > 0) {
                level = max(floor(value * step), 1);
            }
            
            int start = stepCount * step;
            for (int j = start; j < (start + level); j++)
            {
                pixelDisplay[j] = true;
                pixelColor[j] = color;
            }
            
            int promiseLevel = ceil(getGoalPromise(i) * step);
            if (promiseLevel > 0)
            {
                int promiseStart = start + level;
                for (int j = promiseStart; j < (promiseStart + promiseLevel); j++)
                {
                    pixelDimmed[j] = true;
                    pixelColor[j] = color;
                }
            }
            
            stepCount++;
        }
    }
}

// EEPROM Functions

float getGoalValue(int goalIndex)
{
    float value;
    int addr = (goalIndex * ST_OFFSET) + ST_OFFSET_VALUE;
    EEPROM.get(addr, value);
    if (value == ST_4B_EMPTY)
    {
        value = 0.0;
    }

    return value;
}

float getGoalPromise(int goalIndex)
{
    float value;
    int addr = (goalIndex * ST_OFFSET) + ST_OFFSET_PROMISE;
    EEPROM.get(addr, value);
    if (value == ST_4B_EMPTY)
    {
        value = 0.0;
    }

    return value;
}

uint32_t getGoalColor(int goalIndex)
{
    uint32_t value;
    int addr = (goalIndex * ST_OFFSET) + ST_OFFSET_COLOR;
    EEPROM.get(addr, value);
    if (value == ST_4B_EMPTY)
    {
        value = (uint32_t)PIXEL_COLOR_DEFAULT;
    }

    return value;
}

// Event & Function Handlers

int goalClear(String command)
{
    EEPROM.clear();
    updateDisplay();
    Particle.publish("device/clear", PRIVATE);
    return 0;
}

int goalReset(String command)
{
    goalToggle("0|0|0|0");
    goalUpdate("0.00,0.00|0.00,0.00|0.00,0.00|0.00,0.00");
    updateDisplay();
    Particle.publish("device/reset", PRIVATE);
    return 0;
}

int goalToggle(String command) // ex: 1|1|0|0
{
    // TODO: add better validation since device is being contacted
    // directly from the client application
    
    char buffer[8];
    command.toCharArray(buffer, sizeof(buffer));
    for (int i = 0; i < 4; i++)
    {
        int8_t enabled;
        if (i == 0)
        {
            enabled = atoi(strtok(buffer, "|"));
        }
        else
        {
            enabled = atoi(strtok(NULL, "|"));
        }
        
        int addr = i * ST_OFFSET;
        EEPROM.write(addr, enabled);
    }
    
    updateDisplay();
    Particle.publish("device/toggle", command, PRIVATE);
    return 0;
}

int goalUpdate(String command) // ex: 0.50,0.00|0.80,0.00|0.20,0.20|0.60,0.20
{
    // TODO: add better validation since device is being contacted
    // directly from the client application
    
    bool full = false;
    char commandBuffer[40];
    command.toCharArray(commandBuffer, sizeof(commandBuffer));
    String values[4];
    for (int i = 0; i < 4; i++)
    {
        char * goalAndPromise;
        if (i == 0)
        {
            goalAndPromise = strtok(commandBuffer, "|");
        }
        else
        {
            goalAndPromise = strtok(NULL, "|");
        }
        
        String temp = goalAndPromise;
        values[i] = temp;
    }
    
    char valueBuffer[10];
    for (int i = 0; i < 4; i++)
    {
        values[i].toCharArray(valueBuffer, sizeof(valueBuffer));
        
        int goalOffset = i * ST_OFFSET;
        float previousGoal = getGoalValue(i);
        float goal = min(atof(strtok(valueBuffer, ",")), 1.0);
        int goalAddr = goalOffset + ST_OFFSET_VALUE;
        EEPROM.put(goalAddr, goal);

        if (goal == 1.0 && previousGoal < 1.0)
        {
            int8_t enabled = EEPROM.read(goalOffset);
            if (enabled)
            {
                full = true;
            }
        }
        
        float promise = min(atof(strtok(NULL, ",")), (1.0 - goal));
        int promiseAddr = goalOffset + ST_OFFSET_PROMISE;
        EEPROM.put(promiseAddr, promise);
    }
    
    goalReached = full;
    updateDisplay();
    Particle.publish("device/update", command, PRIVATE);
    return 0;
}

int goalColor(String command) // ex: 16763955|1073100|43087|14422029
{
    // TODO: add better validation since device is being contacted
    // directly from the client application
    
    char buffer[36];
    command.toCharArray(buffer, sizeof(buffer));
    for (int i = 0; i < 4; i++)
    {
        int color;
        if (i == 0)
        {
            color = atoi(strtok(buffer, "|"));
        }
        else
        {
            color = atoi(strtok(NULL, "|"));
        }
        
        int addr = (i * ST_OFFSET) + ST_OFFSET_COLOR;
        EEPROM.put(addr, color);
    }
    
    updateDisplay();
    Particle.publish("device/color", command, PRIVATE);
    return 0;
}