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
        if (millis() - *timer > float(delayTime) / speed * 100 && started) {
            // debugTimer = millis(); // логика таймера для замера времени выполнения
            *timer = millis();
            calculate(leds, ledCount);
            FastLED.setBrightness(brightness);
            FastLED.show();
            // Serial.println("time: " + String(millis() - debugTimer)); // вывод таймера для замера времени выполнения
        }
    }
    // запустить режим
    void start() {started = true;}
private:
    // unsigned long debugTimer = 0; // таймер для определения времени выполнения
    bool started = false;
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
        byte* run, uint32_t delayRunTime, byte* moveCounter, bool hasToRun = false) {
        if (hasToRun) {
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
    // временя ожидания между кадрами
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
            byte brightnessForLed = calculateRunLight(i, gap, tailSize, true, &run, runLightDelay, &moveCounter, !i);
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
            if (rgb2hsv_approximate(leds[i]).raw[2] == 0) free++;
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
                break;
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
                calculateRunLight(i, gap, tailSize, direction, &run, runLightDelay, &moveCounter, !i) :
                calculateRunLight(*ledCount - i - 1, gap, tailSize, direction, &run, runLightDelay, &moveCounter));
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

class AlarmManager {
public:
    AlarmManager(uint8_t timeZone) {
        timeNow = new Time(123);
        this->timeZone = timeZone;
        findClosest();
    }
    void findClosest() {
        timeNow->updateTime(timeZone);
        File file;
        File alarms = SD.open("/time/alarms", FILE_READ);
        alarm = Alarm();
        long minTime = -1;
        while (true) {
            Alarm buf;
            file = alarms.openNextFile();
            if (!file) {
                file.close();
                break;
            }
            buf = Alarm(file.readString());
            file.close();
            long bufMinTime = buf.getRawTimeDif(timeNow->getRawTime());
            if ((minTime == -1 || bufMinTime < minTime) && bufMinTime != -1 && buf.isOn) {
                alarm = buf;
                minTime = bufMinTime;
            }
        }
        alarms.close();
        delete mode;
        if (!alarm.wasInit) {
            mode = new Mode();
            return;
        }
        switch (alarm.mode) {
            case 0:
                mode = new SingleColor(20, 80);
                break;
            case 1:
                mode = new Party(25);
                break;
            default:
                mode = new PerlinShow(20, 80);
                break;
            // case 3:
            //     mode = new IridescentLights(20, &NUM_LEDS, leds);
        }
        mode->start();
        file = SD.open("/mode_settings/mode_" + String(alarm.mode) + "/profile_" + alarm.profile + ".txt", FILE_READ);
        long buf = long(file.read());
        byte i = 1;
        while (buf != -1) {
            mode->setParam(i, buf);
            ++i;
            buf = long(file.read());
        }
        file.close();
    }
    void update(CRGB *leds, const long *ledCount, uint32_t *timer) {
        mode->setParam(1, byte(_min(long(float(millis() - startAlarm) / 60000 * 255 / alarm.razingTime), 255)));
        mode->show(leds, ledCount, timer);
    }
    void checkAlarm() {
        if (timeNow != nullptr && WiFi.status() == WL_CONNECTED) {
            timeNow->updateTime(timeZone);
            if (alarm.wasInit) {
                if (alarm.getRawTimeDif(timeNow->getRawTime()) == 0 && !isActive) {
                    startAlarm = millis();
                    isActive = true;
                    if (NuSerial.isConnected()) NuSerial.println("alarm_activated");
                }
            }
        }
    }
    bool checkIsActive() const {return isActive;}
    void offAlarm() {
        isActive = false;
        findClosest();
    }
    void setTimeZone(int8_t timeZone){this->timeZone = timeZone;}
private:
    class Time {
    public:
        explicit Time (byte serv) {
            udp.begin(serv);
        }
        void updateTime(int8_t timeZone) {
            byte packet[48];
            packet[0] = 0b11100011;
            udp.beginPacket("pool.ntp.org", 123);
            udp.write(packet, sizeof(packet));
            udp.endPacket();

            delay(100);
            if (udp.parsePacket() < 48) return;

            udp.read(packet, 48);
            unsigned long epoch = word(packet[40], packet[41]) << 16 | word(packet[42], packet[43]);
            epoch = epoch - 2208988800UL + timeZone * 3600;

            day = (epoch / 86400 + 4) % 7;
            hour = (epoch % 86400) / 3600;
            minute = (epoch % 3600) / 60;
        }
        long getRawTime() const {return getDay() * 1440 + getHour() * 60 + getMinute();}
        byte getDay() const {return day;}
        byte getHour() const {return hour;}
        byte getMinute() const {return minute;}
    private:
        WiFiUDP udp;
        byte day = 0;
        byte hour = 0;
        byte minute = 0;
    };
    struct Alarm {
        Alarm (String data) {
            const char *bytes = data.c_str();
            isOn = bytes[0] / 128;
            for (byte i = 0; i < 7; i++) days[i] = bool(long(bytes[0] % long(pow(2, i + 1)) / pow(2, i)));
            minute = bytes[1];
            hour = bytes[2];
            razingTime = bytes[3];
            mode = bytes[4];
            profile = String(bytes + 5);
            wasInit = true;
        }
        Alarm(){}
        long getRawTimeDif(long rawTime) {
            long minTime = -1;
            for (byte i = 0; i < 7; i++) {
                long buf = (i * 1440 + hour * 60 + minute - rawTime + 10080) % 10080;
                if ((minTime == -1 || buf < minTime) && days[i]) minTime = buf;
            }
            return minTime;
        }
        String getSettings() {
            String ans;
            char buf = char(128 * isOn);
            for (byte i = 0; i < 7; i++) buf += days[i] * pow(2, i);
            ans += buf;
            ans += char(minute);
            ans += char(hour);
            ans += char(razingTime);
            ans += char(mode);
            return ans;
        }
        String getRawSettings() {
            String ans;
            ans += byte(isOn);
            for (byte i = 0; i < 7; i++) ans += days[i];
            ans += minute;
            ans += hour;
            ans += razingTime;
            ans += mode;
            return ans;
        }
        bool wasInit = false;
        bool isOn;
        byte hour;
        byte minute;
        byte razingTime;
        bool days[7];
        String profile;
        byte mode;
    };
    Alarm alarm;
    Time *timeNow = nullptr;
    int8_t timeZone = 3;
    bool isActive = false;
    Mode *mode = nullptr;
    uint32_t startAlarm = 0;
};

#define VERSION "1.0"
#define MODE_COUNT 4

// Константы (на люстре 82, на стенде 61)
const long NUM_LEDS = 61;
#define LED_PIN 12
#define FREE_PIN 34
#define SD_PIN 5

// Два ядра
TaskHandle_t showTask;
TaskHandle_t handlerTask;

// WiFi и NTP
String wifiSsid;
String wifiPassword;
AlarmManager *alarmManager = nullptr;
uint32_t alarmTimer = 0;

// Для режимов
CRGB leds[NUM_LEDS];
Mode *mode = nullptr;
byte currMode = 2;
uint32_t modeTimer = 0;

// Для приходящих данных
String receive;

// Для BLE todo убрать подсветку BLE
bool isConnected = false;

// Для выключения ленты
bool isActive = true;

void(* reset) () = nullptr;
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
    mode->start();
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
    if (receive[0] == 'b') {
        isActive = !isActive;
        FastLED.clear();
        FastLED.show();
        Serial.println("Pause!");
    }
    else if (receive[0] == 'n') {
        String name = receive.substring(1);
        Serial.println("Bluetooth name was changed to \"" + name + "\"!");
        File nameSetting = SD.open("/name.txt", FILE_WRITE);
        nameSetting.print(name);
        nameSetting.close();
    }
    else if (receive[0] == 'g') {
        Serial.println("Parameters!");
        Serial.println(convertRawToSimple(mode->getAllParameters(), ';', '.'));
        if (NuSerial.isConnected()) {
            NuSerial.println("Parameters!");
            NuSerial.println(mode->getAllParameters());
            NuSerial.println(convertRawToSimple(mode->getAllParameters(), ';', '.'));
        }
    }
    else if (receive[0] == 'm') {
        setNewMode(byte(receive[1]) - '0');
        currMode = byte(receive[1]) - '0';
        File setMode = SD.open("/current_mode.txt", FILE_WRITE);
        setMode.print(char(currMode));
        setMode.close();
    }
    else if (receive[0] == 's') {
        mode->setParam(byte(receive[1]) - 'a' + 1, String(receive.substring(2)).toInt());
        File setSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_READ);
        String currProfile = setSettings.readString();
        setSettings.close();
        setSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/profile_"
            + String(currProfile) + ".txt", FILE_WRITE);
        setSettings.print(mode->getAllParameters());
        setSettings.close();
    }
    else if (receive[0] == 'p') {
        File profileSettings;
        if (receive[1] == 's') {
            if (SD.exists("/mode_settings/mode_" + String(currMode) + "/profile_" + String(receive.substring(2)) + ".txt")) {
                profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_WRITE);
                profileSettings.print(receive.substring(2));
                profileSettings.close();
                setNewMode(currMode);
                Serial.println("Profile set to \"" + String(receive.substring(2)) + "\"!");
            }
            else {
                Serial.println("Profile \"" + String(receive.substring(2)) + "\" does not exist!");
            }
        }
        else if (receive[1] == 'n') {
            profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/profile_" +
                String(receive.substring(2)) + ".txt", FILE_WRITE);
            profileSettings.print(mode->getAllParameters());
            profileSettings.close();
            profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_WRITE);
            profileSettings.print(receive.substring(2));
            profileSettings.close();
            setNewMode(currMode);
            Serial.println("Create new profile \"" + String(receive.substring(2)) + "\"!");
        }
        else if (receive[1] == 'd') {
            if (String(receive.substring(2)).equals("default")) {
                Serial.println("You can't delete default profile!");
            }
            else {
                profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_READ);
                String buf = profileSettings.readString();
                if (buf.equals(String(receive.substring(2)))) {
                    profileSettings.close();
                    profileSettings = SD.open("/mode_settings/mode_" + String(currMode) + "/current_profile.txt", FILE_WRITE);
                    profileSettings.print("default");
                    profileSettings.close();
                    setNewMode(currMode);
                }
                profileSettings.close();
                SD.remove("/mode_settings/mode_" + String(currMode) + "/profile_" +
                    String(receive.substring(2)) + ".txt");
                Serial.println("Profile \"" + String(receive.substring(2)) + "\" deleted!");
            }
        }
        else if (receive[1] == 'g') {
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
    else if (receive[0] == 'a') {
        Serial.println("Parameters was changed!");
        mode->setAllParam(String(receive.substring(1)), ';');
    }
    else if (receive[0] == 'w') {
        if (receive[1] == 's') {
            wifiSsid = String(receive.substring(2));
            File ssid = SD.open("/wifi/ssid.txt", FILE_WRITE);
            ssid.print(wifiSsid);
            ssid.close();
            Serial.println("Wifi SSID was changed to \"" + wifiSsid + "\"!");
        }
        else if (receive[1] == 'p') {
            wifiPassword = String(receive.substring(2));
            File password = SD.open("/wifi/password.txt", FILE_WRITE);
            password.print(wifiPassword);
            password.close();
            Serial.println("Wifi password was changed to \"" + wifiPassword + "\"!");
        }
    }
    else if (receive[0] == 't') {
        if (receive[1] == 'z') {
            int8_t timeZone = String(receive.substring(2)).toInt();
            File zone = SD.open("/time/time_zone.txt", FILE_WRITE);
            zone.print(timeZone);
            zone.close();
            alarmManager->setTimeZone(timeZone);
            Serial.println("Time zone was changed to \"" + String(timeZone) + "\"!");
        }
        else if (receive[1] == 'o') {
            alarmManager->offAlarm();
            FastLED.clear();
            FastLED.show();
            Serial.println("Alarm was canceled!");
        }
        else if (receive[1] == 'n') {
            String name = receive.substring(2, receive.indexOf('\t'));
            File alarm = SD.open(String("/time/alarms/") + name + ".txt", FILE_WRITE);
            alarm.print(receive.substring(receive.indexOf('\t') + 1));
            alarm.close();
            alarmManager->findClosest();
            Serial.println("Added new alarm \"" + name + "\"!");
        }
        else if (receive[1] == 'd') {
            SD.remove("/time/alarms/" + String(receive.substring(2)) + ".txt");
            alarmManager->findClosest();
            Serial.println("Removed alarm \"" + String(receive.substring(2)) + "\"!");
        }
    }
    else if (receive[0] == 'r') {
        Serial.println("Rebooting now...");
        reset();
    }
}

