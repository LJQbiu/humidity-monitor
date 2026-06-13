#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ========== OLED 配置 ==========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========== WiFi 配置 ==========
const char* ssid = "LJQ";
const char* password = "12345678";
const char* serverUrl = "http://101.37.19.59:5000/api/data";

// ========== 传感器 ==========
int adc_raw = 0;
float voltage = 0, moisture = 0;
float dry_v = 2.5, wet_v = 1.0;

// ========== 上传状态 ==========
bool uploadOk = false;
bool hasUpload = false;

// ========== 拆分循环时间管理 ==========
unsigned long lastDisplayTime = 0;
unsigned long lastSensorTime = 0;
unsigned long lastUploadTime = 0;
#define DISPLAY_INTERVAL 250   // 250ms刷新(4fps)
#define SENSOR_INTERVAL  5000  // 5秒读传感器
#define UPLOAD_INTERVAL  5000  // 5秒上传

// ========== 动画系统(丰富!) ==========
int wingPhase = 0;          // 翅膀0-7(8帧更平滑)
unsigned long wingTimer = 0;
#define WING_SPEED 60       // 60ms一帧

int breathPhase = 0;        // 呼吸0-3
unsigned long breathTimer = 0;
#define BREATH_SPEED 800    // 800ms一帧

bool blinkState = false;
bool bigBlink = false;
unsigned long blinkStart = 0;
unsigned long nextBlinkTime = 3000;

int headTilt = 0;           // 头倾斜 -1/0/1
unsigned long headTiltTimer = 0;
#define HEAD_TILT_SPEED 1500

int armAction = 0;          // 手臂动作0-6
unsigned long armTimer = 0;
#define ARM_CHANGE_SPEED 2000

int footPhase = 0;          // 脚0-3
unsigned long footTimer = 0;
#define FOOT_SPEED 400

// ========== 5级心情! ==========
enum FairyMood { MOOD_JOY, MOOD_HAPPY, MOOD_OK, MOOD_WORRY, MOOD_SAD };

FairyMood getMood() {
  if (moisture >= 80) return MOOD_JOY;
  if (moisture >= 60) return MOOD_HAPPY;
  if (moisture >= 40) return MOOD_OK;
  if (moisture >= 20) return MOOD_WORRY;
  return MOOD_SAD;
}

// ========== 花仙子锚点 ==========
#define FCX 84
#define HEAD_Y 10
#define HEAD_R 8

