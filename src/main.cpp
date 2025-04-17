#include <Arduino.h>
#include <NimBLE-Arduino/src/NimBLEDevice.h>
#include "NuS-NimBLE-Serial/src/NuSerial.hpp"
#include "FastLED/src/FastLED.h"
#include <SD.h>
#include <WiFi.h>
// #include "IRremote.h"

class Mode {
public:
    Mode() {}
    // установление отдельного параметра
    virtual void setParam(byte paramNumber, byte parameter) {
        switch (paramNumber) {
            case 1:
                brightness = parameter;
                break;
            case 2:
                speed = parameter;
                break;
        }
    }
    // установка сразу всех параметров (?) todo разобраться насколько оно надо
    void setAllParam(String parameters, char separateSymbol) {
        String buf;
        byte ind = 1;
        for (long i = 0; i < parameters.length(); i++) {
            if (parameters[i] != separateSymbol)
                buf += parameters[i];
            else {
                setParam(ind, buf.toInt());
                ind++;
                buf = "";
            }
        }
    }
    // получение всех параметров в нормальном виде (?) todo разобраться насколько оно надо
    virtual String getAllParameters(char separateSymbol, char endSymbol) {
        return String(brightness) + separateSymbol + String(speed) + separateSymbol;
    }
    // получение всех параметров
    virtual String getAllParameters() {
        return String() + char(brightness) + char(speed);
    }
    // логика светодиодов (прописывается отдельно в каждом классе)
    virtual void calculate(CRGB *leds, const long *ledCount) {}
    // основная логика таймера и вызов функции calculate
    void show(CRGB *leds, const long *ledCount, uint32_t *timer) {
        if (millis() - *timer > float(delayTime) / speed * 100) {
            // debugTimer = millis(); // логика таймера для замера времени выполнения
            *timer = millis();
            calculate(leds, ledCount);
            FastLED.setBrightness(brightness);
            FastLED.show();
            // Serial.println("time: " + String(millis() - debugTimer)); // вывод таймера для замера времени выполнения
        }
    }
private:
    // unsigned long debugTimer = 0; // таймер для определения времени выполнения
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
        byte* run, uint32_t delayRunTime, byte* moveCounter) {
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
    // генерирует случайное число от minValue до maxValue основываясь на введенном seed
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
    void setParam(byte numberParameter, byte parameters) override {
        Mode::setParam(numberParameter, parameters);
        switch (numberParameter) {
            case 3:
                hue = parameters;
                break;
            case 4:
                saturation = parameters;
                break;
            case 5:
                value = parameters;
                break;
            case 6:
                gap = parameters;
                break;
            case 7:
                tailSize = parameters;
                break;
            case 8:
                rainbowCount = parameters;
                break;
            case 9:
                twoSides = bool(parameters);
                break;
            case 10:
                direction = bool(parameters);
                break;
            case 11:
                isRunningLight = bool(parameters);
                break;
            case 12:
                isRainbow = bool(parameters);
                break;
        }
    }
    String getAllParameters(char separateSymbol, char endSymbol) override {
        return Mode::getAllParameters(separateSymbol, endSymbol) + String(hue) + separateSymbol +
            String(saturation) + separateSymbol + String(value) + separateSymbol + String(gap) + separateSymbol +
                String(tailSize) + separateSymbol + String(rainbowCount) + separateSymbol + String(int(twoSides)) +
                    separateSymbol + String(int(direction)) + separateSymbol + String(int(isRunningLight)) +
                        separateSymbol + String(int(isRainbow)) + endSymbol;
    }
    String getAllParameters() override {
        return Mode::getAllParameters() + char(hue) + char(saturation) + char(value) + char(gap) + char(tailSize)
            + char(rainbowCount) + char(twoSides) + char(direction) + char(isRunningLight) + char(isRainbow);
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
    void setParam(byte numberParameter, byte parameters) override {
        Mode::setParam(numberParameter, parameters);
        switch (numberParameter) {
            case 3:
                ratio = parameters;
                break;
            case 4:
                descendingStep = parameters;
                break;
            case 5:
                minSaturation = parameters;
                break;
            case 6:
                maxSaturation = parameters;
                break;
            case 7:
                minBrightness = parameters;
                break;
            case 8:
                maxBrightness = parameters;
                break;
            case 9:
                isRainbowGoing = bool(parameters);
                break;
            case 10:
                baseStep = parameters;
                break;
            case 11:
                minHue = parameters;
            break;
            case 12:
                maxHue = parameters;
            break;
        }
    }
    String getAllParameters(char separateSymbol, char endSymbol) override {
        return Mode::getAllParameters(separateSymbol, endSymbol) + String(ratio) + separateSymbol +
            String(descendingStep) + separateSymbol + String(minSaturation) + separateSymbol + String(maxSaturation) +
                separateSymbol + String(minBrightness) + separateSymbol + String(maxBrightness) + separateSymbol +
                    String(int(isRainbowGoing)) + separateSymbol + String(baseStep) + separateSymbol + String(minHue) +
                        separateSymbol + String(maxHue) + endSymbol;
    }
    String getAllParameters() override {
        return Mode::getAllParameters() + char(ratio) + char(descendingStep) + char(minSaturation) + char(maxSaturation)
        + char(minBrightness) + char(maxBrightness) + char(isRainbowGoing) + char(baseStep) + char(minHue) + char(maxHue);
    };
    void calculate(CRGB *leds, const long *ledCount) override {
        long free = 0;
        for (long i = 0; i < *ledCount; i++) {
            leds[i] -= CHSV(0, 0, descendingStep);
            if (rgb2hsv_approximate(leds[i]).raw[2] == 0)
                free++;
        }
        while (free > *ledCount - *ledCount / ratio) {
            long i = random(*ledCount);
            if (rgb2hsv_approximate(leds[i]).raw[2] == 0) {
                leds[i] = CHSV(random(minHue, maxHue) + base * isRainbowGoing,
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
    byte minHue = 0;
    byte maxHue = 255;
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
    void setParam(byte numberParameter, byte parameters) override {
        Mode::setParam(numberParameter, parameters);
        switch (numberParameter) {
            case 3:
                gridWidth = parameters;
                break;
            case 4:
                isCircle = parameters;
                break;
            case 5:
                yScale = parameters; // тысячные
            case 6:
                hueScale = parameters;
                break;
            case 7:
                hueOffset = parameters;
                break;
            case 8:
                saturation = parameters;
                break;
            case 9:
                isRunningLight = parameters;
                break;
            case 10:
                gap = parameters;
                break;
            case 11:
                tailSize = parameters;
                break;
            case 12:
                direction = parameters;
                break;
        }
    }
    String getAllParameters(char separateSymbol, char endSymbol) override {
        return Mode::getAllParameters(separateSymbol, endSymbol) + String(gridWidth) + separateSymbol +
            String(int(isCircle)) + separateSymbol + String(yScale) + separateSymbol + String(hueScale) +
                separateSymbol + String(hueOffset) + separateSymbol + String(saturation) + separateSymbol +
                    String(int(isRunningLight)) + separateSymbol + String(gap) + separateSymbol + String(tailSize) +
                        separateSymbol + String(int(direction)) + endSymbol;
    }
    String getAllParameters() override {
        return Mode::getAllParameters() + char(gridWidth) + char(isCircle) + char(yScale) + char(hueScale) +
            char(hueOffset) + char(saturation) + char(isRunningLight) + char(gap) + char(tailSize) + char(direction);
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
    IridescentLights(byte delayTime, const long *ledsCount, CRGB *leds) {
        this->delayTime = delayTime;
        step = random(4294967295);
        for (long i = 0; i < *ledsCount; i++) {
            leds[i] = CHSV(random(256), 255 - saturation, 255);
        }
        target = randomIntSeeded(0, maxCount, step);
    }
    void setParam(byte numberParameter, byte parameters) override {
        Mode::setParam(numberParameter, parameters);
        switch (numberParameter) {
            case 3:
                saturation = parameters;
                break;
            case 4:
                minCount = parameters;
                break;
            case 5:
                maxCount = parameters;
                break;
        }
    }
    String getAllParameters(char separateSymbol, char endSymbol) override {
        return Mode::getAllParameters(separateSymbol, endSymbol) + String(saturation) +
            separateSymbol + String(maxCount) + separateSymbol + String(minCount) + endSymbol;
    }
    String getAllParameters() override {
        return Mode::getAllParameters() + char(saturation) + char(maxCount) + char(minCount);
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
    byte saturation = 0;  //todo ОНО НЕ РАБОТАЕТ!!!!
    byte maxCount = 255;
    byte minCount = 1;
};

#define version "0.0"

const long NUM_LEDS = 61;
#define LED_PIN 12
#define FREE_PIN 34
#define SD_PIN 5

TaskHandle_t showTask;
TaskHandle_t handlerTask;

CRGB leds[NUM_LEDS];
Mode *mode = nullptr;
byte currMode = 2;
uint32_t modeTimer = 0;
String receive;
bool isConnected = false;

bool isActive = true;

void setNewMode(byte modeNumber) {
    delete mode;
    switch (modeNumber) {
        case 0:
            mode = new SingleColor(20, 80);
        break;
        case 1:
            mode = new Party(25);
        break;
        case 2:
            mode = new PerlinShow(20, 80);
        break;
        default:
            mode = new IridescentLights(20, &NUM_LEDS, leds);
        break;
    }
    File modeSettings = SD.open("/mode_settings/mode_" + String(modeNumber) + "/current_profile.txt", FILE_READ);
    String currProfile = modeSettings.readString();
    modeSettings.close();
    modeSettings = SD.open("/mode_settings/mode_" + String(modeNumber) + "/profile_" + currProfile + ".txt", FILE_READ);
    long buf = long(modeSettings.read());
    long i = 1;
    while (buf != -1) {
        mode->setParam(i, buf);
        i++;
        buf = long(modeSettings.read());
    }
    modeSettings.close();
}
String convertRawToSimple(String raw, char separateSymbol, char endSymbol) {
    String result = String(byte(raw[0]));
    for (int i = 1; i < raw.length(); i++) {
        result += separateSymbol + String(byte(raw[i]));
    }
    return result + endSymbol;
}
void serialHandler(String receive) {
    char receivedChar[receive.length() - 1];
    receive.toCharArray(receivedChar, receive.length() - 1);
    if (receivedChar[0] == 'b') {
        isActive = !isActive;
        Serial.println("Pause!");
    }
    else if (receivedChar[0] == 'g') {
        Serial.println("Parameters!");
        Serial.println(convertRawToSimple(mode->getAllParameters(), ';', '.'));
        if (NuSerial.isConnected()) {
            NuSerial.println("Parameters!");
            NuSerial.println(mode->getAllParameters());
            NuSerial.println(convertRawToSimple(mode->getAllParameters(), ';', '.'));
        }
    }
    else if (receivedChar[0] == 'm') {
        setNewMode(byte(receive[1]) - '0');
        currMode = byte(receive[1]) - '0';
        File setMode = SD.open("/current_mode.txt", FILE_WRITE);
        setMode.print(char(currMode));
        setMode.close();
    }
    else if (receivedChar[0] == 's') {
        mode->setParam(byte(receivedChar[1]) - 'a' + 1, String(receivedChar + 2).toInt());
        File setSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_READ);
        String currProfile = setSettings.readString();
        setSettings.close();
        setSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/profile_"
            + String(currProfile) + ".txt", FILE_WRITE);
        setSettings.print(mode->getAllParameters());
        setSettings.close();
    }
    else if (receivedChar[0] == 'p') {
        File profileSettings;
        if (receivedChar[1] == 's') {
            if (SD.exists("/mode_settings/mode_" + String(currMode) + "/profile_" + String(receivedChar + 2) + ".txt")) {
                profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_WRITE);
                profileSettings.print(receivedChar + 2);
                profileSettings.close();
                setNewMode(currMode);
                Serial.println("Profile set to \"" + String(receivedChar + 2) + "\"!");
            }
            else {
                Serial.println("Profile \"" + String(receivedChar + 2) + "\" does not exist!");
            }
        }
        else if (receivedChar[1] == 'n') {
            profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/profile_" +
                String(receivedChar + 2) + ".txt", FILE_WRITE);
            profileSettings.print(mode->getAllParameters());
            profileSettings.close();
            profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_WRITE);
            profileSettings.print(receivedChar + 2);
            profileSettings.close();
            setNewMode(currMode);
            Serial.println("Create new profile \"" + String(receivedChar + 2) + "\"!");
        }
        else if (receivedChar[1] == 'd') {
            if (String(receivedChar + 2).equals("default")) {
                Serial.println("You can't delete default profile!");
            }
            else {
                profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_READ);
                String buf = profileSettings.readString();
                if (buf.equals(String(receivedChar + 2))) {
                    profileSettings.close();
                    profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_WRITE);
                    profileSettings.print("default");
                    profileSettings.close();
                    setNewMode(currMode);
                }
                profileSettings.close();
                SD.remove("/mode_settings/mode_" + String(currMode) + "/profile_" +
                    String(receivedChar + 2) + ".txt");
                Serial.println("Profile \"" + String(receivedChar + 2) + "\" deleted!");
            }
        }
        else if (receivedChar[1] == 'g') {
            Serial.println("There all profiles!");
            if (NuSerial.isConnected())
                NuSerial.println("There all profiles!");
            profileSettings = SD.open("/mode_settings/mode_" + String(currMode));
            String ans;
            while (true) {
                File file = profileSettings.openNextFile();
                if (!file)
                    break;
                if (!String(file.name()).equals("current_profile.txt"))
                    ans += file.name() + String("\t");
            }
            Serial.println(ans);
            if (NuSerial.isConnected())
                NuSerial.println(ans);
            profileSettings.close();
        }
    }
    else if (receivedChar[0] == 'a') {
        Serial.println("Parameters was changed!");
        mode->setAllParam(String(receivedChar + 1), ';');
    }
}

