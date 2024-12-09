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
    virtual void calculate(CRGB* leds, const byte* ledCount) {}
    // основная логика таймера и вызов функции calculate
    void show(CRGB* leds, const byte* ledCount) {
        if (int(millis() % int(float(delayTime) / speed * 100)) <= 1) {
            calculate(leds, ledCount);
            FastLED.setBrightness(brightness);
            FastLED.show();
        }
    }
    // рассчитывает яркость светодиода для бегущего огонька (в HSV это 3-ий параметр)
    byte calculateRunLight(byte* i, byte* gap, byte* tailSize, bool* direction, byte* run, uint16_t* delayRunTime) {
        if (uint32_t(millis() / int(float(delayTime) / speed * 100)) % (*delayRunTime / delayTime) == 0 && *i == 0) {
            (*run)++;
            if (*run > *gap)
                *run = 0;
        }
        return direction ? max(0, 255 - max((*i + *run) % (*gap + 1), 0) * 255 / (*tailSize + 1)) :
            max(0, 255 - max(*gap - (*gap + *i - *run) % (*gap + 1), 0) * 255 / (*tailSize + 1));
    }
    // рассчитывает оттенок для режимов, изпользующих радугу
    byte calculateRainbow(byte i, byte* base, byte rainbowCount, const byte* ledCount, bool twoSides) {
        if (i == 0)
            (*base)++;
        return *base + i * (rainbowCount * 255 / *ledCount) * (twoSides + 1);
    }

    private:
    /// настраиваемые параметры
    byte speed = 100;
    byte brightness = 255;
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
                shade = parameters.toInt() / 100;
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
    void calculate(CRGB* leds, const byte* ledCount) override {
        for (byte i = 0; i < *ledCount / (twoSides + 1) + twoSides * (*ledCount % 2); i++) {
            byte brightnessForEachLed = calculateRunLight(&i, &gap, &tailSize, &direction, &run, &delayRunLightTime);
            byte rainbowShade = calculateRainbow(i, &base, rainbowCount, ledCount, twoSides);
            leds[direction ? i : *ledCount / (twoSides + 1) - i - 1 + twoSides] =
                CHSV(isRainbow ? rainbowShade : shade,
                    255 - saturation, isRunningLight ? float(brightnessForEachLed) / 255 * value : value);
            if (twoSides)
                leds[direction ? *ledCount - i - 1 : i + *ledCount / 2] =
                    CHSV(isRainbow ? rainbowShade : shade,
                        255 - saturation, isRunningLight ? float(brightnessForEachLed) / 255 * value : value);
        }
    }
    private:
    byte run = 0;
    byte base = 0;
    // настраиваемые параметры
    uint16_t delayRunLightTime;
    byte shade = 0, saturation = 0, value = 255;
    byte gap = 9;
    byte tailSize = 6;
    byte rainbowCount = 1;
    bool twoSides = true;
    bool direction = true;
    bool isRunningLight = true;
    bool isRainbow = true;
};

const byte NUM_LEDS = 11;
#define PIN 12

CRGB leds[NUM_LEDS];
Mode* modes[2];
byte mode = 0;
String receive;

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);
    FastLED.addLeds<WS2812, PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    modes[0] = new SingleColor(20, 80);
}

void loop() {
    modes[mode]->show(leds, &NUM_LEDS);
    delay(1);

    // получение с Serial порта всего необходимого
    if (Serial.available()) {
        receive += Serial.readString();
        if (receive[receive.length() - 1] == '\n') {
            modes[mode]->setParam(receive);
            Serial.print(receive);

            receive = "";
        }
    }
}