void drawCuteFairy(FairyMood mood, bool blink, bool bigB, int wingP, int breathP, int tilt, int armAct, int footP) {
  // 翅膀偏移(8帧平滑)
  int wingOff = 0;
  if (wingP <= 3) wingOff = -3 + wingP;
  else wingOff = 1 + (wingP - 4);

  // 呼吸偏移
  int bodyOff = 0;
  if (breathP == 1) bodyOff = 1;
  else if (breathP == 3) bodyOff = -1;

  int headOffX = tilt;
  int shoulderY = HEAD_Y + HEAD_R + 2 + bodyOff;
  int neckY = HEAD_Y + HEAD_R;
  int skirtTop = shoulderY + 10;
  int hipY = skirtTop + 6;
  int footY = 52;

  // ===== 1. 翅膀 =====
  if (mood == MOOD_SAD) {
    display.fillTriangle(FCX-2, shoulderY, FCX-10, shoulderY+6, FCX-8, shoulderY+wingOff+10, SSD1306_WHITE);
    display.fillTriangle(FCX+2, shoulderY, FCX+10, shoulderY+6, FCX+8, shoulderY+wingOff+10, SSD1306_WHITE);
  } else if (mood == MOOD_WORRY) {
    display.fillTriangle(FCX-2, shoulderY, FCX-14, shoulderY+wingOff-2, FCX-8, shoulderY+wingOff+6, SSD1306_WHITE);
    display.fillTriangle(FCX+2, shoulderY, FCX+14, shoulderY+wingOff-2, FCX+8, shoulderY+wingOff+6, SSD1306_WHITE);
  } else if (mood == MOOD_JOY) {
    display.fillTriangle(FCX-2, shoulderY-3, FCX-28, shoulderY+wingOff-8, FCX-16, shoulderY+wingOff, SSD1306_WHITE);
    display.fillTriangle(FCX-2, shoulderY, FCX-24, shoulderY+wingOff+4, FCX-14, shoulderY+wingOff+10, SSD1306_WHITE);
    display.fillTriangle(FCX+2, shoulderY-3, FCX+28, shoulderY+wingOff-8, FCX+16, shoulderY+wingOff, SSD1306_WHITE);
    display.fillTriangle(FCX+2, shoulderY, FCX+24, shoulderY+wingOff+4, FCX+14, shoulderY+wingOff+10, SSD1306_WHITE);
    display.drawLine(FCX-16, shoulderY+wingOff, FCX-22, shoulderY+wingOff-4, SSD1306_BLACK);
    display.drawLine(FCX+16, shoulderY+wingOff, FCX+22, shoulderY+wingOff-4, SSD1306_BLACK);
    display.fillCircle(FCX-26, shoulderY+wingOff-6, 1, SSD1306_WHITE);
    display.fillCircle(FCX+26, shoulderY+wingOff-6, 1, SSD1306_WHITE);
  } else if (mood == MOOD_HAPPY) {
    display.fillTriangle(FCX-2, shoulderY-2, FCX-24, shoulderY+wingOff-6, FCX-14, shoulderY+wingOff+2, SSD1306_WHITE);
    display.fillTriangle(FCX-2, shoulderY+2, FCX-20, shoulderY+wingOff+6, FCX-12, shoulderY+wingOff+12, SSD1306_WHITE);
    display.fillTriangle(FCX+2, shoulderY-2, FCX+24, shoulderY+wingOff-6, FCX+14, shoulderY+wingOff+2, SSD1306_WHITE);
    display.fillTriangle(FCX+2, shoulderY+2, FCX+20, shoulderY+wingOff+6, FCX+12, shoulderY+wingOff+12, SSD1306_WHITE);
    display.drawLine(FCX-14, shoulderY+wingOff+2, FCX-18, shoulderY+wingOff-2, SSD1306_BLACK);
    display.drawLine(FCX+14, shoulderY+wingOff+2, FCX+18, shoulderY+wingOff-2, SSD1306_BLACK);
  } else {
    display.fillTriangle(FCX-2, shoulderY, FCX-16, shoulderY+wingOff-2, FCX-10, shoulderY+wingOff+8, SSD1306_WHITE);
    display.fillTriangle(FCX+2, shoulderY, FCX+16, shoulderY+wingOff-2, FCX+10, shoulderY+wingOff+8, SSD1306_WHITE);
  }

  // ===== 2. 花瓣裙 =====
  if (mood == MOOD_JOY) {
    display.fillTriangle(FCX, skirtTop-2, FCX-12, skirtTop+8, FCX+12, skirtTop+8, SSD1306_WHITE);
    display.fillCircle(FCX-6, skirtTop+7, 2, SSD1306_WHITE);
    display.fillCircle(FCX+6, skirtTop+7, 2, SSD1306_WHITE);
    display.fillCircle(FCX, skirtTop+8, 2, SSD1306_WHITE);
  } else if (mood == MOOD_HAPPY) {
    display.fillTriangle(FCX, skirtTop-2, FCX-10, skirtTop+8, FCX+10, skirtTop+8, SSD1306_WHITE);
    display.fillCircle(FCX-5, skirtTop+7, 2, SSD1306_WHITE);
    display.fillCircle(FCX+5, skirtTop+7, 2, SSD1306_WHITE);
  } else if (mood == MOOD_WORRY) {
    display.fillTriangle(FCX, skirtTop-2, FCX-6, skirtTop+5, FCX+6, skirtTop+5, SSD1306_WHITE);
  } else if (mood == MOOD_SAD) {
    display.fillTriangle(FCX, skirtTop-2, FCX-4, skirtTop+4, FCX+4, skirtTop+4, SSD1306_WHITE);
  } else {
    display.fillTriangle(FCX, skirtTop-2, FCX-8, skirtTop+6, FCX+8, skirtTop+6, SSD1306_WHITE);
  }

  // ===== 3. 身体 =====
  display.fillRect(FCX-2+bodyOff, neckY+1, 5-bodyOff*2, 10, SSD1306_WHITE);

  // ===== 4. 头部 =====
  int hx = FCX + headOffX;
  display.fillCircle(hx, HEAD_Y, HEAD_R, SSD1306_WHITE);

  // ===== 5. 头发 =====
  if (mood == MOOD_JOY) {
    display.drawLine(hx-5, HEAD_Y-6, hx-14, HEAD_Y-14, SSD1306_WHITE);
    display.drawLine(hx-4, HEAD_Y-7, hx-13, HEAD_Y-16, SSD1306_WHITE);
    display.drawLine(hx-6, HEAD_Y-4, hx-16, HEAD_Y-10, SSD1306_WHITE);
    display.drawLine(hx+5, HEAD_Y-6, hx+10, HEAD_Y-3, SSD1306_WHITE);
    display.fillCircle(hx-3, HEAD_Y-9, 1, SSD1306_BLACK);
    display.drawCircle(hx-3, HEAD_Y-9, 2, SSD1306_WHITE);
    display.fillCircle(hx+3, HEAD_Y-9, 1, SSD1306_BLACK);
    display.drawCircle(hx+3, HEAD_Y-9, 2, SSD1306_WHITE);
  } else if (mood == MOOD_HAPPY) {
    display.drawLine(hx-4, HEAD_Y-5, hx-12, HEAD_Y-10, SSD1306_WHITE);
    display.drawLine(hx-3, HEAD_Y-6, hx-11, HEAD_Y-12, SSD1306_WHITE);
    display.drawLine(hx-5, HEAD_Y-3, hx-14, HEAD_Y-7, SSD1306_WHITE);
    display.drawLine(hx+5, HEAD_Y-5, hx+9, HEAD_Y-2, SSD1306_WHITE);
    display.fillCircle(hx-2, HEAD_Y-8, 1, SSD1306_BLACK);
    display.drawCircle(hx-2, HEAD_Y-8, 2, SSD1306_WHITE);
  } else if (mood == MOOD_WORRY) {
    display.drawLine(hx-5, HEAD_Y-4, hx-10, HEAD_Y-1, SSD1306_WHITE);
    display.drawLine(hx+5, HEAD_Y-4, hx+10, HEAD_Y-1, SSD1306_WHITE);
    display.fillCircle(hx+4, HEAD_Y-7, 1, SSD1306_BLACK);
    display.drawCircle(hx+4, HEAD_Y-7, 2, SSD1306_WHITE);
  } else if (mood == MOOD_SAD) {
    display.drawLine(hx-6, HEAD_Y-3, hx-9, HEAD_Y+4, SSD1306_WHITE);
    display.drawLine(hx+6, HEAD_Y-3, hx+9, HEAD_Y+4, SSD1306_WHITE);
  } else {
    display.drawLine(hx-5, HEAD_Y-4, hx-9, HEAD_Y-1, SSD1306_WHITE);
    display.drawLine(hx+5, HEAD_Y-4, hx+9, HEAD_Y-1, SSD1306_WHITE);
    display.fillCircle(hx+3, HEAD_Y-8, 1, SSD1306_BLACK);
    display.drawCircle(hx+3, HEAD_Y-8, 2, SSD1306_WHITE);
  }

  // ===== 6. 面部 =====
  int eyeL = hx - 3, eyeR = hx + 3;
  int eyeY = HEAD_Y - 1;

  if (bigB) {
    display.drawLine(eyeL-3, eyeY, eyeL+3, eyeY, SSD1306_BLACK);
    display.drawLine(eyeR-3, eyeY, eyeR+3, eyeY, SSD1306_BLACK);
  } else if (blink) {
    display.drawLine(eyeL-2, eyeY, eyeL+2, eyeY, SSD1306_BLACK);
    display.drawLine(eyeR-2, eyeY, eyeR+2, eyeY, SSD1306_BLACK);
  } else {
    if (mood == MOOD_JOY) {
      display.fillCircle(eyeL, eyeY, 3, SSD1306_BLACK);
      display.fillCircle(eyeR, eyeY, 3, SSD1306_BLACK);
      display.drawLine(eyeL-1, eyeY-2, eyeL+1, eyeY-2, SSD1306_WHITE);
      display.drawLine(eyeL, eyeY-3, eyeL, eyeY-1, SSD1306_WHITE);
      display.drawLine(eyeR-1, eyeY-2, eyeR+1, eyeY-2, SSD1306_WHITE);
      display.drawLine(eyeR, eyeY-3, eyeR, eyeY-1, SSD1306_WHITE);
    } else if (mood == MOOD_HAPPY) {
      display.fillCircle(eyeL, eyeY, 2, SSD1306_BLACK);
      display.fillCircle(eyeR, eyeY, 2, SSD1306_BLACK);
      display.fillCircle(eyeL+1, eyeY-1, 1, SSD1306_WHITE);
      display.fillCircle(eyeR+1, eyeY-1, 1, SSD1306_WHITE);
    } else if (mood == MOOD_WORRY) {
      display.fillCircle(eyeL, eyeY+1, 1, SSD1306_BLACK);
      display.fillCircle(eyeR, eyeY+1, 1, SSD1306_BLACK);
      display.drawLine(eyeL-2, eyeY-2, eyeL+2, eyeY-1, SSD1306_BLACK);
      display.drawLine(eyeR-2, eyeY-1, eyeR+2, eyeY-2, SSD1306_BLACK);
    } else if (mood == MOOD_SAD) {
      display.fillCircle(eyeL, eyeY+1, 2, SSD1306_BLACK);
      display.fillCircle(eyeR, eyeY+1, 2, SSD1306_BLACK);
      display.drawLine(eyeL-2, eyeY+3, eyeL-2, eyeY+5, SSD1306_BLACK);
    } else {
      display.fillCircle(eyeL, eyeY, 2, SSD1306_BLACK);
      display.fillCircle(eyeR, eyeY, 2, SSD1306_BLACK);
      display.fillCircle(eyeL+1, eyeY-1, 1, SSD1306_WHITE);
      display.fillCircle(eyeR+1, eyeY-1, 1, SSD1306_WHITE);
    }
  }

  // 嘴
  int mouthY = HEAD_Y + 3;
  if (mood == MOOD_JOY) {
    display.drawLine(hx-3, mouthY, hx, mouthY+2, SSD1306_BLACK);
    display.drawLine(hx, mouthY+2, hx+3, mouthY, SSD1306_BLACK);
  } else if (mood == MOOD_HAPPY) {
    display.drawLine(hx-2, mouthY, hx, mouthY+1, SSD1306_BLACK);
    display.drawLine(hx, mouthY+1, hx+2, mouthY, SSD1306_BLACK);
  } else if (mood == MOOD_WORRY) {
    display.drawLine(hx-2, mouthY, hx-1, mouthY+1, SSD1306_BLACK);
    display.drawLine(hx-1, mouthY+1, hx+1, mouthY, SSD1306_BLACK);
    display.drawLine(hx+1, mouthY, hx+2, mouthY+1, SSD1306_BLACK);
  } else if (mood == MOOD_SAD) {
    display.drawLine(hx-2, mouthY+1, hx, mouthY, SSD1306_BLACK);
    display.drawLine(hx, mouthY, hx+2, mouthY+1, SSD1306_BLACK);
  } else {
    display.drawLine(hx-1, mouthY, hx+1, mouthY, SSD1306_BLACK);
  }

  // 腮红
  if (mood != MOOD_SAD) {
    display.drawCircle(hx-5, mouthY-1, 1, SSD1306_BLACK);
    display.drawCircle(hx+5, mouthY-1, 1, SSD1306_BLACK);
    if (mood == MOOD_JOY) {
      display.drawCircle(hx-5, mouthY-1, 2, SSD1306_BLACK);
      display.drawCircle(hx+5, mouthY-1, 2, SSD1306_BLACK);
    }
  }

  // ===== 7. 手臂 =====
  if (mood == MOOD_SAD) {
    display.drawLine(FCX-2, shoulderY+2, FCX-4, shoulderY+8, SSD1306_WHITE);
    display.drawLine(FCX+2, shoulderY+2, FCX+4, shoulderY+8, SSD1306_WHITE);
  } else if (mood == MOOD_JOY && armAct == 6) {
    display.drawLine(FCX-2, shoulderY+2, FCX-10, shoulderY-6, SSD1306_WHITE);
    display.drawLine(FCX+2, shoulderY+2, FCX+10, shoulderY-6, SSD1306_WHITE);
    display.fillCircle(FCX-10, shoulderY-7, 2, SSD1306_WHITE);
    display.fillCircle(FCX+10, shoulderY-7, 2, SSD1306_WHITE);
  } else {
    switch(armAct) {
      case 0:
        display.drawLine(FCX-2, shoulderY+2, FCX-5, shoulderY+10, SSD1306_WHITE);
        display.drawLine(FCX+2, shoulderY+2, FCX+5, shoulderY+10, SSD1306_WHITE);
        break;
      case 1:
        display.drawLine(FCX-2, shoulderY+2, FCX-8, shoulderY-4, SSD1306_WHITE);
        display.fillCircle(FCX-8, shoulderY-5, 2, SSD1306_WHITE);
        display.drawLine(FCX+2, shoulderY+2, FCX+5, shoulderY+10, SSD1306_WHITE);
        break;
      case 2:
        display.drawLine(FCX-2, shoulderY+2, FCX-5, shoulderY+10, SSD1306_WHITE);
        display.drawLine(FCX+2, shoulderY+2, FCX+8, shoulderY-4, SSD1306_WHITE);
        display.fillCircle(FCX+8, shoulderY-5, 2, SSD1306_WHITE);
        break;
      case 3:
        display.drawLine(FCX-2, shoulderY+2, hx-4, HEAD_Y+2, SSD1306_WHITE);
        display.fillCircle(hx-4, HEAD_Y+1, 2, SSD1306_WHITE);
        display.drawLine(FCX+2, shoulderY+2, FCX+5, shoulderY+10, SSD1306_WHITE);
        break;
      case 4:
        display.drawLine(FCX-2, shoulderY+2, FCX-6, shoulderY+4, SSD1306_WHITE);
        display.drawLine(FCX+2, shoulderY+2, FCX+6, shoulderY+4, SSD1306_WHITE);
        display.fillCircle(FCX-6, shoulderY+3, 2, SSD1306_WHITE);
        display.fillCircle(FCX+6, shoulderY+3, 2, SSD1306_WHITE);
        display.fillCircle(FCX, shoulderY+2, 1, SSD1306_BLACK);
        display.drawCircle(FCX, shoulderY+2, 2, SSD1306_WHITE);
        break;
      case 5:
        display.drawLine(FCX-2, shoulderY+2, FCX-14, shoulderY+2, SSD1306_WHITE);
        display.fillCircle(FCX-14, shoulderY+2, 1, SSD1306_WHITE);
        display.drawLine(FCX+2, shoulderY+2, FCX+5, shoulderY+10, SSD1306_WHITE);
        break;
    }
  }

  // ===== 8. 腿 =====
  if (mood == MOOD_JOY) {
    if (footP % 2 == 0) {
      display.drawLine(FCX-2, hipY, FCX-7, footY-3, SSD1306_WHITE);
      display.drawLine(FCX+2, hipY, FCX+4, footY, SSD1306_WHITE);
      display.fillCircle(FCX-7, footY-2, 2, SSD1306_WHITE);
      display.fillCircle(FCX+4, footY+1, 1, SSD1306_WHITE);
    } else {
      display.drawLine(FCX-2, hipY, FCX-4, footY, SSD1306_WHITE);
      display.drawLine(FCX+2, hipY, FCX+7, footY-3, SSD1306_WHITE);
      display.fillCircle(FCX-4, footY+1, 1, SSD1306_WHITE);
      display.fillCircle(FCX+7, footY-2, 2, SSD1306_WHITE);
    }
  } else if (mood == MOOD_SAD) {
    display.drawLine(FCX-1, hipY, FCX-3, hipY+6, SSD1306_WHITE);
    display.drawLine(FCX+1, hipY, FCX+3, hipY+6, SSD1306_WHITE);
  } else if (mood == MOOD_WORRY) {
    display.drawLine(FCX-1, hipY, FCX-2, footY, SSD1306_WHITE);
    display.drawLine(FCX+1, hipY, FCX+2, footY, SSD1306_WHITE);
  } else {
    int sw = (footP % 2 == 0) ? 0 : 1;
    display.drawLine(FCX-2, hipY, FCX-3-sw, footY, SSD1306_WHITE);
    display.drawLine(FCX+2, hipY, FCX+3+sw, footY, SSD1306_WHITE);
    display.fillCircle(FCX-3-sw, footY+1, 1, SSD1306_WHITE);
    display.fillCircle(FCX+3+sw, footY+1, 1, SSD1306_WHITE);
  }

  // ===== 9. 魔法效果 =====
  if (mood == MOOD_JOY) {
    display.fillCircle(FCX+28, shoulderY+wingOff-4, 1, SSD1306_WHITE);
    display.fillCircle(FCX-22, shoulderY+wingOff-2, 1, SSD1306_WHITE);
    display.fillCircle(FCX+16, HEAD_Y+8, 1, SSD1306_WHITE);
    display.fillCircle(FCX-18, HEAD_Y+5, 1, SSD1306_WHITE);
    int petalY = 56 + (wingP % 4);
    display.fillCircle(FCX-10, petalY, 1, SSD1306_BLACK);
    display.drawCircle(FCX-10, petalY, 2, SSD1306_WHITE);
  } else if (mood == MOOD_HAPPY) {
    display.fillCircle(FCX+20, shoulderY+wingOff-4, 1, SSD1306_WHITE);
    display.fillCircle(FCX-18, shoulderY+wingOff-2, 1, SSD1306_WHITE);
    display.fillCircle(FCX+15, HEAD_Y+10, 1, SSD1306_WHITE);
  }
}

