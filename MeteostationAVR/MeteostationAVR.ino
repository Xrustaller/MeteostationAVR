// ------------------------- НАСТРОЙКИ --------------------
#define RESET_CLOCK 0       // сброс часов на время загрузки прошивки (для модуля с несъёмной батарейкой). Не забудь поставить 0 и прошить ещё раз!
#define SENS_TIME 10000     // время обновления показаний сенсоров на экране, миллисекунд
#define LED_MODE 0          // тип RGB светодиода: 0 - главный катод, 1 - главный анод

// управление яркостью
#define BRIGHT_CONTROL 0      // 0/1 - запретить/разрешить управление яркостью (при отключении яркость всегда будет макс.)
#define BRIGHT_THRESHOLD 150  // величина сигнала, ниже которой яркость переключится на минимум (0-1023)
#define LED_BRIGHT_MAX 255    // макс яркость светодиода СО2 (0 - 255)
#define LED_BRIGHT_MIN 10     // мин яркость светодиода СО2 (0 - 255)
#define LCD_BRIGHT_MAX 255    // макс яркость подсветки дисплея (0 - 255)
#define LCD_BRIGHT_MIN 10     // мин яркость подсветки дисплея (0 - 255)

#define BLUE_YELLOW 1       // жёлтый цвет вместо синего (1 да, 0 нет) но из за особенностей подключения жёлтый не такой яркий
#define DISP_MODE 1         // в правом верхнем углу отображать: 0 - год, 1 - день недели, 2 - секунды
#define WEEK_LANG 1         // язык дня недели: 0 - английский, 1 - русский (транслит)
#define DEBUG 0             // вывод на дисплей лог инициализации датчиков при запуске. Для дисплея 1602 не работает! Но дублируется через порт!
#define DISPLAY_TYPE 1      // тип дисплея: 1 - 2004 (большой), 0 - 1602 (маленький)
#define DISPLAY_ADDR 0x27   // адрес платы дисплея: 0x27 или 0x3f. Если дисплей не работает - смени адрес! На самом дисплее адрес не указан

#define BM_TYPE 1           // 0 - BME280, 1 - BMP280

// адрес BME280 жёстко задан в файле библиотеки Adafruit_BME280.h
// стоковый адрес был 0x77, у китайского модуля адрес 0x76.
// Так что если юзаете НЕ библиотеку из архива - не забудьте поменять

// если дисплей не заводится - поменяйте адрес (строка 54)

#define BUS_ID 4
#define PIN_REDE A2

// пины
#define PIN_BACK_LIGHT 10
#define PIN_PHOTO A1

#define PIN_MHZ_RX 5
#define PIN_MHZ_TX 6

#define PIN_LED_COM 4
#define PIN_LED_R 9
#define PIN_LED_G 7
#define PIN_LED_B 8

#define PIN_HOUR A0
#define PIN_MIN A2
#define PIN_PLUS A3
#define PIN_MINUS A4

// библиотеки
#include <Arduino.h>
#include <Wire.h>
#include <ModbusRtu.h>

Modbus bus(BUS_ID, 0, PIN_REDE);
int8_t state = 0;
uint16_t temp[3] = { 0, 0, 0 };

#include <LiquidCrystal_I2C.h>

#if (DISPLAY_TYPE == 1)
LiquidCrystal_I2C lcd(DISPLAY_ADDR, 20, 4);
#else
LiquidCrystal_I2C lcd(DISPLAY_ADDR, 16, 2);
#endif

#include "RTClib.h"
RTC_DS3231 rtc;
DateTime now;

#define SEALEVELPRESSURE_HPA (1013.25)

#if (BM_TYPE == 0)
#include <Adafruit_BME280.h>
Adafruit_BME280 bme;
byte dispHum;
#else
#include <Adafruit_BMP280.h>
Adafruit_BMP280 bme;
#endif

#include <MHZ19_uart.h>
MHZ19_uart mhz19;

