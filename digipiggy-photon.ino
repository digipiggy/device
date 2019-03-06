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
#define ST_USE_ONOFF 802
#define ST_ON_OFFSET 803
#define ST_OFF_OFFSET 805
#define ST_TIMEZONE_OFFSET 807
#define ST_DST_OFFSET 808
#define ST_MINUTE_OFFSET 1
#define PIXEL_COLOR_DARK 0
#define PIXEL_COLOR_DEFAULT 1073100

//PRODUCT_ID();
//PRODUCT_VERSION(4);
SYSTEM_THREAD(ENABLED);

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

bool pixelDisplay[PIXEL_COUNT];
bool pixelDimmed[PIXEL_COUNT];
uint32_t pixelColor[PIXEL_COUNT];

bool isNew;
bool goalReached;
bool showingRainbow;
// System Functions

Timer timeSync(3600000, updateTime);

void setup()
{
    System.set(SYSTEM_CONFIG_SOFTAP_PREFIX, "DIGIPIGGY");

    String event = System.deviceID();
    event.concat("device/reset");
    Particle.subscribe(event, deviceReset, MY_DEVICES);

    Particle.function("reset", goalReset);
    Particle.function("toggle", goalToggle);
    Particle.function("update", goalUpdate);
    Particle.function("color", goalColor);
    Particle.function("piggysleep", setPiggySleep);

    timeSync.start();
    pinMode(CODE_PIN, OUTPUT);
    updateDisplay();
    strip.begin();
    strip.show();

}

void loop()
{
    if (!Particle.connected())
    {
        if (WiFi.listening())
        {
            showListening();
        }
        else
        {
            showWarning(CODE_WIFI);
        }
    }
    else
    {
        if (isNew)
        {
            showHello();
        }
        else if (!showDisplay())
        {
            clearDisplay();
        }
        else if (goalReached && !showingRainbow)
        {
            showRainbow();
        }
        else
        {
            showGoals();
        }
    }
}


bool isDst()
{
    int month = Time.month();
    int day = Time.day();
    int dow = Time.weekday() - 1;
    //https://stackoverflow.com/questions/5590429/calculating-daylight-saving-time-from-only-date
    //January, february, and december are out.
    if (month < 3 || month > 11) { return false; }
    //April to October are in
    if (month > 3 && month < 11) { return true; }
    int previousSunday = day - dow;
    //In march, we are DST if our previous sunday was on or after the 8th.
    if (month == 3) { return previousSunday >= 8; }
    //In november we must be before the first sunday to be dst.
    //That means the previous sunday must be before the 1st.
    return previousSunday <= 0;
}

void updateTime()
{
    int8_t timezoneOffset = EEPROM.read(ST_TIMEZONE_OFFSET);
    uint8_t observesDst = EEPROM.read(ST_DST_OFFSET);

    Time.zone(timezoneOffset);
    if (observesDst == 1 && isDst()) {
        Time.beginDST();
    } else {
        Time.endDST();
    }
}
// Display Functions