// ========== 启动画面 ==========
void showBootScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCuteFairy(MOOD_OK, false, false, 0, 0, 0, 0, 0);
  display.setTextSize(1);
  display.setCursor(2, 52);
  display.print("Soil Fairy v5");
  display.display();
  delay(2000);
}

void showWiFiConnecting(int dots) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCuteFairy(MOOD_WORRY, false, false, 0, 0, 0, 3, 0);
  display.setTextSize(1);
  display.setCursor(2, 28);
  display.print("WiFi");
  for (int i = 0; i < (dots % 5); i++) display.print(".");
  display.setCursor(2, 40);
  display.print("Connecting");
  display.display();
}

void showWiFiConnected() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCuteFairy(MOOD_JOY, false, false, 2, 0, 0, 6, 0);
  display.setTextSize(1);
  display.setCursor(2, 28);
  display.print("WiFi OK!");
  display.setCursor(2, 40);
  display.print(WiFi.localIP());
  display.display();
  delay(1500);
}

void showWiFiFailed() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCuteFairy(MOOD_SAD, false, false, 0, 0, 0, 0, 0);
  display.setTextSize(1);
  display.setCursor(2, 28);
  display.print("AP Mode");
  display.setCursor(2, 40);
  display.print("192.168.4.1");
  display.display();
  delay(1500);
}

