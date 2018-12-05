#include "math.h"
#include "neopixel.h"

#define CODE_PIN A1
#define CODE_WIFI 2
#define PIXEL_PIN D2
#define PIXEL_COUNT 32
#define PIXEL_TYPE WS2812B
#define ST_4B_EMPTY 0xFFFFFFFF
#define ST_OFFSET 200
#define ST_OFFSET_VALUE 1
#define ST_OFFSET_PROMISE 5
#define ST_OFFSET_COLOR 9
#define PIXEL_COLOR_DARK 0
#define PIXEL_COLOR_WIFI 255
#define PIXEL_COLOR_DEFAULT 2677760

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
    
    Particle.function("reset", bucketReset);
    Particle.function("toggle", bucketToggle);
    Particle.function("update", bucketUpdate);
    Particle.function("color", bucketColor);
    
    pinMode(CODE_PIN, OUTPUT);
    updateDisplay();
    
    strip.begin();
    strip.show();
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
                bucketReset(d);
                break;
                
            case 1:
                bucketToggle(d);
                break;
                
            case 2:
                bucketUpdate(d);
                break;
                
            case 3:
                bucketColor(d);
                break;
        }
    }
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
            showBuckets();
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

void showBuckets()
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
    
    strip.setBrightness(30);
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
            strip.setBrightness(30);
            delay(20);
        }
    }
    
    showingRainbow = false;
    goalReached = false;
}

void updateDisplay()
{
    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        pixelDisplay[i] = false;
        pixelDimmed[i] = false;
        pixelColor[i] = (uint32_t)PIXEL_COLOR_DARK;
    } 
    
    int step = 32;
    int buckets = getBucketsEnabled();
    if (buckets == 2) {
        step = 16;
    } else if (buckets > 2) {
        step = 8;
    }
    
    for (int i = 1; i <= buckets; i++)
    {
        uint32_t color = getBucketColor(i);

        int level = floor(getBucketValue(i) * step);
        int start = (i - 1) * step;
        for (int j = start; j < (start + level); j++)
        {
            pixelDisplay[j] = true;
            pixelColor[j] = color;
        }
        
        int promiseLevel = ceil(getBucketPromise(i) * step);
        if (promiseLevel > 0)
        {
            int promiseStart = start + level;
            for (int j = promiseStart; j < (promiseStart + promiseLevel); j++)
            {
                pixelDimmed[j] = true;
                pixelColor[j] = color;
            }
        }
    }
}

int getBucketsEnabled()
{
    int result = 0;
    for (int i = 0; i < 4; i++)
    {
        int addr = i * ST_OFFSET;
        int8_t enabled = EEPROM.read(addr);
        result += enabled;
    }
    
    return result;
}

float getBucketValue(int bucket)
{
    float value;
    int addr = ((bucket - 1) * ST_OFFSET) + ST_OFFSET_VALUE;
    EEPROM.get(addr, value);
    if (value == ST_4B_EMPTY)
    {
        value = 0.0;
    }

    return value;
}

float getBucketPromise(int bucket)
{
    float value;
    int addr = ((bucket - 1) * ST_OFFSET) + ST_OFFSET_PROMISE;
    EEPROM.get(addr, value);
    if (value == ST_4B_EMPTY)
    {
        value = 0.0;
    }

    return value;
}

uint32_t getBucketColor(int bucket)
{
    uint32_t value;
    int addr = ((bucket - 1) * ST_OFFSET) + ST_OFFSET_COLOR;
    EEPROM.get(addr, value);
    if (value == ST_4B_EMPTY)
    {
        value = (uint32_t)PIXEL_COLOR_DEFAULT;
    }

    return value;
}

int bucketReset(String command)
{
    EEPROM.clear();
    updateDisplay();
    return 0;
}

int bucketToggle(String command)
{
    if (command.length() > 0)
    {
        int index = command.indexOf("|");
        if (index > -1)
        {
            int bucket = command.substring(0, 1).toInt();
            int8_t value = command.substring(index + 1).toInt();
            
            if (value == 1) {
                for (int i = (bucket - 1); i >= 0; i--) {
                    int addr = i * ST_OFFSET;
                    EEPROM.write(addr, value);
                }
            }
            else
            {
                for (int i = (bucket - 1); i < 4; i++) {
                    int addr = i * ST_OFFSET;
                    EEPROM.write(addr, value);
                }
            }

            updateDisplay();
            return 0;
        }
    }
    
    return -1;
}

int bucketUpdate(String command)
{
    if (command.length() > 0)
    {
        int valueIndex = command.indexOf("|");
        if (valueIndex > -1)
        {
            int promiseIndex = command.indexOf("|", valueIndex + 1);
            if (promiseIndex > -1)
            {
                int bucket = command.substring(0, 1).toInt();
                float previousValue = getBucketValue(bucket);
                
                float value = min(command.substring(valueIndex + 1, promiseIndex).toFloat(), 1.0);
                int valueAddr = ((bucket - 1) * ST_OFFSET) + ST_OFFSET_VALUE;
                EEPROM.put(valueAddr, value);
                
                float promise = min(command.substring(promiseIndex + 1).toFloat(), (1.0 - value));
                int promiseAddr = ((bucket - 1) * ST_OFFSET) + ST_OFFSET_PROMISE;
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

int bucketColor(String command)
{
    if (command.length() > 0)
    {
        int index = command.indexOf("|");
        if (index > -1)
        {
            int bucket = command.substring(0, 1).toInt();
            uint32_t value = command.substring(index + 1).toInt();
            
            int addr = (bucket - 1) * ST_OFFSET + ST_OFFSET_COLOR;
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