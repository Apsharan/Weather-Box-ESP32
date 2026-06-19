#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include "secrets.h"

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
Adafruit_BME280 bme;

const char* timezone = "CET-1CEST,M3.5.0/2,M10.5.0/3";

float indoorTemp = 0;
float indoorHum = 0;
float pressure = 0;

float outdoorTemp = 0;
float windSpeed = 0;
float rain = 0;
int rainChance = 0;
int weatherCode = 0;
String sunrise = "";
String sunset = "";

unsigned long lastWeatherUpdate = 0;
unsigned long lastSensorUpdate = 0;

void readIndoorSensor() {
  indoorTemp = bme.readTemperature();
  indoorHum = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
}

void getWeatherData() {
  String url =
    "https://api.open-meteo.com/v1/forecast?"
    "latitude=48.91&longitude=2.33"
    "&current=temperature_2m,weather_code,wind_speed_10m,rain"
    "&daily=sunrise,sunset,precipitation_probability_max"
    "&timezone=Europe/Paris";

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      outdoorTemp = doc["current"]["temperature_2m"];
      weatherCode = doc["current"]["weather_code"];
      windSpeed = doc["current"]["wind_speed_10m"];
      rain = doc["current"]["rain"];
      rainChance = doc["daily"]["precipitation_probability_max"][0];
      sunrise = doc["daily"]["sunrise"][0].as<String>();
      sunset = doc["daily"]["sunset"][0].as<String>();

      Serial.println("Weather updated");
    } else {
      Serial.println("JSON parse failed");
    }
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
  }

  http.end();
}

void drawWifiIcon(int x, int y, int level, uint16_t color) {
  tft.fillRect(x - 90, y - 100, 180, 120, ILI9341_BLACK);
  tft.fillCircle(x, y, 7, color);

  if (level >= 1) {
    for (int r = 24; r <= 28; r++) {
      for (int angle = 220; angle <= 320; angle++) {
        float rad = angle * DEG_TO_RAD;
        tft.drawPixel(x + cos(rad) * r, y + sin(rad) * r, color);
      }
    }
  }

  if (level >= 2) {
    for (int r = 48; r <= 54; r++) {
      for (int angle = 220; angle <= 320; angle++) {
        float rad = angle * DEG_TO_RAD;
        tft.drawPixel(x + cos(rad) * r, y + sin(rad) * r, color);
      }
    }
  }

  if (level >= 3) {
    for (int r = 74; r <= 82; r++) {
      for (int angle = 220; angle <= 320; angle++) {
        float rad = angle * DEG_TO_RAD;
        tft.drawPixel(x + cos(rad) * r, y + sin(rad) * r, color);
      }
    }
  }
}

void connectWiFi() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 30);
  tft.println("Connecting WiFi");

  WiFi.begin(ssid, password);

  int level = 0;

  while (WiFi.status() != WL_CONNECTED) {
    drawWifiIcon(120, 240, level, ILI9341_CYAN);
    level++;
    if (level > 3) level = 0;
    delay(400);
  }

  tft.fillScreen(ILI9341_BLACK);
  drawWifiIcon(120, 240, 3, ILI9341_GREEN);

  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.setCursor(30, 30);
  tft.println("WiFi Connected");

  delay(1000);
}

void setupTime() {
  configTzTime(timezone, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.println("Waiting for time...");
  }

  Serial.println("Time OK");
}

void drawStaticLayout() {
  tft.fillScreen(ILI9341_BLACK);

  tft.drawFastHLine(0, 105, 320, ILI9341_WHITE);

  tft.setTextSize(2);

  tft.setTextColor(ILI9341_GREEN);
  tft.setCursor(20, 125);
  tft.println("INDOOR");

  tft.setTextColor(ILI9341_ORANGE);
  tft.setCursor(20, 220);
  tft.println("OUTDOOR");
}

void updateTimeDisplay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char timeStr[20];
  char dateStr[30];

  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);

  static String lastTime = "";
  static String lastDate = "";

  if (String(timeStr) != lastTime) {
    tft.fillRect(60, 20, 180, 40, ILI9341_BLACK);
    tft.setTextColor(ILI9341_CYAN);
    tft.setTextSize(4);
    tft.setCursor(60, 20);
    tft.println(timeStr);
    lastTime = String(timeStr);
  }

  if (String(dateStr) != lastDate) {
    tft.fillRect(90, 70, 150, 25, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(60, 70);
    tft.println(dateStr);
    lastDate = String(dateStr);
  }
}

void updateIndoorDisplay() {
  static int lastTemp10 = -9999;
  static int lastHum = -9999;

  int temp10 = indoorTemp * 10;
  int hum = indoorHum;

  if (temp10 != lastTemp10) {
    tft.fillRect(20, 155, 180, 22, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 155);
    tft.print("Temp: ");
    tft.print(indoorTemp, 1);
    tft.println(" C");

    lastTemp10 = temp10;
  }

  if (hum != lastHum) {
    tft.fillRect(20, 180, 180, 22, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 180);
    tft.print("Hum : ");
    tft.print(indoorHum, 0);
    tft.println(" %");

    lastHum = hum;
  }
}

void updateOutdoorDisplay() {
  static int lastOutTemp10 = -9999;
  static int lastWind = -9999;
  static int lastRainChance = -9999;

  int outTemp10 = outdoorTemp * 10;
  int wind = windSpeed;

  if (outTemp10 != lastOutTemp10) {
    tft.fillRect(20, 250, 180, 22, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 250);
    tft.print("Temp: ");
    tft.print(outdoorTemp, 1);
    tft.println(" C");

    lastOutTemp10 = outTemp10;
  }

  if (wind != lastWind) {
    tft.fillRect(20, 275, 200, 22, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 275);
    tft.print("Wind: ");
    tft.print(windSpeed, 0);
    tft.println(" km/h");

    lastWind = wind;
  }

  if (rainChance != lastRainChance) {
    tft.fillRect(20, 300, 180, 22, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 300);
    tft.print("Rain: ");
    tft.print(rainChance);
    tft.println(" %");

    lastRainChance = rainChance;
  }
}

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(4);
  tft.fillScreen(ILI9341_BLACK);

  if (!bme.begin(0x76)) {
    Serial.println("BME280 not found");
  } else {
    Serial.println("BME280 OK");
  }

  connectWiFi();
  setupTime();

  readIndoorSensor();
  getWeatherData();

  drawStaticLayout();
  updateTimeDisplay();
  updateIndoorDisplay();
  updateOutdoorDisplay();

  lastWeatherUpdate = millis();
  lastSensorUpdate = millis();
}

void loop() {
  if (millis() - lastSensorUpdate > 5000) {
    readIndoorSensor();
    updateIndoorDisplay();
    lastSensorUpdate = millis();
  }

  updateTimeDisplay();

  if (millis() - lastWeatherUpdate > 600000) {
    getWeatherData();
    updateOutdoorDisplay();
    lastWeatherUpdate = millis();
  }

  delay(200);
}