// ========== 传感器读取 ==========
void readSensor() {
  adc_raw = analogRead(A0);
  voltage = adc_raw * (3.3 / 1024.0);
  if (voltage >= dry_v) moisture = 0;
  else if (voltage <= wet_v) moisture = 100;
  else moisture = (dry_v - voltage) / (dry_v - wet_v) * 100.0;
  if (moisture < 0) moisture = 0;
  if (moisture > 100) moisture = 100;
}

// ========== 上传 ==========
void uploadData() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient wifiClient;
  HTTPClient http;
  http.begin(wifiClient, serverUrl);
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"adc\":" + String(adc_raw) +
                   ",\"voltage\":" + String(voltage, 2) +
                   ",\"moisture\":" + String(moisture, 1) + "}";
  int httpCode = http.POST(payload);
  hasUpload = true;
  uploadOk = (httpCode == 200);
  http.end();
}

// ========== 主显示界面(250ms刷新!) ==========
void showData() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  unsigned long now = millis();

  // === 动画更新 ===
  if (now - wingTimer >= WING_SPEED) {
    wingPhase = (wingPhase + 1) % 8;
    wingTimer = now;
  }
  if (now - breathTimer >= BREATH_SPEED) {
    breathPhase = (breathPhase + 1) % 4;
    breathTimer = now;
  }
  if (now - headTiltTimer >= HEAD_TILT_SPEED) {
    headTilt = (headTilt == 0) ? ((now % 2 == 0) ? -1 : 1) : 0;
    headTiltTimer = now;
  }

  // 眨眼(含大眨眼)
  if (!blinkState && !bigBlink && (now >= nextBlinkTime)) {
    if ((now % 7) == 0) bigBlink = true;
    else blinkState = true;
    blinkStart = now;
  }
  if (blinkState && (now - blinkStart >= 120)) {
    blinkState = false;
    nextBlinkTime = now + 2500 + (wingPhase * 600);
  }
  if (bigBlink && (now - blinkStart >= 200)) {
    bigBlink = false;
    nextBlinkTime = now + 3000;
  }

  // 手臂动作
  if (now - armTimer >= ARM_CHANGE_SPEED) {
    FairyMood m = getMood();
    if (m == MOOD_JOY) {
      armAction = (armAction == 6) ? 4 : 6;
    } else if (m == MOOD_HAPPY) {
      armAction = (armAction + 1) % 4;
      if (armAction == 3) armAction = 4;
    } else if (m == MOOD_WORRY) {
      armAction = 3;
    } else if (m == MOOD_SAD) {
      armAction = 0;
    } else {
      int r = (now / 2000) % 5;
      armAction = (r <= 1) ? 0 : (r == 2 ? 1 : (r == 3 ? 5 : 0));
    }
    armTimer = now;
  }

  // 脚部
  if (now - footTimer >= FOOT_SPEED) {
    footPhase = (footPhase + 1) % 4;
    footTimer = now;
  }

  FairyMood mood = getMood();

  // === 左侧文字(0-38px) ===
  int mVal = (int)moisture;
  display.setTextSize(2);
  display.setCursor(2, 0);
  display.print(mVal);
  display.setTextSize(1);
  int pctX;
  if (mVal < 10) pctX = 2 + 12;
  else if (mVal < 100) pctX = 2 + 24;
  else pctX = 2 + 36;
  display.setCursor(pctX, 4);
  display.print("%");

  display.setCursor(2, 18);
  if (mood == MOOD_JOY) display.print("Wet!");
  else if (mood == MOOD_HAPPY) display.print("Wet~");
  else if (mood == MOOD_OK) display.print("OK");
  else if (mood == MOOD_WORRY) display.print("Dry?");
  else display.print("DRY!");

  display.setCursor(2, 28);
  display.print(voltage, 1);
  display.print("V");

  if (WiFi.status() == WL_CONNECTED) {
    display.setCursor(2, 40);
    display.print("WiFi");
    display.setCursor(2, 52);
    display.print(WiFi.localIP());
  } else {
    display.setCursor(2, 40);
    display.print("WiFi OFF");
  }

  // 上传状态点
  if (hasUpload) {
    display.fillCircle(36, 52, 1, uploadOk ? SSD1306_WHITE : SSD1306_BLACK);
    display.drawCircle(36, 52, 1, SSD1306_WHITE);
  }

  // === 右侧花仙子 ===
  drawCuteFairy(mood, blinkState, bigBlink, wingPhase, breathPhase, headTilt, armAction, footPhase);

  display.display();
}

// ========== setup ==========
void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED fail");
    return;
  }
  display.setTextColor(SSD1306_WHITE);
  showBootScreen();

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    showWiFiConnecting(attempts);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
    showWiFiConnected();
  } else {
    Serial.println("WiFi FAILED");
    showWiFiFailed();
    WiFi.softAP("SoilSensor", "12345678");
  }

  // 初始化动画计时器
  wingTimer = millis();
  breathTimer = millis();
  headTiltTimer = millis();
  armTimer = millis();
  footTimer = millis();
  lastDisplayTime = millis();
  lastSensorTime = millis();
  lastUploadTime = millis();
}

// ========== loop(拆分循环!) ==========
void loop() {
  unsigned long now = millis();

  // 显示刷新(250ms = 4fps)
  if (now - lastDisplayTime >= DISPLAY_INTERVAL) {
    showData();
    lastDisplayTime = now;
  }

  // 传感器读取(5s)
  if (now - lastSensorTime >= SENSOR_INTERVAL) {
    readSensor();
    lastSensorTime = now;
  }

  // 数据上传(5s)
  if (now - lastUploadTime >= UPLOAD_INTERVAL) {
    uploadData();
    lastUploadTime = now;
  }
}
