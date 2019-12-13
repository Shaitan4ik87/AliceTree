#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <FastLED.h>
#include <WiFiMulti.h>

// wifi
const char* ssid     = "Spider";
const char* password = "12345679";
const char* server = "https://server-address.com";
WiFiMulti wifiMulti;

// led
#define NUM_LEDS 60
#define BRIGHTNESS 200
#define LED_PIN 13
CRGB leds[NUM_LEDS];

// colors
#define NUM_COLORS 24
bool rainbow = false;
byte code = 10;
byte colors[NUM_COLORS][3];
byte slowness = 1;
byte partSize = 5;

// animation
unsigned int step = 0;
byte speedStep = 0;
bool gradient = false;

// other
unsigned long lastLoaded = 0;
#define RELOAD_DELAY_MS 1000
#define LOOP_DELAY 200

// declarations
void setMode(const String& s);
void loadMode(unsigned long time);
void animateStep();
long hexToLong(const String& s);
int mixColors(int color1, int color2, float ratio);

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);

    wifiMulti.addAP(ssid, password);
}

void loop() {
    unsigned long ct = millis();

    if (lastLoaded > ct) {
        // clear timers because of overflow
        lastLoaded = 0;
        return;
    }

    loadMode(ct);

    FastLED.clear();
    if (code < 10) {
        animateStep();
    } else {
        // set white
        for(int i = 0; i < NUM_LEDS; i++) {
            leds[i].setRGB(255, 255, 255);
        }
    }
    FastLED.show();

    unsigned long after = millis();
    unsigned int diff = after - ct;
    if (diff < LOOP_DELAY) {
        delay(LOOP_DELAY - diff);
    }
}

/**
 * Get current mode string from http server
 * @param time current time on milliseconds
 */
void loadMode(unsigned long time) {
    if (lastLoaded < time - RELOAD_DELAY_MS) {
        lastLoaded = time;

        if ((wifiMulti.run() == WL_CONNECTED)) {
            HTTPClient http;
            Serial.print("[HTTP] begin...\n");

            http.begin(server);
            Serial.print("[HTTP] GET...\n");
            int httpCode = http.GET();

            if (httpCode > 0) {
                Serial.printf("[HTTP] GET... code: %d\n", httpCode);

                if (httpCode == HTTP_CODE_OK) {
                    String payload = http.getString();
                    Serial.println(payload);

                    setMode(payload);
                }
            } else {
                Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            }

            http.end();
        }
    }
}

/**
 * Convert loaded string to led's colors and animation type
 * @param s
 */
void setMode(const String& s) {
    byte newCode = s.substring(0, 1).toInt();
    if (newCode == code) {
        // same mode as current
        return;
    }

    speedStep = 0;
    step = 0;

    code = newCode;
    slowness = s.substring(1, 3).toInt();
    partSize = s.substring(3, 5).toInt();
    if (partSize == 0) {
        partSize = NUM_LEDS;
    }
    gradient = s.substring(5, 6) == "1";

    // load colors
    int lastLed = 0;
    int lastColor = 0;
    String cStr = "";

    // rainbow special mode
    if (s.substring(6, 7) == "-") {
        rainbow = true;
        partSize = max(1, partSize / 8);
        return;
    }

    // normal colors mode
    rainbow = false;
    for (int i = 6; i < s.length(); i++) {
        char c = s.charAt(i);
        cStr += c;
        unsigned int cLen = cStr.length();
        if (cLen == 2 && lastLed < NUM_COLORS) {
            colors[lastLed][lastColor] = hexToLong(cStr);
            lastColor = ++lastColor % 3;
            cStr = "";
        }
    }
}

void animateStep() {
    unsigned int currentNumColors = rainbow ? 360 : NUM_COLORS;

    for (int i = 0; i < NUM_LEDS; i++) {
        int color = floor(((double)step + i) / currentNumColors);

        if (rainbow) {
            leds[i].setHSV(color, 100, 100);
        } else {
            // softly mix with next near the end
            int stepsToNext = partSize / 3;
            int inColorStep = min(stepsToNext, partSize - color % partSize);
            unsigned int nextColor = floor(((double)step + i + partSize) / currentNumColors);
            float mixRatio = (float)inColorStep / (float)stepsToNext;

            // set colors
            int new_r = mixColors(colors[color][0], colors[nextColor][0], mixRatio);
            int new_g = mixColors(colors[color][1], colors[nextColor][1], mixRatio);
            int new_b = mixColors(colors[color][2], colors[nextColor][2], mixRatio);
            float coeff = gradient ? 0.6 : 1.0;

            leds[i].r += round(((double)new_r - leds[i].r) * coeff);
            leds[i].g += round(((double)new_g - leds[i].g) * coeff);
            leds[i].b += round(((double)new_b - leds[i].b) * coeff);
        }
    }

    speedStep++;
    if (speedStep >= slowness) {
        if (gradient) {
            // next color stop
            step = (step + partSize) % (currentNumColors * partSize);
        } else {
            // next animation step
            step = ++step % (currentNumColors * partSize);
        }
        speedStep = 0;
    }
}

long hexToLong(const String& s){
    char c[s.length() + 1];
    s.toCharArray(c, s.length() + 1);
    return strtol(c, nullptr, 16);
}

int mixColors(int color1, int color2, float ratio) {
    return (int)round(ratio * (double)color1 + (1.0 - ratio * (double)color2));
}
