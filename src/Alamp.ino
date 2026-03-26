/*Pikud Haoref Alarm lamp
Created by Ofek Golan for Amit education 
USE AT YOUR OWN RISK!

Modified to include WiFiManager, Preferences, mDNS, 15-Min Timer, and Stability Fix
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>    // https://github.com/tzapu/WiFiManager
#include <Preferences.h>    // Built-in ESP32 library for flash storage
#include <ESPmDNS.h>        // Built-in library for .local domain support

// ==========================================
// USER CONFIGURATION (Dynamic)
// ==========================================

// Flash storage object
Preferences preferences;

// Global variables for our custom cities string
char custom_cities_input[255] = ""; // Default value
String TARGET_CITIES[10];           // Support up to 10 cities
int NUM_TARGET_CITIES = 0;

// Flag for saving data from WiFiManager
bool shouldSaveConfig = false;

// Callback notifying us of the need to save config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Function to split the comma-separated string into our array
void parseCities(String input) {
  NUM_TARGET_CITIES = 0;
  int start = 0;
  int end = input.indexOf(',');
  
  // If input is empty, leave one empty string to alert for ALL cities
  if (input.length() == 0) {
    TARGET_CITIES[0] = "";
    NUM_TARGET_CITIES = 1;
    return;
  }

  while (end != -1 && NUM_TARGET_CITIES < 10) {
    String city = input.substring(start, end);
    city.trim(); // Remove leading/trailing spaces
    TARGET_CITIES[NUM_TARGET_CITIES++] = city;
    start = end + 1;
    end = input.indexOf(',', start);
  }
  if (start < input.length() && NUM_TARGET_CITIES < 10) {
    String city = input.substring(start);
    city.trim();
    if (city.length() > 0) {
      TARGET_CITIES[NUM_TARGET_CITIES++] = city;
    }
  }
}

// ==========================================

// Pikud HaOref (Home Front Command) API
const char* OREF_URL = "https://www.oref.org.il/WarningMessages/alert/alerts.json";

//internal_LED
const int ALERT_PIN         = 2;
//LED STRIP
#define PIN 4         // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS 11  // Popular NeoPixel ring size
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastCheckTime = 0;
const unsigned long POLLING_INTERVAL = 1000;  // Check every 1 seconds

// State tracker and 15-Minute Timer
bool alertActive = false; 
unsigned long alertStartTime = 0;
const unsigned long ALERT_TIMEOUT = 900000; // 15 minutes in milliseconds

void setup() {
  Serial.begin(115200);
  
  pixels.begin();
  pixels.clear();
  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);

 // Set LEDs to BLUE to indicate it is in Setup/Config mode
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 150));
    pixels.show();
    delay(10);
  }

  // --- FLASH STORAGE SETUP ---
  preferences.begin("oref-app", false);
  
  // Load saved cities from flash, or use default if none exist
  String savedCities = preferences.getString("cities", custom_cities_input);
  savedCities.toCharArray(custom_cities_input, 255);
  parseCities(savedCities);
  
  Serial.println("Loaded Cities: " + savedCities);

  // --- WIFIMANAGER SETUP (Dynamic Allocation to prevent crashing) ---
  WiFiManager* wm = new WiFiManager();
  
  // wm->resetSettings(); // UNCOMMENT THIS LINE TO WIPE SAVED WIFI AND TEST THE PORTAL

  wm->setSaveConfigCallback(saveConfigCallback);

  // Add the custom parameter for the cities
  WiFiManagerParameter* custom_cities_param = new WiFiManagerParameter("cities", "Target Cities (comma separated)", custom_cities_input, 255);
  wm->addParameter(custom_cities_param);

  Serial.println("Connecting to WiFi or opening Setup Portal...");
  
  // Connect or open portal "Alamp_AP"
  if (!wm->autoConnect("Alamp_AP")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    ESP.restart(); // Reset and try again
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // --- mDNS SETUP ---
  if (!MDNS.begin("alamp")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started. Address: http://alamp.local");
  }

  // Save the custom parameters to flash if they were changed in the portal
  if (shouldSaveConfig) {
    String newCities = custom_cities_param->getValue();
    preferences.putString("cities", newCities); // Save to flash
    parseCities(newCities);                     // Update our array
    Serial.println("Saved new cities to flash: " + newCities);
  }

  // Start with Green
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 150, 0));
    pixels.show();
    delay(10);
  }
}

void loop() {
  // 15-Minute Timer Logic
  if (alertActive && (millis() - alertStartTime >= ALERT_TIMEOUT)) {
    alertActive = false;
    Serial.println("Notice: 15-minute timer completed. Returning to Green.");
    digitalWrite(ALERT_PIN, LOW);
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 255, 0));
      pixels.show();
      delay(10);
    }
  }

  // Non-blocking delay for HTTP polling
  if (millis() - lastCheckTime >= POLLING_INTERVAL) {
    lastCheckTime = millis();
    checkAlerts();
  }
  delay(10);
}

void checkAlerts() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(OREF_URL);

    // Expanded User-Agent to look more like a real desktop browser to bypass Akamai
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    http.addHeader("Referer", "https://www.oref.org.il/");
    http.addHeader("X-Requested-With", "XMLHttpRequest");
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();

      // CRITICAL FIX: Strip UTF-8 BOM (Byte Order Mark) if Oref API includes it
      if (payload.startsWith("\xEF\xBB\xBF")) {
        payload.remove(0, 3);
      }
      payload.trim();  // Remove whitespace/newlines

      // When there are no alerts anywhere, Oref returns an empty string
      if (payload.length() <= 2) {
        
        // If we are actively in an alert, HOLD the state and let the 15-min timer run.
        if (alertActive) {
          Serial.println("Checked API: Empty payload. 15-minute timer is running...");
        } else {
          Serial.println("Checked API: All clear.");
          digitalWrite(ALERT_PIN, LOW);
          //Green -----------------------------------------------------------------------------------
          for (int i = 0; i < NUMPIXELS; i++) {
            pixels.setPixelColor(i, pixels.Color(0, 255, 0));
            pixels.show();
            delay(10);
          }
        }
        
      } else {

        // Print payload to monitor to verify we aren't getting HTML blocked pages
        Serial.println("--- INCOMING DATA ---");
        Serial.println(payload);

        // Check if payload looks like HTML (Akamai block) instead of JSON
        if (payload.indexOf("<html") >= 0 || payload.indexOf("<HTML") >= 0) {
          Serial.println("Error: Received HTML instead of JSON. Blocked by Akamai.");
          http.end();
          return;  // Exit the function early
        }

        // We have active alerts, parse the JSON
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          bool isRelevantAlert = false;
          String alertCategory = "";
          
          String category = doc["cat"].as<String>();
          JsonArray cities = doc["data"].as<JsonArray>();

          // Check if the current alert matches ANY of our configured cities
          for (JsonVariant city : cities) {
            String currentCity = city.as<String>();
            
            for (int i = 0; i < NUM_TARGET_CITIES; i++) {
              if (TARGET_CITIES[i] == "" || currentCity == TARGET_CITIES[i]) {
                isRelevantAlert = true;
                alertCategory = category;
                break;
              }
            }
            if (isRelevantAlert) break; // Break outer loop if we found a match
          }

          if (isRelevantAlert) {
            // We have a live alert for our city.
            if (!alertActive) {
              alertActive = true; 
              alertStartTime = millis(); // START THE 15-MINUTE TIMER
            }
            
            // Determine alert type based on category
            if (alertCategory == "1" || alertCategory == "2" || alertCategory == "6") {
              //Red -----------------------------------------------------------------------------------
              if (alertCategory == "1"){
                for (int i = 0; i < NUMPIXELS; i++) {
                  pixels.setPixelColor(i, pixels.Color(255, 0, 0));
                  pixels.show();
                  delay(10);
                }
                digitalWrite(ALERT_PIN, HIGH);
                Serial.println("RED ALERT! Missile!");
              } else {
                // PURPLE ---------------------------------------------
                for (int i = 0; i < NUMPIXELS; i++) {
                  pixels.setPixelColor(i, pixels.Color(255, 0, 200));
                  pixels.show();
                  delay(10);
                }
                digitalWrite(ALERT_PIN, HIGH);
                Serial.println("RED ALERT! UAV!");
              }

            } else if (alertCategory == "10") {
              //Yellow -----------------------------------------------------------------------------------
              for (int i = 0; i < NUMPIXELS; i++) {
                pixels.setPixelColor(i, pixels.Color(255, 100, 0));
                pixels.show();
                delay(10);
              }
              digitalWrite(ALERT_PIN, HIGH);
              Serial.println("YELLOW ALERT! Pre-Alert active.");
            } 
          } 
          else {
            // There is an alert in Israel, but NOT for our specific cities.
            if (!alertActive) {
              // Only wipe green if we aren't currently holding a Red alert state
              //Green -----------------------------------------------------------------------------------
              for (int i = 0; i < NUMPIXELS; i++) {
                pixels.setPixelColor(i, pixels.Color(0, 255, 0));
                pixels.show();
                delay(10);
              }
              digitalWrite(ALERT_PIN, LOW);
            } else {
              Serial.println("Other cities alerting. 15-minute timer is running...");
            }
          }
        } else {
          Serial.print("JSON Parse Failed: ");
          //5 Red json error
          pixels.clear();
          delay(50);
          for (int i = 0; i < 5; i++) {
          pixels.setPixelColor(i, pixels.Color(255, 0, 0));
          pixels.show();
          delay(10);
          }
          Serial.println(error.c_str());
        }
      }
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
          //3 Red Http error
          pixels.clear();
          delay(50);
          for (int i = 0; i < 3; i++) {
          pixels.setPixelColor(i, pixels.Color(255, 0, 0));
          pixels.show();
          delay(10);
          }
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected!");
          //1 Red Wifi error
          pixels.clear();
          delay(50);
          for (int i = 0; i < 1; i++) {
          pixels.setPixelColor(i, pixels.Color(255, 0, 0));
          pixels.show();
          delay(10);
          }
  }
}
