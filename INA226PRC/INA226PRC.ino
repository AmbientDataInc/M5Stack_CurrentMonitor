/*
 * INA226PRCで4ミリ秒毎に3000回、12秒、電流値を測定し、SDカードに書く
 */
#include <M5Stack.h>
#include <Wire.h>
#include "INA226PRC.h"
#include "menu.h"

INA226PRC ina226prc;

void beep(int freq, int duration, uint8_t volume);

#define TIMER0 0
hw_timer_t * samplingTimer = NULL;

#define NSAMPLES 3000     // 4ms x 3000 = 12秒

short ampbuf[NSAMPLES];
short voltbuf[NSAMPLES];

int sampling  = 4;  // サンプリング間隔（ミリ秒）
int startthreshold = 3;  // 記録を開始する電流値（ミリA）

Menu menu;

volatile int t0flag;

void IRAM_ATTR onTimer0() {
    t0flag = 1;
}

int selectitem(int *candi, int items, int val, char *tail) {
    int focused;

    for (int i = 0; i < items; i++) {
        if (candi[i] == val) {
            focused = i;
        }
    }
    M5.Lcd.fillScreen(BLACK);
    menu.setMenu("up", "OK", "down");
    bool first = true;
    while (true) {
        bool modified = false;
        M5.update();
        if (M5.BtnA.wasPressed()) {
            focused--;
            modified = true;
        }
        if (M5.BtnC.wasPressed()) {
            focused++;
            modified = true;
        }
        if (M5.BtnB.wasPressed()) {
            M5.Lcd.fillScreen(BLACK);
            return candi[focused];
        }
        if (first || modified) {
            first = false;
            beep(1000, 100, 2);
            for (int i = 0; i < items; i++) {
                M5.Lcd.setCursor(100, 40 + i * 20);
                int16_t textcolor = ((focused % items) == i) ? BLACK : WHITE;
                int16_t backcolor = ((focused % items) == i) ? WHITE : BLACK;
                M5.Lcd.setTextColor(textcolor, backcolor);
                M5.Lcd.printf(" %d ", candi[i]);
                M5.Lcd.setTextColor(WHITE, BLACK);
                M5.Lcd.print(tail);
            }
        }
    }
}

void config() {
    int focused = 2;
    const int nItems = 3;
    int thresholds[] = {2, 3, 5, 10, 20};
    int samplings[] = {2, 4, 10, 20, 50};
    bool first = true;
    while (true) {
        bool modified = false;
        M5.update();
        if (M5.BtnA.wasPressed()) {
            focused--;  // upボタン
            modified = true;
        }
        if (M5.BtnC.wasPressed()) {
            focused++;  // downボタン
            modified = true;
        }
        if (M5.BtnB.wasPressed()) {  // changeボタン
            modified = true;
            switch (focused % nItems) {
            case 0:
                startthreshold = selectitem(thresholds, sizeof(thresholds) / sizeof(int), startthreshold, "mA");
                break;
            case 1:
                sampling = selectitem(samplings, sizeof(samplings) / sizeof(int), sampling, "ms");
                break;
            case 2:
            default:
                return;
            }
        }
        if (first || modified) {  // loop中で文字を書くとスピーカーからノイズが出るようだ
            first = false;
            beep(1000, 100, 2);
            menu.setMenu("up", "GO", "down");
            M5.Lcd.setCursor(20, 40);
            M5.Lcd.print("Start threshold: ");
            if ((focused % nItems) == 0) M5.Lcd.setTextColor(BLACK, WHITE);
            M5.Lcd.printf(" %d ", startthreshold);
            if ((focused % nItems) == 0) M5.Lcd.setTextColor(WHITE, BLACK);
            M5.Lcd.print("mA");
        
            M5.Lcd.setCursor(20, 100);
            M5.Lcd.print("Sampling period: ");
            if ((focused % nItems) == 1) M5.Lcd.setTextColor(BLACK, WHITE);
            M5.Lcd.printf(" %d ", sampling);
            if ((focused % nItems) == 1) M5.Lcd.setTextColor(WHITE, BLACK);
            M5.Lcd.print("ms");
    
            M5.Lcd.setCursor(20, 160);
            if ((focused % nItems) == 2) M5.Lcd.setTextColor(BLACK, WHITE);
            M5.Lcd.print(" DONE ");
            if ((focused % nItems) == 2) M5.Lcd.setTextColor(WHITE, BLACK);
        }
    }
}

#define X0 10
#define Y0 220

void drawData(short maxamp) {
    M5.Lcd.fillRect(0, 0, 320, 220, BLACK);
    maxamp = ((maxamp / 100) + 1) * 100;
    for (int i = 0; i < 299; i++) {
        int y0 = map(ampbuf[i * 10], 0, maxamp, Y0, 0);
        int y1 = map(ampbuf[(i + 1) * 10], 0, maxamp, Y0, 0);
        M5.Lcd.drawLine(i + X0, y0, i + 1 + X0, y1, WHITE);
        Serial.printf("%d, %d,  %d, %d\r\n", ampbuf[i * 10], ampbuf[(i + 1) * 10], y0, y1);
    }
    M5.Lcd.drawLine(X0, Y0, 310, Y0, WHITE);
    M5.Lcd.drawLine(X0, 0, X0, Y0, WHITE);
}

void setup() {
    M5.begin();
    Wire.begin();
    ina226prc.begin();

    Serial.print("Manufacture ID: ");
    Serial.println(ina226prc.readId(), HEX);

    M5.Lcd.setTextSize(2);
    config();

    M5.Lcd.fillScreen(BLACK);
    beep(1000, 100, 2);
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
    short maxamp = 0;

    M5.Lcd.fillRect(50, 100, 200, 10, BLACK);
    while (true) {
        t0flag = 0;
        while (t0flag == 0) {  // タイマー割り込みを待つ
            delay(0);
        }
        short amp = ina226prc.readCurrentReg();
        short volt = ina226prc.readVoltageReg();

        if (!started) {
            // 電流値がしきい値（startthreshold）未満だったら、測定を始めない
            if (amp * 0.1 > -(float)startthreshold && amp * 0.1 < (float)startthreshold) {
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
            f.printf("%d, %.2f, %.2f\r\n", sampling * i, ampbuf[i] * 0.1, voltbuf[i] * 1.25);
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

void loop()
{
}
