#include <Arduino.h>
#include "FastLED/src/FastLED.h"

class Mode {
public:
    Mode() {}
    virtual void setParam(String &parameters) {
        switch (int(parameters.toInt()) % 100) {
            case 1:
                brightness = parameters.toInt() / 100;
                break;
            case 2:
                speed = parameters.toInt() / 100;
                break;
        }
    }
    // логика светодиодов (прописывается отдельно в каждом классе)
    virtual void calculate(CRGB *leds, const long *ledCount) {}
    // основная логика таймера и вызов функции calculate
    void show(CRGB *leds, const long *ledCount, uint32_t *timer) {
        if (millis() - *timer > float(delayTime) / speed * 100) {
            // debugTimer = millis(); // таймер для замера выполнения режима
            *timer = millis();
            calculate(leds, ledCount);
            FastLED.setBrightness(brightness);
            FastLED.show();
            // Serial.println("time: " + String(millis() - debugTimer)); // вывод таймера для замера выполнения режима
        }
    }
private:
    //******************************
    // unsigned long debugTimer = 0;
    //******************************
    /// настраиваемые параметры
    byte speed = 100;
    byte brightness = 50;
protected:
    // рассчитывает оттенок для режимов, использующих радугу
    byte calculateRainbow(long i, byte *base, byte rainbowCount, const long *ledCount, bool twoSides) {
        if (i == 0)
            (*base)++;
        return *base + i * (rainbowCount * 255 / *ledCount) * (twoSides + 1);
    }
    // рассчитывает яркость светодиода для бегущего огонька (в HSV это 3-й параметр)
    byte calculateRunLight(long i, byte gap, byte tailSize, bool direction,
        byte* run, uint16_t delayRunTime, byte* moveCounter) {
        if (i == 0) {
            if (/*uint32_t(millis() / int(float(delayTime) / speed * 100)) % (delayRunTime / delayTime) == 0*/
                *moveCounter >= delayRunTime / delayTime + bool(delayRunTime % delayTime) - 1) {
                *moveCounter = 0;
                (*run)++;
                if (*run > gap)
                    *run = 0;
                }
            else
                (*moveCounter)++;
        }
        return direction ? max(0, 255 - max((int(i) + *run) % (gap + 1), 0) * 255 / (tailSize + 1)) :
            max(0, 255 - max(gap - (gap + int(i) - *run) % (gap + 1), 0) * 255 / (tailSize + 1));
    }
    static long randomIntSeeded(long minValue, long maxValue, uint32_t seed, byte a = 0, byte b = 0) {
        return minValue + (seed ^ ((214013 - a) * seed + (2531011 - b) >> 15)) % (maxValue - minValue + 1);
    }

    byte delayTime;
};

class SingleColor : public Mode {
public:
    SingleColor(byte delayTime, byte delayRunLightTime) {
        this->delayTime = delayTime;
        this->runLightDelay = delayRunLightTime;
    }
    void setParam(String &parameters) override {
        Mode::setParam(parameters);
        switch (int(parameters.toInt() % 100)) {
            case 3:
                hue = parameters.toInt() / 100;
                break;
            case 4:
                saturation = parameters.toInt() / 100;
                break;
            case 5:
                value = parameters.toInt() / 100;
                break;
            case 6:
                gap = parameters.toInt() / 100;
                break;
            case 7:
                tailSize = parameters.toInt() / 100;
                break;
            case 8:
                rainbowCount = parameters.toInt() / 100;
                break;
            case 9:
                twoSides = bool(parameters.toInt() / 100);
                break;
            case 10:
                direction = bool(parameters.toInt() / 100);
                break;
            case 11:
                isRunningLight = bool(parameters.toInt() / 100);
                break;
            case 12:
                isRainbow = bool(parameters.toInt() / 100);
                break;
        }
    }
    void calculate(CRGB *leds, const long *ledCount) override {
        for (long i = 0; i < *ledCount / (twoSides + 1) + twoSides * (*ledCount % 2); i++) {
            byte brightnessForLed = calculateRunLight(i, gap, tailSize, direction, &run, runLightDelay, &moveCounter);
            byte rainbowHue = calculateRainbow(i, &base, rainbowCount, ledCount, twoSides);
            leds[direction ? i : *ledCount / (twoSides + 1) - i - 1 + twoSides] =
                    CHSV(isRainbow ? rainbowHue : hue,
                         255 - saturation, isRunningLight ? float(brightnessForLed) / 255 * value : value);
            if (twoSides)
                leds[direction ? *ledCount - i - 1 : i + *ledCount / 2] =
                        CHSV(isRainbow ? rainbowHue : hue,
                             255 - saturation, isRunningLight ? float(brightnessForLed) / 255 * value : value);
        }
    }
private:
    byte run = 0;
    byte moveCounter = 0;
    byte base = 0;
    /// настраиваемые параметры
    byte runLightDelay;
    byte hue = 0, saturation = 0, value = 255;
    byte gap = 9;
    byte tailSize = 6;
    byte rainbowCount = 1;
    bool twoSides = true;
    bool direction = true;
    bool isRunningLight = true;
    bool isRainbow = true;
};