#include <GyverTimer.h>
GTimer_ms sensorsTimer(SENS_TIME);
GTimer_ms drawSensorsTimer(SENS_TIME);
GTimer_ms clockTimer(500);
GTimer_ms hourPlotTimer((long)4 * 60 * 1000);         // 4 минуты
GTimer_ms dayPlotTimer((long)1.6 * 60 * 60 * 1000);   // 1.6 часа
GTimer_ms plotTimer(240000);
GTimer_ms predictTimer((long)10 * 60 * 1000);         // 10 минут
GTimer_ms brightTimer(2000);

int8_t hrs, mins, secs;

// переменные для вывода
float dispTemp;
int dispPres;
int dispCO2;

// цифры
uint8_t LT[8] =     { 0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111 };
uint8_t RT[8] =     { 0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111 };
uint8_t LL[8] =     { 0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111 };
uint8_t LR[8] =     { 0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111 };
uint8_t UMB[8] =    { 0b11111,  0b11111,  0b11111,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111 };
uint8_t UB[8] =     { 0b11111,  0b11111,  0b11111,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000 };
uint8_t LMB[8] =    { 0b11111,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111 };
uint8_t LB[8] =     { 0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111 };

#if (WEEK_LANG == 0)
static const char* dayNames[] = {
  "Sund",
  "Mond",
  "Tues",
  "Wedn",
  "Thur",
  "Frid",
  "Satu",
};
#else
static const char* dayNames[] = {
  "BOCK",
  "POND",
  "BTOP",
  "CPED",
  "4ETB",
  "5YAT",
  "CYBB",
};
#endif

#if (LED_MODE == 0)
byte LED_ON = (LED_BRIGHT_MAX);
byte LED_OFF = (LED_BRIGHT_MIN);
#else
byte LED_ON = (255 - LED_BRIGHT_MAX);
byte LED_OFF = (255 - LED_BRIGHT_MIN);
#endif

void setLED(byte color) {
    // сначала всё выключаем
    if (!LED_MODE) {
        analogWrite(PIN_LED_R, 0);
        analogWrite(PIN_LED_G, 0);
        analogWrite(PIN_LED_B, 0);
    }
    else {
        analogWrite(PIN_LED_R, 255);
        analogWrite(PIN_LED_G, 255);
        analogWrite(PIN_LED_B, 255);
    }
    switch (color) {    // 0 выкл, 1 красный, 2 зелёный, 3 синий (или жёлтый)
    case 0:
        break;
    case 1: analogWrite(PIN_LED_R, LED_ON);
        break;
    case 2: analogWrite(PIN_LED_G, LED_ON);
        break;
    case 3:
        if (!BLUE_YELLOW) analogWrite(PIN_LED_B, LED_ON);
        else {
            analogWrite(PIN_LED_R, LED_ON - 50);    // чутка уменьшаем красный
            analogWrite(PIN_LED_G, LED_ON);
        }
        break;
    }
}

void checkBrightness() {
    if (analogRead(PIN_PHOTO) < BRIGHT_THRESHOLD) {   // если темно
        analogWrite(PIN_BACK_LIGHT, LCD_BRIGHT_MIN);
#if (LED_MODE == 0)
        LED_ON = (LED_BRIGHT_MIN);
#else
        LED_ON = (255 - LED_BRIGHT_MIN);
#endif
    }
    else {                                      // если светло
        analogWrite(PIN_BACK_LIGHT, LCD_BRIGHT_MAX);
#if (LED_MODE == 0)
        LED_ON = (LED_BRIGHT_MAX);
#else
        LED_ON = (255 - LED_BRIGHT_MAX);
#endif
    }
    if (dispCO2 < 800) setLED(2);
    else if (dispCO2 < 1200) setLED(3);
    else if (dispCO2 >= 1200) setLED(1);
}

