#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "fairy_bitmaps.h"

// ========== OLED 配置 ==========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========== WiFi 配置 ==========
const char* ssid = "LJQ";
const char* password = "12345678";

// ========== 服务器 ==========
const char* serverUrl = "http://101.37.19.59:5000/api/data";

// ========== 传感器 ==========
int adc_raw = 0;
float voltage = 0, moisture = 0;
float dry_v = 2.5, wet_v = 1.0;

// ========== 上传状态追踪 ==========
bool uploadOk = false;
bool hasUpload = false;

// ========== 动画系统 ==========
unsigned long lastFrameTime = 0;
int animFrame = 0;       // 0=base, 1=blink, 2=wing flap
const unsigned long FRAME_INTERVAL = 3000;  // 3秒切换帧

// ========== OLED 显示函数 ==========

const char* getMoistureLabel() {
  if (moisture < 20) return "DRY!";
  if (moisture < 40) return "Low";
  if (moisture < 60) return "OK";
  if (moisture < 80) return "Wet";
  return "Full";
}

void showBootScreen() {
  display.clearDisplay();
  display.drawBitmap(52, 8, fairy_happy, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 46);
  display.print("Soil Fairy v2.0");
  // 小花装饰
  display.drawPixel(46, 42, SSD1306_WHITE);
  display.drawPixel(80, 42, SSD1306_WHITE);
  display.drawPixel(60, 44, SSD1306_WHITE);
  display.display();
}

void showWiFiConnecting(int dots) {
  display.clearDisplay();
  display.drawBitmap(4, 8, fairy_normal, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(36, 8);
  display.print("WiFi...");
  display.setCursor(36, 22);
  for (int i = 0; i < (dots % 5); i++) {
    display.print(".");
  }
  // 等待动画提示
  display.setCursor(36, 36);
  display.print("Connecting");
  display.display();
}

void showWiFiConnected() {
  display.clearDisplay();
  display.drawBitmap(4, 8, fairy_happy, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(36, 8);
  display.print("WiFi OK!");
  display.setCursor(36, 22);
  display.print(WiFi.localIP());
  // 小花庆祝
  display.drawPixel(32, 40, SSD1306_WHITE);
  display.drawPixel(36, 42, SSD1306_WHITE);
  display.display();
  delay(1500);
}

void showWiFiFailed() {
  display.clearDisplay();
  display.drawBitmap(4, 8, fairy_sad, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(36, 8);
  display.print("AP Mode");
  display.setCursor(36, 22);
  display.print("192.168.4.1");
  display.display();
  delay(1500);
}

// ========== 主界面：花仙子 + 湿度 + WiFi ==========
void showData() {
  display.clearDisplay();
  
  // === 动画帧更新 ===
  unsigned long now = millis();
  if (now - lastFrameTime >= FRAME_INTERVAL) {
    animFrame = (animFrame + 1) % 3;  // 0→1→2→0 循环
    lastFrameTime = now;
  }
  
  // === 花仙子状态 + 动画帧 ===
  FairyState state = getFairyState(moisture);
  const uint8_t* fairy = getFairyFrame(state, animFrame);
  display.drawBitmap(2, 4, fairy, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  
  // === 小花装饰（花仙子脚下）===
  display.drawPixel(6, 40, SSD1306_WHITE);
  display.drawPixel(18, 40, SSD1306_WHITE);
  display.drawPixel(12, 42, SSD1306_WHITE);
  display.drawPixel(8, 44, SSD1306_WHITE);
  display.drawPixel(16, 44, SSD1306_WHITE);
  
  // === 湿度大字 (右侧) ===
  int mVal = (int)moisture;
  display.setTextSize(3);
  display.setCursor(38, 2);
  display.print(mVal);
  
  // %号 (紧跟数字后面)
  display.setTextSize(2);
  int pctX;
  if (mVal < 10) pctX = 38 + 18;
  else if (mVal < 100) pctX = 38 + 36;
  else pctX = 38 + 54;
  display.setCursor(pctX, 8);
  display.print("%");
  
  // === 湿度标签 ===
  display.setTextSize(1);
  display.setCursor(38, 28);
  display.print(getMoistureLabel());
  
  // === 电压 + ADC ===
  display.setCursor(38, 38);
  display.print("V:");
  display.print(voltage, 1);
  display.print("V");
  display.setCursor(80, 38);
  display.print("A:");
  display.print(adc_raw);
  
  // === WiFi IP (右下) ===
  bool wifiOn = (WiFi.status() == WL_CONNECTED);
  if (wifiOn) {
    display.setTextSize(1);
    display.setCursor(2, 52);
    display.print(WiFi.localIP());
  } else {
    display.setTextSize(1);
    display.setCursor(2, 52);
    display.print("WiFi OFF");
  }
  
  // === 上传状态指示 (右下角) ===
  if (hasUpload) {
    if (uploadOk) {
      display.fillCircle(122, 58, 3, SSD1306_WHITE);  // 上传成功：实心圆
    } else {
      display.drawCircle(122, 58, 3, SSD1306_WHITE);  // 上传失败：空心圆
    }
  } else {
    display.drawPixel(122, 58, SSD1306_WHITE);  // 未上传：小点
  }
  
  display.display();
}

// ========== 传感器 ==========
void readSensor() {
  adc_raw = analogRead(A0);
  voltage = (adc_raw / 1023.0) * 3.3;
  moisture = (dry_v - voltage) / (dry_v - wet_v) * 100.0;
  if (moisture < 0) moisture = 0;
  if (moisture > 100) moisture = 100;
}

// ========== 上传 ==========
void uploadData() {
  hasUpload = false;
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverUrl);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"adc\":" + String(adc_raw) + 
                   ",\"voltage\":" + String(voltage, 2) + 
                   ",\"moisture\":" + String(moisture, 1) + "}";
  
  int httpCode = http.POST(payload);
  
  hasUpload = true;
  uploadOk = (httpCode == 200);
  
  http.end();
}

void setup() {
  Wire.begin(4, 5);  // SDA=GPIO4, SCL=GPIO5
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }
  
  showBootScreen();
  delay(1500);
  
  WiFi.begin(ssid, password);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    showWiFiConnecting(retry % 10);
    delay(500);
    retry++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    showWiFiConnected();
  } else {
    showWiFiFailed();
    WiFi.softAP("SoilSensor", "12345678");
  }
  
  lastFrameTime = millis();
  animFrame = 0;
}

void loop() {
  readSensor();
  uploadData();
  showData();
  delay(2000);
}
