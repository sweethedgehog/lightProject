#include <Arduino.h>
#include "FastLED/src/FastLED.h"

class Mode {
    public:
    Mode() {}
    virtual void setParam(String& parameters) {
        switch (int(parameters.toInt()) % 100) {
            case 1:
                brightness = parameters.toInt() / 100;
                break;
            case 2:
                speed = parameters.toInt() / 100;
                break;
        }
    }
    // для режимов, у которых должно быть начало (прописывается в самом режиме)
    virtual void startMode(){}
    // логика светодиодов (прописывается отдельно в каждом классе)
    virtual void calculate(CRGB* leds, const long* ledCount) {}
    // основная логика таймера и вызов функции calculate
    void show(CRGB* leds, const long* ledCount) {
        if (int(millis() % int(float(delayTime) / speed * 100)) <= 1) {
            calculate(leds, ledCount);
            FastLED.setBrightness(brightness);
            FastLED.show();
        }
    }
    // рассчитывает яркость светодиода для бегущего огонька (в HSV это 3-ий параметр)
    byte calculateRunLight(long* i, byte* gap, byte* tailSize, bool* direction, byte* run, uint16_t* delayRunTime) {
        if (uint32_t(millis() / int(float(delayTime) / speed * 100)) % (*delayRunTime / delayTime) == 0 && *i == 0) {
            (*run)++;
            if (*run > *gap)
                *run = 0;
        }
        return direction ? max(0, 255 - max((int(*i) + *run) % (*gap + 1), 0) * 255 / (*tailSize + 1)) :
            max(0, 255 - max(*gap - (*gap + int(*i) - *run) % (*gap + 1), 0) * 255 / (*tailSize + 1));
    }
    // рассчитывает оттенок для режимов, изпользующих радугу
    byte calculateRainbow(long i, byte* base, byte rainbowCount, const long* ledCount, bool twoSides) {
        if (i == 0)
            (*base)++;
        return *base + i * (rainbowCount * 255 / *ledCount) * (twoSides + 1);
    }

    private:
    /// настраиваемые параметры
    byte speed = 100;
    byte brightness = 50;
    protected:
    byte delayTime;
};

class SingleColor : public Mode {
    public: SingleColor(int delayTime, int delayRunLightTime) {
        this->delayTime = delayTime;
        this->delayRunLightTime = delayRunLightTime;
    }
    void setParam(String& parameters) override {
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
    void calculate(CRGB* leds, const long* ledCount) override {
        for (long i = 0; i < *ledCount / (twoSides + 1) + twoSides * (*ledCount % 2); i++) {
            byte brightnessForEachLed = calculateRunLight(&i, &gap, &tailSize, &direction, &run, &delayRunLightTime);
            byte rainbowShade = calculateRainbow(i, &base, rainbowCount, ledCount, twoSides);
            leds[direction ? i : *ledCount / (twoSides + 1) - i - 1 + twoSides] =
                CHSV(isRainbow ? rainbowShade : hue,
                    255 - saturation, isRunningLight ? float(brightnessForEachLed) / 255 * value : value);
            if (twoSides)
                leds[direction ? *ledCount - i - 1 : i + *ledCount / 2] =
                    CHSV(isRainbow ? rainbowShade : hue,
                        255 - saturation, isRunningLight ? float(brightnessForEachLed) / 255 * value : value);
        }
    }
    private:
    byte run = 0;
    byte base = 0;
    // настраиваемые параметры
    uint16_t delayRunLightTime;
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
    Party(long delayTime) {
        this->delayTime = delayTime;
    }
    void startMode() override {
        FastLED.clear();
        FastLED.show();
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
                minHue = parameters.toInt() / 100;
            break;
            case 12:
                maxHue = parameters.toInt() / 100;
            break;
        }
    }
    void calculate(CRGB* leds, const long* ledCount) override {
        long free = 0;
        for (long i = 0; i < *ledCount; i++) {
            leds[i] -= CHSV(0, 0, descendingStep);
            if (rgb2hsv_approximate(leds[i]).raw[2] == 0)
                free++;
        }
        while (free - *ledCount / ratio > 0) {
            long i = random(0, *ledCount);
            if (rgb2hsv_approximate(leds[i]).raw[2] == 0) {
                leds[i] = CHSV(random(minHue, maxHue) + base * isRainbowGoing,
                    random(minSaturation, maxSaturation), random(minBrightness, maxBrightness));
                free--;
            }
        }
        base += baseStep;
    }
    private:
    byte base = 0;
    // настраиваемые параметры
    byte ratio = 2;
    byte descendingStep = 1;
    byte minSaturation = 0;
    byte maxSaturation = 255;
    byte minBrightness = 70;
    byte maxBrightness = 255;
    bool isRainbowGoing = false;
    byte baseStep = 1;
    byte minHue = 0;
    byte maxHue = 255;
};

const long NUM_LEDS = 11;
#define PIN 12
#define FREE_PIN 34

CRGB leds[NUM_LEDS];
Mode* modes[2];
byte mode = 1;
String receive;

bool isActive = true;

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);

    pinMode(FREE_PIN, INPUT);
    randomSeed(analogRead(FREE_PIN));

    FastLED.addLeds<WS2812, PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    modes[0] = new SingleColor(20, 80);
    modes[1] = new Party(25);
}

void loop() {
    if (isActive)
        modes[mode]->show(leds, &NUM_LEDS);
    delay(1);

    // получение с Serial порта всего необходимого
    if (Serial.available()) {
        receive += Serial.readString();
        if (receive[receive.length() - 1] == '\n') {
            if (receive[0] == 'p')
                isActive = !isActive;
            modes[mode]->setParam(receive);
            Serial.print(receive);

            receive = "";
        }
    }
}