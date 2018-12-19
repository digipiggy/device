#include "math.h"
#include "neopixel.h"

#define CODE_PIN D6
#define CODE_WIFI 2
#define PIXEL_PIN D2
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

SYSTEM_THREAD(ENABLED);

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

String commands[] = { "reset", "toggle", "update", "color" };

bool pixelDisplay[PIXEL_COUNT];
bool pixelDimmed[PIXEL_COUNT];
uint32_t pixelColor[PIXEL_COUNT];

bool goalReached;
bool showingRainbow;
String disconnected = "";

void setup()
{
    Particle.subscribe(System.deviceID(), handleDeviceEvent, MY_DEVICES);
    
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
        if (disconnected == "")
        {
            disconnected = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);
        }
        
        showWarning(CODE_WIFI);
    }
    else
    {
        if (disconnected != "")
        {
            Particle.publish("device/online", disconnected, PRIVATE);
            disconnected = "";
        }
        
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
                strip.setColorDimmed(i, r, g, b, 128);
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

void handleDeviceEvent(const char *event, const char *data)
{
    String evt = event;
    String d = data;
    int eventIndex = evt.indexOf("/");
    if (eventIndex > -1)
    {
        String eventName = evt.substring(eventIndex + 1);
        
        int i = -1;
        for (i = 0; i <= arraySize(commands); i++)
        {
            if (eventName.equals(commands[i])) break;
        }
  
        switch (i)
        {
            case 0:
                goalReset(d);
                break;
                
            case 1:
                goalToggle(d);
                break;
                
            case 2:
                goalUpdate(d);
                break;
                
            case 3:
                goalColor(d);
                break;
        }
    }
}

int goalReset(String command)
{
    EEPROM.clear();
    updateDisplay();
    return 0;
}

int goalToggle(String command)
{
    // ex: 1|1|0|0
    char buffer[8];
    command.toCharArray(buffer, 8);
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
    return 0;
}

int goalUpdate(String command)
{
    if (command.length() > 0)
    {
        int valueIndex = command.indexOf("|");
        if (valueIndex > -1)
        {
            int promiseIndex = command.indexOf("|", valueIndex + 1);
            if (promiseIndex > -1)
            {
                int goal = command.substring(0, 1).toInt();
                float previousValue = getGoalValue(goal);
                
                float value = min(command.substring(valueIndex + 1, promiseIndex).toFloat(), 1.0);
                int valueAddr = ((goal - 1) * ST_OFFSET) + ST_OFFSET_VALUE;
                EEPROM.put(valueAddr, value);
                
                float promise = min(command.substring(promiseIndex + 1).toFloat(), (1.0 - value));
                int promiseAddr = ((goal - 1) * ST_OFFSET) + ST_OFFSET_PROMISE;
                EEPROM.put(promiseAddr, promise);

                updateDisplay();
                if (value == 1.0 && previousValue < 1.0)
                {
                    goalReached = true;
                }
                
                return 0;
            }
        }
    }
    
    return -1;
}

int goalColor(String command)
{
    if (command.length() > 0)
    {
        int index = command.indexOf("|");
        if (index > -1)
        {
            int goal = command.substring(0, 1).toInt();
            uint32_t value = command.substring(index + 1).toInt();
            
            int addr = (goal - 1) * ST_OFFSET + ST_OFFSET_COLOR;
            EEPROM.put(addr, value);

            updateDisplay();
            return 0;
        }
    }
    
    return -1;
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