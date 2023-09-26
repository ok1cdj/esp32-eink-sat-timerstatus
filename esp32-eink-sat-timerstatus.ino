/*
  ESPink- Satellite timer status by OK1CDJ
  ----------------------
  https://remoteqth.com

  ___               _        ___ _____ _  _
  | _ \___ _ __  ___| |_ ___ / _ \_   _| || |  __ ___ _ __
  |   / -_) '  \/ _ \  _/ -_) (_) || | | __ |_/ _/ _ \ '  \
  |_|_\___|_|_|_\___/\__\___|\__\_\|_| |_||_(_)__\___/_|_|_|


  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  Based on
  Display test for LaskaKit ESPink-4.2"
  -------- ESPink pinout -------
  MOSI/SDI 23
  CLK/SCK 18
  SS 5 //CS
  DC 17
  RST 16
  BUSY 4
  -------------------------------
  Web:laskakit.cz

  HARDWARE ESP32 Dev Module

*/
#define REV 20230924
#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <SD.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <AsyncElegantOTA.h>
#include <AsyncTCP.h>


// https://rop.nl/truetype2gfx/

#include "Logisoso8pt7b.h"
#include "Logisoso10pt7b.h"
uint16_t colorB = GxEPD_BLACK;
uint16_t colorW = GxEPD_WHITE;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=5*/ SS, /*DC=*/17, /*RST=*/16, /*BUSY=*/4)); // GDEW042T2 400x300, UC8176 (IL0398)
unsigned int eInkRotation = 1; // 1 USB TOP, 3 USB DOWN | 0 default, 1 90°CW, 2 180°CW, 3 90°CCW
#define SD_CS 27
SPIClass spiSD(HSPI); // Use HSPI for SD card
const int SdCardPresentPin = 33;
bool SdCardPresentStatus   = false;

unsigned long previousMillis = 0UL;
unsigned long interval = 0UL;

unsigned long refreshTime = 300000UL;

// Server URL
const char*  server = "www.df2et.de";

// File name to store configuration
const char* configFileName = "/config.txt";
// configuration strings stored in /config.txt
String cfgWifiSSID;
String cfgWifiKey;
String cfgLoc;
String cfgUTCoffset;
String cfgDisplayRot;

AsyncWebServer httpServer(80);

DynamicJsonDocument jsonDoc(1024);
String jsonPayload; // string to store JSON data

String epochToDateTime(long epochTime) {
  // Create a tmElements_t structure to hold the date and time
  tmElements_t tm;

  // Convert epoch time to a tmElements_t structure
  breakTime(epochTime, tm);

  // Format the date and time as a string
  char formattedTime[20];
  sprintf(formattedTime, "%04d-%02d-%02d %02d:%02d:%02d",
          tm.Year + 1970, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);

  // Return the formatted date and time as a String
  return String(formattedTime);
}

void setup() {
  Serial.begin(115200);

  pinMode(SdCardPresentPin, INPUT);
  pinMode(2, OUTPUT);    // Set epaper transistor as output
  digitalWrite(2, HIGH); // turn on epaper transistor
  delay(100);     // Delay so it has time to turn on
  display.init();
  display.setRotation(eInkRotation); // 1 USB TOP, 3 USB DOWN | 0 default, 1 90°CW, 2 180°CW, 3 90°CCW
  display.fillScreen(colorW);
  display.setTextColor(colorB);
  SdCardPresentStatus = !digitalRead(SdCardPresentPin);
  Serial.println("Check microSD ");
  if (SdCardPresentStatus != true) {
    Serial.print("micro SD card is not inserted");
    display.fillScreen(colorW);
    display.setTextColor(colorB);
    display.setFont(&Logisoso10pt7b);
    display.setCursor(30, 120);
    display.println("!");
    display.setCursor(30, 160);
    display.println("micro SD card is not inserted");
    display.setCursor(30, 190);
    display.println("Please insert card with");
    display.setCursor(30, 220);
    display.println("config.txt config file");
    display.display(false);
    while (SdCardPresentStatus != true) {
      delay(1000);
      SdCardPresentStatus = !digitalRead(SdCardPresentPin);
    }
  }
  display.display(false);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("SD card initialization failed");
    
  }
  readConfig();

  display.setFont(&Logisoso10pt7b);
  display.setCursor(70, 150);
  display.println("Connecting...");
  display.setFont(&Logisoso8pt7b);
  display.setCursor(90, 190);
  display.display(false);

  // Connect to Wi-Fi network
  WiFi.begin(cfgWifiSSID.c_str(), cfgWifiKey.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  display.fillScreen(colorW);
  display.setTextColor(colorB);
  display.setFont(&Logisoso10pt7b);
  display.setCursor(70, 150);
  display.println("Connected to:");
  display.setCursor(90, 220);
  display.println("WiFi " + cfgWifiSSID + " " + String(WiFi.RSSI()) + " dBm");
  display.fillCircle(80, 220 - 7, 3, colorW);
  display.setCursor(90, 250);
  display.println(WiFi.localIP());
  Serial.println(WiFi.localIP());
  display.display(false);

  // Initialize and configure NTPClient
  timeClient.begin();
  timeClient.setTimeOffset(0);
  timeClient.update();

  // Wait for the time to be set
  while (!timeClient.isTimeSet()) {
    timeClient.forceUpdate();
    delay(1000);
    Serial.println("Waiting for time synchronization...");
  }

  Serial.println("Time synchronized!");

  // Initialize AsyncElegantOTA
  httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "Satellite status, please got to /update for update..");
  });
  AsyncElegantOTA.begin(&httpServer);
  httpServer.begin();
  Serial.println("HTTP server started");

}