void readSensors() {
#if (BM_TYPE == 0)
    bme.takeForcedMeasurement();
    dispHum = bme.readHumidity();
    temp[2] = dispHum;
#endif
    dispTemp = bme.readTemperature();
    temp[0] = dispTemp;
    dispPres = (float)bme.readPressure() * 0.00750062;
    temp[1] = dispPres;

    dispCO2 = mhz19.getPPM();
    if (dispCO2 < 0) {
        temp[3] = 0;
    }
    else {
        temp[3] = dispCO2;
    }
    if (dispCO2 < 800) setLED(2);
    else if (dispCO2 < 1200) setLED(3);
    else if (dispCO2 >= 1200) setLED(1);
}

void drawSensors() {
#if (DISPLAY_TYPE == 1)
    // дисплей 2004
    lcd.setCursor(0, 2);
    lcd.print(String(dispTemp, 1));
    lcd.write(223);
    lcd.setCursor(6, 2); 
#if (BM_TYPE == 0)
    lcd.print(" " + String(dispHum) + "%  ");
#else
    lcd.setCursor(12, 2);
#endif
    lcd.print(String(dispCO2) + " ppm");
    if (dispCO2 < 1000) lcd.print(" ");

    lcd.setCursor(0, 3);
    lcd.print(String(dispPres) + " mm  rain ");
    lcd.print(F("       "));

#else
    // дисплей 1602
    lcd.setCursor(0, 0);
    lcd.print(String(dispTemp, 1));
    lcd.write(223);
    lcd.setCursor(6, 0);
#if (BM_TYPE == 0)
    lcd.print(String(dispHum) + "%  ");
#else
    lcd.setCursor(12, 0);
#endif
    lcd.print(String(dispCO2) + "ppm");
    if (dispCO2 < 1000) lcd.print(" ");

    lcd.setCursor(0, 1);
    lcd.print(String(dispPres) + " mm");
#endif
}

void drawDig(byte dig, byte x, byte y) {
    switch (dig) {
    case 0:
        lcd.setCursor(x, y); // set cursor to column 0, line 0 (first row)
        lcd.write(0);  // call each segment to create
        lcd.write(1);  // top half of the number
        lcd.write(2);
        lcd.setCursor(x, y + 1); // set cursor to colum 0, line 1 (second row)
        lcd.write(3);  // call each segment to create
        lcd.write(4);  // bottom half of the number
        lcd.write(5);
        break;
    case 1:
        lcd.setCursor(x + 1, y);
        lcd.write(1);
        lcd.write(2);
        lcd.setCursor(x + 2, y + 1);
        lcd.write(5);
        break;
    case 2:
        lcd.setCursor(x, y);
        lcd.write(6);
        lcd.write(6);
        lcd.write(2);
        lcd.setCursor(x, y + 1);
        lcd.write(3);
        lcd.write(7);
        lcd.write(7);
        break;
    case 3:
        lcd.setCursor(x, y);
        lcd.write(6);
        lcd.write(6);
        lcd.write(2);
        lcd.setCursor(x, y + 1);
        lcd.write(7);
        lcd.write(7);
        lcd.write(5);
        break;
    case 4:
        lcd.setCursor(x, y);
        lcd.write(3);
        lcd.write(4);
        lcd.write(2);
        lcd.setCursor(x + 2, y + 1);
        lcd.write(5);
        break;
    case 5:
        lcd.setCursor(x, y);
        lcd.write(0);
        lcd.write(6);
        lcd.write(6);
        lcd.setCursor(x, y + 1);
        lcd.write(7);
        lcd.write(7);
        lcd.write(5);
        break;
    case 6:
        lcd.setCursor(x, y);
        lcd.write(0);
        lcd.write(6);
        lcd.write(6);
        lcd.setCursor(x, y + 1);
        lcd.write(3);
        lcd.write(7);
        lcd.write(5);
        break;
    case 7:
        lcd.setCursor(x, y);
        lcd.write(1);
        lcd.write(1);
        lcd.write(2);
        lcd.setCursor(x + 1, y + 1);
        lcd.write(0);
        break;
    case 8:
        lcd.setCursor(x, y);
        lcd.write(0);
        lcd.write(6);
        lcd.write(2);
        lcd.setCursor(x, y + 1);
        lcd.write(3);
        lcd.write(7);
        lcd.write(5);
        break;
    case 9:
        lcd.setCursor(x, y);
        lcd.write(0);
        lcd.write(6);
        lcd.write(2);
        lcd.setCursor(x + 1, y + 1);
        lcd.write(4);
        lcd.write(5);
        break;
    case 10:
        lcd.setCursor(x, y);
        lcd.write(32);
        lcd.write(32);
        lcd.write(32);
        lcd.setCursor(x, y + 1);
        lcd.write(32);
        lcd.write(32);
        lcd.write(32);
        break;
    }
}