void showTaskCode(void *pvParameters) {
    pinMode(FREE_PIN, INPUT);
    pinMode(2, OUTPUT);
    randomSeed(analogRead(FREE_PIN));

    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

    File getMode = SD.open("/current_mode.txt", FILE_READ);
    currMode = byte(getMode.read());
    getMode.close();
    setNewMode(currMode);
    while (true) {
        delay(1);
        if (alarmManager != nullptr)
            if (alarmManager->checkIsActive()) {
                alarmManager->update(leds, &NUM_LEDS, &modeTimer);
                continue;
            }
        if (isActive)
            mode->show(leds, &NUM_LEDS, &modeTimer);
    }
}
void handlerTaskCode(void *pvParameters) {
    File file = SD.open("/wifi/ssid.txt", FILE_READ);
    File bufFile;
    wifiSsid = file.readString();
    file.close();
    file = SD.open("/wifi/password.txt", FILE_READ);
    wifiPassword = file.readString();
    file.close();

    WiFi.begin(wifiSsid, wifiPassword);
    uint32_t timer = millis();
    while (WiFiClass::status() != WL_CONNECTED && millis() - timer < 5000) {}
    if (WiFiClass::status() != WL_CONNECTED) Serial.println("WiFi not connected!");

    file = SD.open("/time/time_zone.txt", FILE_READ);
    uint8_t timeZone = file.readString().toInt();
    file.close();

    file = SD.open("/name.txt", FILE_READ);
    NimBLEDevice::init(file.readString().c_str());
    file.close();
    NimBLEDevice::setMTU(517);
    NuSerial.setTimeout(10);
    NuSerial.start();

    alarmManager = new AlarmManager(timeZone);

    while (true) {
        // работа с BLE
        String stat = "";
        if (NuSerial.available()) {
            stat = NuSerial.readString();
            if (stat.length() > 0) {
                serialHandler(stat);
                Serial.println(stat);
                for (char c : stat) Serial.print(String() + byte(c) + " ");
                Serial.println();
            }
        }
        digitalWrite(2, NuSerial.isConnected());
        if (NuSerial.isConnected() && !isConnected) {
            delay(1500);
            NuSerial.println(VERSION);
            timer = millis();
            while (millis() - timer < 1500) {
                String data = NuSerial.readString();
                if (data.length() > 0) {
                    if (!data.toInt()) {
                        NuSerial.disconnect();
                        timer = millis() - 1500;
                    }
                    break;
                }
            }
            if (millis() - timer >= 1500) {
                NuSerial.disconnect();
                continue;
            }
            // отправка всех настроек на устройство
            NuSerial.println(String("isOn\t") + int(isActive));
            file = SD.open("/name.txt", FILE_READ);
            NuSerial.println(String("name\t") + file.readString());
            file.close();
            file = SD.open("/wifi/ssid.txt", FILE_READ);
            NuSerial.println(String("ssid\t") + file.readString());
            file.close();
            file = SD.open("/wifi/password.txt", FILE_READ);
            NuSerial.println(String("password\t") + file.readString());
            file.close();
            NuSerial.println(String("current_mode\t") + char(currMode));
            for (byte i = 0; i < MODE_COUNT; ++i) {
                NuSerial.println(String("mode\t") + char(i));
                file = SD.open("/mode_settings/mode_" + String(i), FILE_READ);
                while (true) {
                    bufFile = file.openNextFile();
                    if (!bufFile) break;
                    NuSerial.println(String(bufFile.name()) + "\t" + bufFile.readString());
                }
                bufFile.close();
                file.close();
            }

            file = SD.open("/time/time_zone.txt", FILE_READ);
            NuSerial.println(String("time_zone\t") + char(file.readString().toInt()));
            file.close();
            NuSerial.println("alarms_start");
            file = SD.open("/time/alarms", FILE_READ);
            while (true) {
                bufFile = file.openNextFile();
                if (!bufFile) break;
                NuSerial.println(String(bufFile.name()) + "\t" +  bufFile.readString());
            }
            file.close();
            bufFile.close();
            NuSerial.println("alarms_stop");
            if (alarmManager->checkIsActive())
                NuSerial.println("alarm_activated");
        }
        isConnected = NuSerial.isConnected();
        // работа с Serial портом
        if (Serial.available()) {
            String buf = Serial.readString();
            if (buf.endsWith("\n")) {
                serialHandler(receive);
                Serial.println(receive);
                receive = "";
            }
            else
                receive += buf;
        }
        // проверка времени и будильника
        if (millis() - alarmTimer > 5000) {
            alarmTimer = millis();
            alarmManager->checkAlarm();
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);
    digitalWrite(2, LOW);

    if (!SD.begin(SD_PIN)) {
        Serial.println("SD card not initialized!");
        while (true) {}
    }

    xTaskCreatePinnedToCore(showTaskCode, "showTask", 8192, NULL, 10, &showTask, 1);
    delay(500);
    xTaskCreatePinnedToCore(handlerTaskCode, "handlerTask", 8192, NULL, 0, &handlerTask, 0);
}

void loop() {}