bool showDisplay() {
    uint8_t useOnOff = EEPROM.read(ST_USE_ONOFF);

    uint8_t onHour = EEPROM.read(ST_ON_OFFSET);
    uint8_t onMinute = EEPROM.read(ST_ON_OFFSET + ST_MINUTE_OFFSET);
    uint8_t offHour = EEPROM.read(ST_OFF_OFFSET);
    uint8_t offMinute = EEPROM.read(ST_OFF_OFFSET + ST_MINUTE_OFFSET);

    int onMinutes = onHour * 60 + onMinute;
    int offMinutes = offHour * 60 + offMinute;
    int currentMinutes = Time.hour() * 60 + Time.minute();


    if (useOnOff != 1) {
        return true;
    }

    if (onMinutes <= currentMinutes && offMinutes > currentMinutes)
    {
        return true;
    }

    return false;

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

void clearDisplay()
{
    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        strip.setPixelColor(i, (uint32_t)PIXEL_COLOR_DARK);
    }

    strip.show();
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

    clearDisplay();

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 256; j++)
        {
            for (int k = 0; k < PIXEL_COUNT; k++)
            {
                strip.setPixelColor(k, colorWheel((k + j) & 255));
            }

            strip.setBrightness(PIXEL_BRIGHTNESS);
            strip.show();
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

void showListening()
{
    clearDisplay();

    int pixels[] = {0, 1, 8, 9, 10, 11, 16, 17, 18, 19, 20, 21, 24, 25, 26, 27, 28, 29, 30, 31};
    for (int i = 0; i < arraySize(pixels); i++)
    {
        strip.setPixelColor(pixels[i], (uint32_t)PIXEL_COLOR_DEFAULT);
    }

    strip.setBrightness(PIXEL_BRIGHTNESS);
    strip.show();
}

void showHello()
{
    clearDisplay();

    int pixels[] = {0, 1, 2, 3, 4, 5, 6, 7, 11, 12, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 30, 31};
    for (int i = 0; i < arraySize(pixels); i++)
    {
        strip.setPixelColor(pixels[i], (uint32_t)PIXEL_COLOR_DEFAULT);
    }

    strip.setBrightness(PIXEL_BRIGHTNESS);
    strip.show();
}

void updateDisplay()
{
    isNew = getIsNew();

    // clear all pixels
    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        pixelDisplay[i] = false;
        pixelDimmed[i] = false;
        pixelColor[i] = (uint32_t)PIXEL_COLOR_DARK;
    }

    // determine which goals and how many are enabled
    uint8_t totalGoalsEnabled = 0;
    uint8_t enabledGoals[4];
    for (int i = 0; i < 4; i++)
    {
        int addr = i * ST_OFFSET;
        uint8_t enabled = EEPROM.read(addr);
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
            if (value > 0)
            {
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

void setIsNew(uint8_t value)
{
    int addr = (4 * ST_OFFSET) + ST_OFFSET_VALUE;
    EEPROM.write(addr, value);
}

bool getIsNew()
{
    int addr = (4 * ST_OFFSET) + ST_OFFSET_VALUE;
    uint8_t value = EEPROM.read(addr);
    if (value == ST_1B_EMPTY)
    {
        return true;
    }

    return false;
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



// Event & Function Handlers
int deviceReset(const char *eventName, const char *data)
{
    EEPROM.clear();
    updateDisplay();

    Particle.publish("device/reset", PRIVATE);

    WiFi.clearCredentials();
    WiFi.disconnect();
    WiFi.listen();
}

int setPiggySleep(String command) // 0|00:00|00:00|0|0 - enabled | ontime | offTime | timezoneOffset | observes dst
{
    _setPiggySleep(command);
    updateTime();
    Particle.publish("setPiggySleep", command, PRIVATE);

    return 0;
}

void _setPiggySleep(String command)
{
    char buffer[20];
    command.toCharArray(buffer, sizeof(buffer));

    uint8_t useOnOff;
    char * on;
    char * off;
    int8_t timezoneOffset;
    uint8_t observesDst;

    uint8_t onHour;
    uint8_t onMinute;
    uint8_t offHour;
    uint8_t offMinute;

    useOnOff = atoi(strtok(buffer, "|"));
    on = strtok(NULL, "|");
    off = strtok(NULL, "|");
    timezoneOffset = atoi(strtok(NULL, "|"));
    observesDst = atoi(strtok(NULL, "|"));

    onHour = atoi(strtok(on, ":"));
    onMinute = atoi(strtok(NULL, ":"));

    offHour = atoi(strtok(off, ":"));
    offMinute = atoi(strtok(NULL, ":"));

    EEPROM.write(ST_USE_ONOFF, useOnOff);
    EEPROM.write(ST_ON_OFFSET, onHour);
    EEPROM.write(ST_ON_OFFSET + ST_MINUTE_OFFSET, onMinute);
    EEPROM.write(ST_OFF_OFFSET, offHour);
    EEPROM.write(ST_OFF_OFFSET + ST_MINUTE_OFFSET, offMinute);
    EEPROM.write(ST_TIMEZONE_OFFSET, timezoneOffset);
    EEPROM.write(ST_DST_OFFSET, observesDst);
}


int goalReset(String command)
{
    _goalToggle("0|0|0|0");
    _goalUpdate("0.00,0.00|0.00,0.00|0.00,0.00|0.00,0.00");
    _setPiggySleep("0|00:00|00:00|0|0");
    setIsNew((uint8_t)ST_1B_EMPTY);

    updateDisplay();
    Particle.publish("goal/reset", PRIVATE);
    return 0;
}

int goalToggle(String command) // ex: 1|1|0|0
{
    // TODO: add better validation since device is being contacted
    // directly from the client application

    _goalToggle(command);

    setIsNew(0);
    updateDisplay();

    Particle.publish("goal/toggle", command, PRIVATE);
    return 0;
}

void _goalToggle(String command)
{
    char buffer[8];
    command.toCharArray(buffer, sizeof(buffer));
    for (int i = 0; i < 4; i++)
    {
        uint8_t enabled;
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
}

int goalUpdate(String command) // ex: 0.50,0.00|0.80,0.00|0.20,0.20|0.60,0.20
{
    // TODO: add better validation since device is being contacted
    // directly from the client application

    _goalUpdate(command);

    setIsNew(0);
    updateDisplay();

    Particle.publish("goal/update", command, PRIVATE);
    return 0;
}

void _goalUpdate(String command)
{
    bool full = false;
    char commandBuffer[40];
    command.toCharArray(commandBuffer, sizeof(commandBuffer));
    String values[4];
    for (int i = 0; i < 4; i++)
    {
        char *goalAndPromise;
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
            uint8_t enabled = EEPROM.read(goalOffset);
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

    setIsNew(0);
    updateDisplay();

    Particle.publish("goal/color", command, PRIVATE);
    return 0;
}