void drawdots(byte x, byte y, boolean state) {
    byte code;
    if (state) code = 165;
    else code = 32;
    lcd.setCursor(x, y);
    lcd.write(code);
    lcd.setCursor(x, y + 1);
    lcd.write(code);
}

void drawClock(byte hours, byte minutes, byte x, byte y, boolean dotState) {
    // чисти чисти!
    lcd.setCursor(x, y);
    lcd.print("               ");
    lcd.setCursor(x, y + 1);
    lcd.print("               ");

    //if (hours > 23 || minutes > 59) return;
    if (hours / 10 == 0) drawDig(10, x, y);
    else drawDig(hours / 10, x, y);
    drawDig(hours % 10, x + 4, y);
    // тут должны быть точки. Отдельной функцией
    drawDig(minutes / 10, x + 8, y);
    drawDig(minutes % 10, x + 12, y);
}

void drawData() {
    lcd.setCursor(15, 0);
    if (now.day() < 10) lcd.print(0);
    lcd.print(now.day());
    lcd.print(".");
    if (now.month() < 10) lcd.print(0);
    lcd.print(now.month());

    if (DISP_MODE == 0) {
        lcd.setCursor(16, 1);
        lcd.print(now.year());
    }
    else if (DISP_MODE == 1) {
        lcd.setCursor(16, 1);
        int dayofweek = now.dayOfTheWeek();
        lcd.print(dayNames[dayofweek]);
    }
}

boolean dotFlag;
void clockTick() {
    dotFlag = !dotFlag;
    if (dotFlag) {          // каждую секунду пересчёт времени
        secs++;
        if (secs > 59) {      // каждую минуту
            secs = 0;
            mins++;
            if (mins <= 59) drawClock(hrs, mins, 0, 0, 1);
        }
        if (mins > 59) {      // каждый час
            now = rtc.now();
            secs = now.second();
            mins = now.minute();
            hrs = now.hour();
            drawClock(hrs, mins, 0, 0, 1);
            if (hrs > 23) {
                hrs = 0;
            }
            if (DISPLAY_TYPE) drawData();
        }
        if (DISP_MODE == 2) {
            lcd.setCursor(16, 1);
            if (secs < 10) lcd.print(" ");
            lcd.print(secs);
        }
    }
    drawdots(7, 0, dotFlag);
    if (dispCO2 >= 1200) {
        if (dotFlag) setLED(1);
        else setLED(0);
    }
}

void loadClock() {
    lcd.createChar(0, LT);
    lcd.createChar(1, UB);
    lcd.createChar(2, RT);
    lcd.createChar(3, LL);
    lcd.createChar(4, LB);
    lcd.createChar(5, LR);
    lcd.createChar(6, UMB);
    lcd.createChar(7, LMB);
}

