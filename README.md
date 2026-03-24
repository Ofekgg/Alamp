# Alamp
# Red Alert Visual Indicator (מנורת התרעה - פיקוד העורף)

An ESP32-based visual alert system designed for the deaf and hard of hearing. This device connects to the Home Front Command (Pikud HaOref) API and provides real-time visual notifications through a color-coded LED system.

מערכת התרעה ויזואלית מצילת חיים לחירשים וכבדי שמיעה, המבוססת על בקר ESP32. המנורה מתחברת בזמן אמת לשרתים של פיקוד העורף ומתרגמת את סוג האיום לצבעים שונים.

## 🚦 Alert Logic (צבעי התרעה)
The lamp changes colors based on the type of alert received:
* 🟢 **Green (ירוק):** Routine / All clear (שגרה).
* 🟡 **Yellow (צהוב):** Early warning (התרעה מקדימה).
* 🔴 **Red (אדום):** Missile / Rocket alert (ירי טילים).
* 🟣 **Purple (סגול):** Drone intrusion (חדירת כטב"ם).

## 🛠 Hardware Requirements
* **Microcontroller:** ESP32
* **LEDs:** WS2812B NeoPixels
* **Pinout:** GPIO 4 (Data), Vin (5V), GND (Ground)

## 💻 Software & Libraries
The following libraries are required to compile the project. You can install them via the Arduino Library Manager:

* **WiFi.h** & **HTTPClient.h**: For network connectivity and API requests.
* **WiFiManager.h**: For easy captive portal Wi-Fi configuration (no hardcoded passwords).
* **Preferences.h**: To save user settings (like alert zones) to the ESP32 NVS memory.
* **ArduinoJson.h**: To parse the JSON response from the Home Front Command servers.
* **Adafruit_NeoPixel.h**: To control the visual LED states and colors.

## 🚀 Setup
1. **Hardware:** Connect the LED strip as shown in the wiring diagram (Red to Vin, Black to GND, Green to GPIO 4).
2. **Configuration:** On first boot, connect to the "Alamp_AP" Wi-Fi network to configure your home internet settings and locations setup.

## ⚖️ License
This project is open-source. Feel free to use and modify it to help keep people safe.
