#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ========== 类型定义(必须在最前，避免Arduino预处理器原型生成问题) ==========
enum FairyMood { MOOD_JOY, MOOD_HAPPY, MOOD_OK, MOOD_WORRY, MOOD_SAD };

// ========== AP模式 ==========
bool isAPMode = false;
ESP8266WebServer apServer(80);
DNSServer dnsServer;

// ========== OLED 配置 ==========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========== WiFi 配置 ==========
String ssid = "LJQ";
String password = "12345678";
const char* serverUrl = "http://101.37.19.59:5000/api/data";

// ========== EEPROM 持久化 ==========
#define EEPROM_SIZE 128
#define SSID_ADDR 0
#define PASS_ADDR 64
#define MAGIC_ADDR 126
#define MAGIC_VAL 0xA5

void loadWiFiFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(MAGIC_ADDR) == MAGIC_VAL) {
    char buf[64];
    for (int i = 0; i < 63; i++) buf[i] = EEPROM.read(SSID_ADDR + i);
    buf[63] = 0;
    String savedSsid = String(buf);
    for (int i = 0; i < 63; i++) buf[i] = EEPROM.read(PASS_ADDR + i);
    buf[63] = 0;
    String savedPass = String(buf);
    if (savedSsid.length() > 0) {
      ssid = savedSsid;
      password = savedPass;
      Serial.println("EEPROM: loaded SSID=" + ssid);
    }
  } else {
    Serial.println("EEPROM: no saved config, using defaults");
  }
  EEPROM.end();
}

void saveWiFiToEEPROM(String newSsid, String newPass) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 63; i++) {
    EEPROM.write(SSID_ADDR + i, i < newSsid.length() ? newSsid[i] : 0);
    EEPROM.write(PASS_ADDR + i, i < newPass.length() ? newPass[i] : 0);
  }
  EEPROM.write(MAGIC_ADDR, MAGIC_VAL);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("EEPROM: saved SSID=" + newSsid);
}

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

void drawCuteFairy(FairyMood mood, bool isBlink, bool bigB, int wingP, int breathP, int tilt, int armAct, int footP) {
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
  } else if (isBlink) {
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
  Serial.println("Upload: " + String(uploadOk ? "OK" : "FAIL") + " code=" + String(httpCode) + " data=" + payload);
  http.end();
}

// ========== AP模式 Web服务器 ==========

void handleApScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":\"" + String(WiFi.RSSI(i)) + "\",\"enc\":" + String(WiFi.encryptionType(i) != ENC_TYPE_NONE) + "}";
  }
  json += "]";
  apServer.send(200, "application/json", json);
}