void setup() {
    bus.begin(19200);

    pinMode(PIN_BACK_LIGHT, OUTPUT);
    pinMode(PIN_LED_COM, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    setLED(0);

    digitalWrite(PIN_LED_COM, LED_MODE);
    analogWrite(PIN_BACK_LIGHT, LCD_BRIGHT_MAX);

    lcd.init();
    lcd.backlight();
    lcd.clear();

#if (DEBUG == 1 && DISPLAY_TYPE == 1)
    boolean status = true;

    setLED(1);

    lcd.setCursor(0, 0);
    lcd.print(F("MHZ-19... "));
    mhz19.begin(PIN_MHZ_RX, PIN_MHZ_TX);
    mhz19.setAutoCalibration(false);
    mhz19.getStatus();    // первый запрос, в любом случае возвращает -1
    delay(500);
    if (mhz19.getStatus() == 0) {
        lcd.print(F("OK"));
    }
    else {
        lcd.print(F("ERROR"));
        status = false;
    }

    setLED(2);
    lcd.setCursor(0, 1);
    lcd.print(F("RTC... "));
    delay(50);
    if (rtc.begin()) {
        lcd.print(F("OK"));
    }
    else {
        lcd.print(F("ERROR"));
        status = false;
    }

    setLED(3);
    lcd.setCursor(0, 2);
    lcd.print(F("BME280... "));
    delay(50);
#if (BM_TYPE == 0)
    if (bme.begin(0x77, &Wire)) {
        lcd.print(F("OK"));
    }
#else
    if (bme.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
        lcd.print(F("OK"));
    }
#endif
    else {
        lcd.print(F("ERROR"));
        status = false;
    }

    setLED(0);
    lcd.setCursor(0, 3);
    if (status) {
        lcd.print(F("All good"));
    }
    else {
        lcd.print(F("Check wires!"));
    }
    while (1) {
        lcd.setCursor(14, 1);
        lcd.print("P:    ");
        lcd.setCursor(16, 1);
        lcd.print(analogRead(PIN_PHOTO), 1);
        delay(300);
    }
#else
    mhz19.begin(PIN_MHZ_RX, PIN_MHZ_TX);
    mhz19.setAutoCalibration(false);
    rtc.begin();
#if (BM_TYPE == 0)
    bme.begin(0x77, &Wire);
#else
    bme.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
#endif

#endif

#if (BM_TYPE == 0)
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
        Adafruit_BME280::SAMPLING_X1, // temperature
        Adafruit_BME280::SAMPLING_X1, // pressure
        Adafruit_BME280::SAMPLING_X1, // humidity
        Adafruit_BME280::FILTER_OFF);
#else
    bme.setSampling(Adafruit_BMP280::MODE_FORCED,
        Adafruit_BMP280::SAMPLING_X1, // temperature
        Adafruit_BMP280::SAMPLING_X1, // pressure
        Adafruit_BMP280::FILTER_OFF);
#endif

    if (RESET_CLOCK || rtc.lostPower())
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    now = rtc.now();
    secs = now.second();
    mins = now.minute();
    hrs = now.hour();
#if (BM_TYPE == 0)
    bme.takeForcedMeasurement();
#endif
    uint32_t pressure = bme.readPressure();

    if (DISPLAY_TYPE == 1) {
        loadClock();
        drawClock(hrs, mins, 0, 0, 1);
        drawData();
    }
    readSensors();
    drawSensors();
}

void loop() {
    state = bus.poll(temp, 4);

    if (brightTimer.isReady()) checkBrightness(); // яркость
    if (sensorsTimer.isReady()) readSensors();    // читаем показания датчиков с периодом SENS_TIME

#if (DISPLAY_TYPE == 1)
    if (clockTimer.isReady()) clockTick();        // два раза в секунду пересчитываем время и мигаем точками
	// в режиме "главного экрана"
	if (drawSensorsTimer.isReady()) drawSensors();  // обновляем показания датчиков на дисплее с периодом SENS_TIME
 
#else
    if (drawSensorsTimer.isReady()) drawSensors();
#endif
}