class Party : public Mode {
public:
    Party(byte delayTime) {
        this->delayTime = delayTime;
        FastLED.clear();
    }
    void setParam(String &parameters) {
        Mode::setParam(parameters);
        switch (int(parameters.toInt() % 100)) {
            case 3:
                ratio = parameters.toInt() / 100;
                break;
            case 4:
                descendingStep = parameters.toInt() / 100;
                break;
            case 5:
                minSaturation = parameters.toInt() / 100;
                break;
            case 6:
                maxSaturation = parameters.toInt() / 100;
                break;
            case 7:
                minBrightness = parameters.toInt() / 100;
                break;
            case 8:
                maxBrightness = parameters.toInt() / 100;
                break;
            case 9:
                isRainbowGoing = bool(parameters.toInt() / 100);
                break;
            case 10:
                baseStep = parameters.toInt() / 100;
                break;
            case 11:
                hueGap = parameters.toInt() / 100;
                break;
        }
    }
    void calculate(CRGB *leds, const long *ledCount) override {
        long free = 0;
        for (long i = 0; i < *ledCount; i++) {
            leds[i] -= CHSV(0, 0, descendingStep);
            if (rgb2hsv_approximate(leds[i]).raw[2] == 0)
                free++;
        }
        while (free - *ledCount / ratio > 0) {
            long i = random(0, *ledCount);
            if (rgb2hsv_approximate(leds[i]).raw[2] == 0) {
                leds[i] = CHSV(random(255 - hueGap, 256) + base * isRainbowGoing,
                               255 - random(minSaturation, maxSaturation), random(minBrightness, maxBrightness));
                free--;
            }
        }
        base += baseStep;
    }
private:
    byte base = 0;
    /// настраиваемые параметры
    byte ratio = 2;
    byte descendingStep = 1;
    byte minSaturation = 0;
    byte maxSaturation = 255;
    byte minBrightness = 70;
    byte maxBrightness = 255;
    bool isRainbowGoing = true;
    byte baseStep = 1;
    byte hueGap = 10;
};

class PerlinShow : public Mode {
public:
    PerlinShow(byte delayTime, byte runLightDelay) {
        this->delayTime = delayTime;
        this->runLightDelay = runLightDelay;
        seed = random(4294967295);
        randParamA = random(256);
        randParamB = random(256);
    }
    void setParam(String &parameters) override {
        Mode::setParam(parameters);
        switch (int(parameters.toInt() % 100)) {
            case 3:
                gridWidth = parameters.toInt() / 100;
                break;
            case 4:
                isCircle = parameters.toInt() / 100;
                break;
            case 5:
                yScale = parameters.toInt() / 100; // тысячные
            case 6:
                hueScale = parameters.toInt() / 100;
                break;
            case 7:
                hueOffset = parameters.toInt() / 100;
                break;
            case 8:
                saturation = parameters.toInt() / 100;
                break;
            case 9:
                isRunningLight = parameters.toInt() / 100;
                break;
            case 10:
                gap = parameters.toInt() / 100;
                break;
            case 11:
                tailSize = parameters.toInt() / 100;
                break;
            case 12:
                direction = parameters.toInt() / 100;
                break;
        }
    }
    void calculate(CRGB *leds, const long *ledCount) override {
        for (long i = 0; i < *ledCount; i++) {
            leds[i] = CHSV((perlinNoise(i, float(gridWidth) / *ledCount, float(yScale) / 1000) + 1)
                * hueScale + hueOffset,255 - saturation, !isRunningLight ? 255 : i <= *ledCount / 2 ?
                calculateRunLight(i, gap, tailSize, direction, &run, runLightDelay, &moveCounter) :
                calculateRunLight(*ledCount - i, gap, 3, direction, &run, runLightDelay, &moveCounter));
        }
        y++;
    }
private:
    float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }
    float qunticCurve(float t) {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }
    float perlinNoise(int x, float xScale, float yScale) {
        struct Vector2 {
            float x, y;
            Vector2(float x, float y) : x(x), y(y) {}
            Vector2(long seed, byte randA, byte randB) {
                x = randomIntSeeded(0, 7, seed, randA, randB) % 3 - 1;
                y = randomIntSeeded(0, 7, seed, randA, randB) / 3 - 1;
                if (x == 0 && y == 0) {x = 1; y = 1;}
            }
            float operator*(Vector2 a) {return a.x * x + a.y * y;}
        };
        return lerp(lerp(Vector2(float(x) * xScale - floor(float(x) * xScale),
                                 float(y) * yScale - floor(float(y) * yScale)) * Vector2(
                             seed + floor(float(x) * xScale) + floor(float(y) * yScale) * gridWidth,
                             randParamA, randParamB), Vector2(float(x) * xScale - floor(float(x) * xScale) - 1,
                                 float(y) * yScale - floor(float(y) * yScale))
                         * Vector2(seed + (isCircle ? int(floor(float(x) * xScale) + 1) % gridWidth
                            : int(floor(float(x) * xScale) + 1)) + floor(float(y) * yScale) * gridWidth, randParamA,
                            randParamB), qunticCurve(float(x) * xScale - floor(float(x) * xScale))), lerp(
                        Vector2(float(x) * xScale - floor(float(x) * xScale),
                                float(y) * yScale - floor(float(y) * yScale) - 1) * Vector2(
                            seed + floor(float(x) * xScale) + (floor(float(y) * yScale) + 1) * gridWidth,
                            randParamA, randParamB), Vector2(float(x) * xScale - floor(float(x) * xScale) - 1,
                                float(y) * yScale - floor(float(y) * yScale) - 1) * Vector2(
                                seed + (isCircle ? int(floor(float(x) * xScale) + 1) % gridWidth
                                    : int(floor(float(x) * xScale) + 1)) + (floor(float(y) * yScale) + 1) * gridWidth,
                                    randParamA, randParamB), qunticCurve(float(x) * xScale - floor(float(x) * xScale))),
                    qunticCurve(float(y) * yScale - floor(float(y) * yScale)));
    }

    uint32_t seed;
    byte randParamA;
    byte randParamB;
    uint32_t y = 0;
    byte run = 0;
    byte moveCounter = 0;
    /// настраиваемые параметры
    byte runLightDelay;
    byte gridWidth = 2;
    bool isCircle = true;
    byte yScale = 7; // тысячные
    byte hueScale = 230;
    byte hueOffset = 0;
    byte saturation = 0;
    bool isRunningLight = false;
    byte gap = 5;
    byte tailSize = 3;
    bool direction = false;
};