void loop() {

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval)
  {
    // Update time from the NTP server
    timeClient.update();
    Serial.println(timeClient.getFormattedTime());
    // Get current time
    unsigned long current_timestamp = timeClient.getEpochTime();


    // Fetch JSON data from URL
    fetchJsonData();

    // Parse JSON data
    DeserializationError err = deserializeJson(jsonDoc, jsonPayload);
    if (err) {
      Serial.print("JSON parsing error: ");
      Serial.println(err.c_str());
      return;
    }

    // Iterate over satellites in the JSON data
    JsonArray satellites = jsonDoc["data"];

    display.fillScreen(colorW);
    display.setTextColor(colorB);
    display.setCursor(10, 15);
    display.setFont(&Logisoso10pt7b);
    display.print("Satellite status " + String(timeClient.getFormattedTime()) + " UTC ");
    display.setFont(&Logisoso8pt7b);
    display.setCursor(0, 30);

    for (JsonObject sat : satellites) {
      const char* satName = sat["sat"];
      const char* timestampString = sat["timestamp"];

      // Print satellite name
      Serial.print("Satellite: ");
      Serial.print(satName);
      Serial.print(", ");


      display.print("  " + String(satName) + " ");
      // Check if timestamp is null
      if (timestampString == nullptr) {
        Serial.println("Timestamp not available");
        display.println("Not available");
        continue;
      }

      // Convert timestamp to unsigned long
      unsigned long target_timestamp = atol(timestampString);

      // Calculate remaining time

      unsigned long remaining_time = target_timestamp - current_timestamp ;

      // Display remaining time
      if (target_timestamp > current_timestamp) {
        unsigned long days = remaining_time / (24 * 60 * 60);
        unsigned long hours = (remaining_time % (24 * 60 * 60)) / (60 * 60);
        unsigned long minutes = (remaining_time % (60 * 60)) / 60;
        unsigned long seconds = remaining_time % 60;

        Serial.print("Remaining time: ");
        Serial.print(days);
        Serial.print("d ");
        Serial.print(hours);
        Serial.print("h ");
        Serial.print(minutes);
        Serial.print("m  ");
        Serial.print(seconds);
        Serial.println("s");

        display.print(days);
        display.print("d ");
        display.print(hours);
        display.print("h ");
        display.print(minutes);
        display.print("m ");
        display.print(seconds);
        display.println("s ");
        //display.println("     Next:A:08:48 23 L:08:58 156 M:35 D:10  "); //display next orbit when available in API
        display.println("");
      } else {
        Serial.print("timed-out: ");
        Serial.println(epochToDateTime(target_timestamp));
        display.print("timed-out: ");
        display.println(epochToDateTime(target_timestamp));

      }

    }


    display.display(false);
    interval = refreshTime;
    previousMillis = currentMillis;
  }
  AsyncElegantOTA.loop();
}

void fetchJsonData() {
  WiFiClientSecure client; // Changed to WiFiClientSecure
  client.setInsecure();

  Serial.println("\nStarting connection to server...");
  if (!client.connect(server, 443))
    Serial.println("Connection failed!");
  else {
    Serial.println("Connected to server!");
    // Make a HTTP request:
    client.println("GET /tevel/api.php HTTP/1.1");
    client.println("Host: www.df2et.de");
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("Headers received");
        break;
      }
    }
    // if there are incoming bytes available
    // from the server, read them and print them:
    while (client.available()) {
      char c = client.read();
      jsonPayload = jsonPayload + String(c);
    }
    Serial.println("Closing connectio to server!");
    client.stop();
  }
}
void readConfig() {
  // Check if the configuration file exists
  if (SD.exists(configFileName)) {
    Serial.println("Reading config...");
    // Read WiFi configuration from the file
    File configFile = SD.open(configFileName, FILE_READ);
    if (configFile) {
      while (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        int delimiterIndex = line.indexOf('=');
        if (delimiterIndex != -1) {
          String key = line.substring(0, delimiterIndex);
          String value = line.substring(delimiterIndex + 1);
          if (key == "wifi_ssid") {
            cfgWifiSSID = value;
          } else if (key == "wifi_key") {
            cfgWifiKey = value;
          }
        }
      }

      // Close the file
      configFile.close();
    }
  } else Serial.println("Config not found.");
}
