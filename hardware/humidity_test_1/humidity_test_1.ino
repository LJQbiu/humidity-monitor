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

// ========== OLED 显示函数 ==========

// 获取湿度状态文字
const char* getMoistureLabel() {
  if (moisture < 20) return "DRY";
  if (moisture < 40) return "Low";
  if (moisture < 60) return "OK";
  if (moisture < 80) return "Wet";
  return "Full";
}

void showBootScreen() {
  display.clearDisplay();
  // 花仙子居中显示
  display.drawBitmap(52, 8, fairy_happy, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 50);
  display.print("Soil Sensor v1.0");
  // 小花装饰
  display.drawPixel(46, 44, SSD1306_WHITE);
  display.drawPixel(80, 44, SSD1306_WHITE);
  display.display();
}

void showWiFiConnecting(int dots) {
  display.clearDisplay();
  // 小花仙子在等待
  display.drawBitmap(4, 8, fairy_happy, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(36, 8);
  display.print("WiFi...");
  // 动态点
  display.setCursor(36, 22);
  for (int i = 0; i < (dots % 5); i++) {
    display.print(".");
  }
  display.display();
}

void showWiFiConnected() {
  display.clearDisplay();
  // 花仙子开心！WiFi连上了
  display.drawBitmap(4, 8, fairy_happy, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(36, 8);
  display.print("WiFi OK!");
  display.setCursor(36, 22);
  display.print(WiFi.localIP());
  display.display();
  delay(1000);
}

void showWiFiFailed() {
  display.clearDisplay();
  // 花仙子有点伤心
  display.drawBitmap(4, 8, fairy_sad, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(36, 8);
  display.print("AP Mode");
  display.setCursor(36, 22);
  display.print("192.168.4.1");
  display.display();
  delay(1000);
}

// ========== 主界面（花仙子为主角）==========
void showData() {
  display.clearDisplay();
  
  // === 花仙子 - 主角！(左侧 x=4, y=4, 24x32) ===
  const uint8_t* fairy = getFairyBitmap(moisture);
  display.drawBitmap(4, 4, fairy, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  
  // === 湿度大字 (右侧) ===
  int mVal = (int)moisture;
  display.setTextSize(3);
  display.setCursor(38, 6);
  display.print(mVal);
  
  // %号
  display.setTextSize(2);
  int pctX;
  if (mVal < 10) pctX = 38 + 18;
  else if (mVal < 100) pctX = 38 + 36;
  else pctX = 38 + 54;
  display.setCursor(pctX, 12);
  display.print("%");
  
  // === 湿度标签 ===
  display.setTextSize(1);
  display.setCursor(38, 34);
  display.print(getMoistureLabel());
  
  // === 小花装饰（花仙子脚下）===
  display.drawPixel(8, 40, SSD1306_WHITE);
  display.drawPixel(20, 40, SSD1306_WHITE);
  display.drawPixel(14, 42, SSD1306_WHITE);
  display.drawPixel(10, 44, SSD1306_WHITE);
  display.drawPixel(18, 44, SSD1306_WHITE);
  
  // === WiFi指示（右下角小点）===
  bool wifiOn = (WiFi.status() == WL_CONNECTED);
  if (wifiOn) {
    display.fillCircle(122, 58, 3, SSD1306_WHITE);  // WiFi在线：实心圆
  } else {
    display.drawCircle(122, 58, 3, SSD1306_WHITE);  // WiFi离线：空心圆
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
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"adc\":" + String(adc_raw) + 
                   ",\"voltage\":" + String(voltage) + 
                   ",\"moisture\":" + String(moisture) + "}";
  
  int httpCode = http.POST(payload);
  
  hasUpload = true;
  uploadOk = (httpCode == 200);
  
  http.end();
}

void setup() {
  Serial.begin(115200);
  
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
  
  delay(2000);
}

void loop() {
  readSensor();
  uploadData();
  showData();
  delay(5000);
}