class IridescentLights : public Mode {
    public:
    IridescentLights(byte delayTime, byte runLightDelay, const long *ledsCount, CRGB *leds) {
        this->delayTime = delayTime;
        this->runLightDelay = runLightDelay;
        step = random(4294967295);
        for (long i = 0; i < *ledsCount; i++) {
            leds[i] = CHSV(random(256), 255 - saturation, 255);
        }
        target = randomIntSeeded(0, maxCount, step);
    }
    void setParam(String &parameters) override {
        Mode::setParam(parameters);
        switch (int(parameters.toInt() % 100)) {
            case 3:
                saturation = parameters.toInt() / 100;
                break;
            case 4:
                minCount = parameters.toInt() / 100;
                break;
            case 5:
                maxCount = parameters.toInt() / 100;
                break;
        }
    }
    void calculate(CRGB *leds, const long *ledsCount) override {
        for (long i = 0; i < *ledsCount; i++) {
            if (count == target) {
                count = 0;
                step += *ledsCount;
                target = randomIntSeeded(minCount, maxCount, step);
            }
            leds[i] = CHSV((randomIntSeeded(0, 1, i + step) * 2 - 1) * 3 +
                rgb2hsv_approximate(leds[i])[0],255 - saturation, 255);
        }
        count++;
    }
    private:
    byte target;
    uint32_t step = 0;
    byte count = 0;
    /// настраиваемые параметры
    byte runLightDelay;
    byte saturation = 0;
    byte maxCount = 255;
    byte minCount = 1;
};

const long NUM_LEDS = 61;
#define PIN 12
#define FREE_PIN 34

CRGB leds[NUM_LEDS];
Mode *mode;
byte currMode = 2;
uint32_t modeTimer = 0;
String receive;

bool isActive = true;

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);

    pinMode(FREE_PIN, INPUT);
    randomSeed(analogRead(FREE_PIN));

    FastLED.addLeds<WS2812, PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    switch (currMode) {
        case 0:
            mode = new SingleColor(20, 80);
            break;
        case 1:
            mode = new Party(25);
            break;
        case 2:
            mode = new PerlinShow(20, 80);
            break;
        case 3:
            mode = new IridescentLights(20, 80, &NUM_LEDS, leds);
            break;
    }
}

void loop() {
    if (isActive)
        mode->show(leds, &NUM_LEDS, &modeTimer);
    delay(1);

    // получение с Serial порта всего необходимого
    if (Serial.available()) {
        receive += Serial.readString();
        if (receive[receive.length() - 1] == '\n') {
            if (receive[0] == 'p')
                isActive = !isActive;
            mode->setParam(receive);
            Serial.print(receive);

            receive = "";
        }
    }
}
