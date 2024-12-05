#include <Arduino.h>
#include "FastLED/src/FastLED.h"

class Mode {
    public:
    Mode() {}
    virtual void setParam(String& parameters) {
        switch (int(parameters.toInt()) % 10) {
            case 1:
                brightness = parameters.toInt() / 10;
                break;
            case 2:
                speed = parameters.toInt() / 10;
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
    byte calculateRunLight(byte* i, byte* gap, byte* tailSize, bool* direction, byte& run, uint16_t* delayRunTime) {
        if (uint32_t(millis() / int(float(delayTime) / speed * 100)) % (*delayRunTime / delayTime) == 0 && *i == 0) {
            run++;
            if (run > *gap)
                run = 0;
        }
        return direction ? max(0, 255 - max((*i + run) % (*gap + 1), 0) * 255 / (*tailSize + 1)) :
            max(0, 255 - max(*gap - (*gap + *i - run) % (*gap + 1), 0) * 255 / (*tailSize + 1));
    }

    private:
    /// настраиваемые параметры
    byte speed = 100;
    byte brightness = 100;
    protected:
    byte delayTime;
};

class Rainbow : public Mode {
    public:
    Rainbow(int delayTime, int delayRunLightTime) {
        this->delayTime = delayTime;
        this->delayRunLightTime = delayRunLightTime;
    }
    void setParam(String& parameters) override {
        Mode::setParam(parameters);
        switch (int(parameters.toInt() % 10)) {
            case 3:
                rainbowCount = parameters.toInt() / 10;
                break;
            case 4:
                twoSides = parameters.toInt() / 10;
                break;
            case 5:
                pastels = parameters.toInt() / 10;
                break;
            case 6:
                direction = parameters.toInt() / 10;
                break;
            case 7:
                isRunningLight = parameters.toInt() / 10;
                break;
            case 8:
                gap = parameters.toInt() / 10;
                break;
            case 9:
                tailSize = parameters.toInt() / 10;
                break;
        }
    }
    void calculate(CRGB* leds, const byte* ledCount) override {
        for (byte i = 0; i < *ledCount / (twoSides + 1) + twoSides * (*ledCount % 2); i++) {
            byte brightnessForEachLed = calculateRunLight(&i, &gap, &tailSize, &direction, run, &delayRunLightTime);
            leds[direction ? i : *ledCount / (twoSides + 1) - i - 1 + twoSides] =
                CHSV(base + i * (rainbowCount * 255 / *ledCount) * (twoSides + 1),
                    255 - pastels, isRunningLight ? brightnessForEachLed : 255);
            if (twoSides)
                leds[direction ? *ledCount - i - 1 : i + *ledCount / 2] =
                    CHSV(base + i * (rainbowCount * 255 / *ledCount) * 2,
                        255 - pastels, isRunningLight ? brightnessForEachLed : 255);
        }
        base++;
    }

    private:
    byte base = 0;
    byte run = 0;
    /// настраиваемые параметры
    uint16_t delayRunLightTime;
    byte rainbowCount = 2;
    byte pastels = 50;
    byte gap = 9;
    byte tailSize = 6;
    bool twoSides = true;
    bool direction = true;
    bool isRunningLight = true;
};

const byte NUM_LEDS = 11;
#define PIN 12

CRGB leds[NUM_LEDS];
Mode* mode[1];
String receive;

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);
    FastLED.addLeds<WS2812, PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    mode[0] = new Rainbow(20, 75);
}

void loop() {
    mode[0]->show(leds, &NUM_LEDS);
    delay(1);

    // получение с Serial порта всего необходимого
    if (Serial.available()) {
        receive += Serial.readString();
        if (receive[receive.length() - 1] == '\n') {
            mode[0]->setParam(receive);
            Serial.print(receive);

            receive = "";
        }
    }
}