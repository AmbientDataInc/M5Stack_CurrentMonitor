/*
 * PAC1710で10ミリ秒毎に1000回、10秒、電流値を測定し、SDカードに書く
 */
#include <M5Stack.h>
#include <Wire.h>
#include "PAC1710.hpp"
#include "menu.h"

void beep(int freq, int duration, uint8_t volume);

#define DEVID PAC1710::ADDR::OPEN // Resistor OPEN (N.C.) at ADDR_SEL pin
const int VSHUNT_mOHM = 10;

int sampling  = 10;  // サンプリング間隔（ミリ秒）
#define NSAMPLES 1000     // 10ms x 1000 = 10秒
float startthreshold = 20.0;  // 記録を開始する電流値（ミリA）

void datasend(int id,int reg,int *data,int datasize) {
    Wire.beginTransmission(id);
    Wire.write(reg);
    for(int i=0;i<datasize;i++) {
        Wire.write(data[i]);
    }
    Wire.endTransmission();
}

int dataread(int id,int reg,int *data,int datasize) {
    Wire.beginTransmission(id);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(id, datasize);
    int i=0;
    while((i<datasize) && Wire.available()) {
        data[i] = Wire.read();
        i++;
    }
    return Wire.endTransmission(true);
}

uint16_t getID() {
    int id[2] = {0};
    dataread(DEVID, PAC1710::REG::PID, id, 2);
    return (id[0] << 8) | id[1];
}

#define TIMER0 0
hw_timer_t * samplingTimer = NULL;

float ampbuf[NSAMPLES];
float voltbuf[NSAMPLES];

Menu menu;

volatile int t0flag;

void IRAM_ATTR onTimer0() {
    t0flag = 1;
}

#define X0 10
#define Y0 220

void drawData(float maxamp) {
    M5.Lcd.fillRect(0, 0, 320, 220, BLACK);
    for (int i = 0; i < 299; i++) {
        int y0 = map((int)ampbuf[i * 3], 0, (int)maxamp, Y0, 0);
        int y1 = map((int)ampbuf[(i + 1) * 3], 0, (int)maxamp, Y0, 0);
        M5.Lcd.drawLine(i + X0, y0, i + 1 + X0, y1, WHITE);
    }
    M5.Lcd.drawLine(X0, Y0, 310, Y0, WHITE);
    M5.Lcd.drawLine(X0, 0, X0, Y0, WHITE);
}

void setup() {
    M5.begin();
    pinMode(21, INPUT_PULLUP);  // SDAをプルアップする
    pinMode(22, INPUT_PULLUP);  // SCLをプルアップする
    Wire.begin();

    M5.Lcd.setTextSize(2);
    M5.Lcd.fillScreen(BLACK);

    if (getID() != 0x585D) {
        M5.Lcd.setCursor(20, 100);
        M5.Lcd.print("Can not find PAC1710");
    }
    int c1cnf[] = {0B00110000};  // Sample time: 20ms, Range: -10mV to 10mV
    datasend(DEVID, PAC1710::REG::C1_VSAMP_CFG, c1cnf, 1);

    menu.setMenu("start", "", "");
    M5.Lcd.setCursor(20, 100);
    M5.Lcd.print("Press A button");
    M5.Lcd.setCursor(40, 120);
    M5.Lcd.print("to start sampling");

    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed()) break;
    }

    M5.Lcd.fillScreen(BLACK);
    beep(2000, 100, 2);

    samplingTimer = timerBegin(TIMER0, 80, true);  // 1マイクロ秒のタイマーを初期設定する
    timerAttachInterrupt(samplingTimer, &onTimer0, true);  // 割り込み処理関数を設定する
    timerAlarmWrite(samplingTimer, sampling * 1000, true);  // samplingミリ秒のタイマー値を設定する

    timerAlarmEnable(samplingTimer);  // タイマーを起動する

    bool started = false;
    int indx = 0;
    float maxamp = 0;

    M5.Lcd.fillRect(50, 100, 200, 10, BLACK);
    while (true) {
        t0flag = 0;
        while (t0flag == 0) {  // タイマー割り込みを待つ
            delay(0);
        }

        int ch1Vsense[2] = {0};
        int ch1Vsource[2] = {0};

        dataread(DEVID, PAC1710::REG::C1_SVRES_H, ch1Vsense, 2); // CHANNEL 1 VSENSE RESULT REGISTER 
        dataread(DEVID, PAC1710::REG::C1_VVRES_H, ch1Vsource, 2); // CHANNEL 1 VSOURCE RESULT REGISTER 

        float amp = ( (int16_t(ch1Vsense[0] << 8 | (ch1Vsense[1])) >>4) * (1000.0 / 2047));
        float volt = (int16_t((ch1Vsource[0] << 3) | (ch1Vsource[1] >> 5) ) * 19.531); 

        if (!started) {
            // 電流値がしきい値（startthreshold）未満だったら、測定を始めない
            if (amp > -startthreshold && amp < startthreshold) {
                continue;
            }
            started = true;  // 電流値がしきい値を超えたら測定開始
        }
        ampbuf[indx] = amp;  // 電流値をメモリーに記録する
        voltbuf[indx] = volt;  // 電圧値をメモリーに記録する
        maxamp = max(amp, maxamp);
        M5.Lcd.setCursor(100, 100);
        M5.Lcd.print(indx * 100 / NSAMPLES); M5.Lcd.print(" %");
        if (++indx >= NSAMPLES) {  // データー数がサンプル数を超えたら、周期処理を終わる
            break;
        }
    }
    timerAlarmDisable(samplingTimer);  // タイマーを停止する

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(20, 100);
    beep(2000, 400, 2);

    char fname[20];
    sprintf(fname, "/curLog.csv");
    for (int i = 0; SD.exists(fname); i++) {
        sprintf(fname, "/curLog(%d).csv", i + 1);
    }
    File f = SD.open(fname, FILE_WRITE);
    if (f) {
        f.println("time, current(mA), volt(mV)");
        for (int i = 0; i < NSAMPLES; i++) {
            f.printf("%d, %.2f, %.2f\r\n", sampling * i, ampbuf[i], voltbuf[i]);
        }
        f.close();
        M5.Lcd.print("Data written to");
        M5.Lcd.setCursor(40, 120);
        M5.Lcd.print(fname);
    } else {
        M5.Lcd.printf("open error %s", fname);
    }

    menu.setMenu("view", "", "");
    M5.Lcd.setCursor(40, 160);
    M5.Lcd.print("Press A button");
    M5.Lcd.setCursor(40, 180);
    M5.Lcd.print("to view data");

    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed()) {
            drawData(maxamp);
        }
    }
}

void loop() {
}