void handleApRoot() {
  String html = "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Soil Sensor</title>"
    "<style>body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;padding:20px}"
    ".card{background:#16213e;border-radius:12px;padding:20px;margin:10px 0}"
    "h1{color:#0f3460;margin:0}h2{color:#e94560}"
    ".val{font-size:2em;color:#00ff88;font-weight:bold}"
    ".bar{height:24px;background:#333;border-radius:12px;margin:8px 0}"
    ".fill{height:24px;border-radius:12px;background:linear-gradient(90deg,#e94560,#00ff88)}"
    "input{width:100%;padding:10px;margin:6px 0;border:1px solid #0f3460;border-radius:8px;background:#1a1a2e;color:#e0e0e0;box-sizing:border-box}"
    "button{width:100%;padding:12px;background:#e94560;color:#fff;border:none;border-radius:8px;font-size:1.1em;cursor:pointer}"
    ".wifi-item{padding:10px;border-bottom:1px solid #0f3460;cursor:pointer;display:flex;justify-content:space-between}"
    ".wifi-item:hover{background:#0f3460}"
    ".signal{color:#00ff88;font-size:0.9em}"
    ".lock{color:#e94560}"
    "#wifiList{margin:8px 0;border-radius:8px;background:#0d1b2a;overflow:hidden}"
    "#scanBtn{background:#0f3460;margin-bottom:8px}"
    ".spinner{display:inline-block;width:16px;height:16px;border:2px solid #0f3460;border-top:2px solid #e94560;border-radius:50%;animation:spin 1s linear infinite}"
    "@keyframes spin{to{transform:rotate(360deg)}}"
    "</style></head><body>"
    "<div class='card'><h1>🌱 Soil Sensor</h1></div>"
    "<div class='card'><h2>实时数据</h2>"
    "<div class='val'>" + String((int)moisture) + "%</div>"
    "<div class='bar'><div class='fill' style='width:" + String((int)moisture) + "%'></div></div>"
    "<p>电压: " + String(voltage, 2) + "V | ADC: " + String(adc_raw) + "</p></div>"
    "<div class='card'><h2>配置WiFi</h2>"
    "<button id='scanBtn' onclick='scanWifi()'>扫描附近WiFi</button>"
    "<div id='wifiList'></div>"
    "<form action='/connect' method='POST' id='connForm'>"
    "<input name='ssid' id='ssidInput' placeholder='WiFi名称（点击上方列表选择）' value='" + String(ssid) + "'>"
    "<input name='pass' id='passInput' placeholder='WiFi密码' type='password'>"
    "<button type='submit'>连接WiFi并重启</button></form></div>"
    "<script>"
    "function scanWifi(){"
    "var btn=document.getElementById('scanBtn');"
    "btn.innerHTML='<span class=\"spinner\"></span> 扫描中...';btn.disabled=true;"
    "fetch('/scan').then(r=>r.json()).then(list=>{"
    "btn.innerHTML='重新扫描';btn.disabled=false;"
    "var html='';"
    "list.sort((a,b)=>b.rssi-a.rssi);"
    "list.forEach(w=>{"
    "var sig=w.rssi;var bars=sig>-50?'4':sig>-60?'3':sig>-70?'2':'1';"
    "html+='<div class=\"wifi-item\" onclick=\"pickWifi(\\''+w.ssid.replace(/'/g,\"\\\\'\")+'\\',\\''+w.enc+'\\')\">'"
    "+'<span>'+w.ssid+'</span><span class=\"signal\">📶'+bars+(w.enc?'<span class=\"lock\">🔒</span>':'')+'</span></div>';"
    "});"
    "if(!html)html='<div style=\"padding:10px;color:#888\">未发现WiFi网络</div>';"
    "document.getElementById('wifiList').innerHTML=html;"
    "}).catch(e=>{btn.innerHTML='扫描失败,重试';btn.disabled=false;});}"
    "function pickWifi(ssid,enc){"
    "document.getElementById('ssidInput').value=ssid;"
    "if(enc=='0')document.getElementById('passInput').placeholder='开放网络,无需密码';"
    "else document.getElementById('passInput').placeholder='请输入WiFi密码';"
    "}"
    "scanWifi();"
    "</script></body></html>";
  apServer.send(200, "text/html", html);
}

void handleApConnect() {
  String newSsid = apServer.arg("ssid");
  String newPass = apServer.arg("pass");
  if (newSsid.length() == 0) {
    apServer.send(400, "text/plain", "SSID不能为空");
    return;
  }
  saveWiFiToEEPROM(newSsid, newPass);
  ssid = newSsid;
  password = newPass;
  apServer.send(200, "text/html",
    "<html><body><h2>正在连接 " + newSsid + "...</h2>"
    "<p>设备将重启尝试连接新WiFi。如果失败会再次进入AP模式。</p>"
    "<script>setTimeout(()=>location.reload(),10000)</script></body></html>");
  delay(1000);
  ESP.restart();
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
    if (hasUpload && uploadOk) display.print(" OK");
    else if (hasUpload) display.print(" FAIL");
    else display.print(" --");
    display.setCursor(2, 52);
    display.print(WiFi.localIP());
  } else if (isAPMode) {
    display.setCursor(2, 40);
    display.print("AP:SoilSensor");
    display.setCursor(2, 52);
    display.print("192.168.4.1");
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

  loadWiFiFromEEPROM();
  Serial.println("Connecting to: " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    showWiFiConnecting(attempts);
    Serial.println("WiFi retry: " + String(attempts));
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
    showWiFiConnected();
    isAPMode = false;
  } else {
    Serial.println("WiFi FAILED -> AP Mode");
    isAPMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SoilSensor", "12345678");
    dnsServer.start(53, "*", WiFi.softAPIP());
    showWiFiFailed();
    apServer.on("/", handleApRoot);
    apServer.on("/scan", handleApScan);
    apServer.on("/connect", HTTP_POST, handleApConnect);
    apServer.onNotFound(handleApRoot);
    apServer.begin();
    Serial.println("AP WebServer started at 192.168.4.1");
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

  // AP模式Web服务 + DNS重定向
  if (isAPMode) {
    dnsServer.processNextRequest();
    apServer.handleClient();
  }
}