void showTaskCode(void *pvParameeters) {
    pinMode(FREE_PIN, INPUT);
    pinMode(2, OUTPUT);
    randomSeed(analogRead(FREE_PIN));

    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

    File getMode = SD.open("/current_mode.txt", FILE_READ);
    currMode = byte(getMode.read());
    getMode.close();
    setNewMode(currMode);
    while (true) {
        if (isActive)
            mode->show(leds, &NUM_LEDS, &modeTimer);
        delay(1);
    }
}
void handlerTaskCode(void *pvParameters) {
    NimBLEDevice::init("test");
    NimBLEDevice::setMTU(517);
    NuSerial.setTimeout(10);
    NuSerial.start();

    while (true) {
        // работа с BLE
        String stat = "";
        if (NuSerial.available()) {
            stat = NuSerial.readStringUntil('\n');
            if (stat.length() > 0) {
                serialHandler(stat + "12");
                Serial.println(stat);
            }
        }
        digitalWrite(2, NuSerial.isConnected());
        if (NuSerial.isConnected() && !isConnected) {
            delay(1000); // todo узнать насколько оно надо
            NuSerial.println(version);
            while (true) {
                String data = NuSerial.readString();
                if (data.length() > 0) {
                    if (!data.toInt())
                        NuSerial.disconnect();
                    break;
                }
            }
        }
        isConnected = NuSerial.isConnected();
        // работа с Serial портом
        if (Serial.available()) {
            receive += Serial.readString();
            if (receive[receive.length() - 1] == '\n') {
                serialHandler(receive);
                Serial.print(receive);
                receive = "";
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);

    if (!SD.begin(SD_PIN)) {
        Serial.println("SD card not initialized!");
        while (true) {}
    }

    xTaskCreatePinnedToCore(showTaskCode, "showTask", 8192, NULL, 10, &showTask, 1);
    delay(500);
    xTaskCreatePinnedToCore(handlerTaskCode, "handlerTask", 8192, NULL, 0, &handlerTask, 0);
}

void loop() {}