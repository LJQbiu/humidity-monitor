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
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 4);
  display.println("Soil");
  display.setCursor(8, 24);
  display.println("Moisture");
  display.setTextSize(1);
  display.setCursor(90, 56);
  display.print("v1.0");
  display.display();
}

void showWiFiConnecting(int dots) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi");
  display.println(ssid);
  
  // 动态进度点
  display.setCursor(0, 25);
  for (int i = 0; i < 10; i++) {
    if (i < dots) display.print("* ");
    else display.print(". ");
  }
  
  // 进度条
  int progress = dots * 10;
  display.drawRect(0, 45, 128, 10, SSD1306_WHITE);
  int fillW = progress * 124 / 100;
  if (fillW > 0) {
    display.fillRect(2, 47, fillW, 6, SSD1306_WHITE);
  }
  
  display.display();
}

void showWiFiConnected() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.println(ssid);
  display.print("IP:");
  display.println(WiFi.localIP());
  display.display();
}

void showWiFiFailed() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WiFi Failed!");
  display.println("AP: SoilSensor");
  display.println("PW: 12345678");
  display.println("IP: 192.168.4.1");
  display.display();
}

// ========== 主界面 ==========
void showData() {
  display.clearDisplay();
  
  // === 顶部状态栏 (y=0~8) ===
  display.setTextSize(1);
  bool wifiOn = (WiFi.status() == WL_CONNECTED);
  
  display.setCursor(0, 0);
  if (wifiOn) display.print("WiFi");
  else display.print("AP");
  
  display.setCursor(28, 0);
  if (hasUpload) {
    if (uploadOk) display.print("ok");
    else display.print("x");
  } else {
    display.print("--");
  }
  
  // 湿度等级(左半区域)
  display.setCursor(70, 0);
  display.print(getMoistureLabel());
  
  // 分隔线(只画左半，右侧留给花仙子)
  display.drawFastHLine(0, 10, 100, SSD1306_WHITE);
  
  // === 花仙子 (右侧 x=102, y=12, 24x32) ===
  const uint8_t* fairy = getFairyBitmap(moisture);
  display.drawBitmap(102, 12, fairy, FAIRY_WIDTH, FAIRY_HEIGHT, SSD1306_WHITE);
  
  // === 中央大号湿度 (左侧) ===
  int mVal = (int)moisture;
  display.setTextSize(3);
  display.setCursor(2, 14);
  display.print(mVal);
  
  // %号紧跟数字
  display.setTextSize(2);
  int pctX;
  if (mVal < 10) pctX = 2 + 18;
  else if (mVal < 100) pctX = 2 + 36;
  else pctX = 2 + 54;
  display.setCursor(pctX, 20);
  display.print("%");
  
  // === 进度条 (y=44~51, 左半) ===
  int barY = 44;
  int barH = 8;
  int barW = 100;
  display.drawRect(2, barY, barW, barH, SSD1306_WHITE);
  
  int fillW = (int)(barW * moisture / 100.0);
  if (fillW > 2) {
    display.fillRect(4, barY + 2, fillW - 3, barH - 4, SSD1306_WHITE);
    if (moisture > 70) {
      for (int i = 4; i < fillW; i += 4) {
        display.drawFastVLine(i, barY + 2, barH - 4, SSD1306_BLACK);
      }
    }
  }
  
  // 刻度: 25%/50%/75%
  for (int mark = 25; mark <= 75; mark += 25) {
    int mx = 2 + barW * mark / 100;
    display.drawFastVLine(mx, barY + 2, 4, SSD1306_BLACK);
  }
  
  // === 底部信息行 (y=56~63, 左半) ===
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("ADC:");
  display.print(adc_raw);
  display.setCursor(40, 56);
  display.print(voltage, 2);
  display.print("V");
  display.setCursor(76, 56);
  display.print(wifiOn ? "STA" : "AP");
  
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
    Serial.println("WiFi not connected, skip upload");
    return;
  }
  
  WiFiClient client;
  HTTPClient http;
  
  Serial.print("Connecting to: ");
  Serial.println(serverUrl);
  
  http.begin(client, serverUrl);
  http.setTimeout(10000);  // 10s timeout (default was 5s)
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"adc\":" + String(adc_raw) + 
                   ",\"voltage\":" + String(voltage) + 
                   ",\"moisture\":" + String(moisture) + "}";
  
  Serial.print("Payload: ");
  Serial.println(payload);
  
  int httpCode = http.POST(payload);
  
  Serial.print("HTTP code: ");
  Serial.println(httpCode);
  
  hasUpload = true;
  uploadOk = (httpCode == 200);
  
  if (!uploadOk) {
    Serial.print("Upload failed, code: ");
    Serial.println(httpCode);
    if (httpCode > 0) {
      String response = http.getString();
      Serial.print("Response: ");
      Serial.println(response);
    }
  } else {
    String response = http.getString();
    Serial.print("Upload OK! Response: ");
    Serial.println(response);
  }
  
  http.end();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Soil Sensor v1.0 ===");
  
  Wire.begin(4, 5);  // SDA=GPIO4, SCL=GPIO5
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 failed");
    for(;;);
  }
  Serial.println("OLED OK");
  
  showBootScreen();
  delay(1500);
  
  WiFi.begin(ssid, password);
  Serial.println("WiFi connecting...");
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    showWiFiConnecting(retry % 10);
    delay(500);
    retry++;
    if (retry % 10 == 0) {
      Serial.print("WiFi retry: ");
      Serial.println(retry);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    showWiFiConnected();
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    showWiFiFailed();
    Serial.println("WiFi FAILED -> AP mode");
    WiFi.softAP("SoilSensor", "12345678");
  }
  
  delay(2000);
}

void loop() {
  readSensor();
  uploadData();
  showData();
  
  Serial.print("ADC:");
  Serial.print(adc_raw);
  Serial.print(" V:");
  Serial.print(voltage, 2);
  Serial.print(" M:");
  Serial.print(moisture, 1);
  Serial.println("%");
  
  delay(5000);
}