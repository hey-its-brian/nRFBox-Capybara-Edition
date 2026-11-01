/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#ifndef CONFIG_H
#define CONFIG_H

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Input: rotary encoder (with push switch) + ONE button (replaces the 5 buttons).
// The EC11's push switch (terminals D/E) is wired to ENC_SW_PIN and GND.
// Physical pins (reuse the GPIOs the old buttons freed up):
#define ENC_A_PIN    25   // rotary encoder channel A
#define ENC_B_PIN    26   // rotary encoder channel B
#define ENC_SW_PIN   27   // encoder push switch (EC11 D/E) -> SELECT
#define BTN_BACK_PIN 32   // single button: tap = back (LEFT), hold = RIGHT
// Radio C's CSN is on GPIO13 (clear of the encoder cluster); GPIO33 is free. SD is parked (SD_CS placeholder on GPIO2).

// Logical (virtual) button IDs. The firmware still asks for these five, but they are
// now synthesized from the encoder + button by rdBtn(), not read from real GPIOs:
//   rotate CCW -> UP     rotate CW -> DOWN
//   encoder short press -> SELECT (enter) in the menu / RIGHT (activate) in an app
//   encoder long press  -> exit the app     button tap -> LEFT     button hold -> RIGHT
// Values are sentinels (>GPIO range) so a stray read can't hit a real pin.
#define BUTTON_UP_PIN       201
#define BUTTON_DOWN_PIN     202
#define BUTTON_SELECT_PIN   203
#define BTN_PIN_LEFT        204
#define BTN_PIN_RIGHT       205

// Input shim (defined in nRFBox.ino): rdBtn() returns LOW when the given logical
// button is active and HIGH otherwise, matching the old active-low digitalRead().
int  rdBtn(int id);
void inputSetup();

// SD Card Slot-specific Pins
#define SD_CS_PIN 2    // SD is parked and unwired; moved off 13 (now Radio C CSN). GPIO2 is just a placeholder here until SD is revived.
#define FIRMWARE_FILE "/firmware.bin"

// nRF24-specific Pins
#define NRF_CE_PIN_A    5   
#define NRF_CSN_PIN_A   17 
#define NRF_CE_PIN_B    16  
#define NRF_CSN_PIN_B   4  
#define NRF_CE_PIN_C    15
#define NRF_CSN_PIN_C   13   // moved off GPIO2 (LED-loaded strapping pin) and GPIO33 (sits next to the encoder); 13 is clear of the encoder cluster

// Common dependencies
#include "setting.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <Preferences.h>
#include <vector>
#include <string>
#include <SD.h>
#include <Update.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

Adafruit_NeoPixel pixels(1, 14, NEO_GRB + NEO_KHZ800);

// BLE-specific dependencies
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// nRF24-specific dependencies
#include <nRF24L01.h>
#include <RF24.h>

// WiFi-specific dependencies
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <string>

// ESP-specific configurations
#include <esp_bt.h>
#include <esp_wifi.h>

// External declarations
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern Adafruit_NeoPixel pixels;

// BLE-related namespaces
namespace BleJammer {
  void blejammerSetup();
  void blejammerLoop();
}

namespace BleScan {
  void blescanSetup();
  void blescanLoop();
}

namespace SourApple {
  void sourappleSetup();
  void sourappleLoop();
}

namespace Spoofer {
  void spooferSetup();
  void spooferLoop();
}

// nRF24-related namespaces
namespace Analyzer {
  void analyzerSetup();
  void analyzerLoop();
}

namespace ProtoKill {
  void blackoutSetup();
  void blackoutLoop();
}

namespace Scanner {
  void scannerSetup();
  void scannerLoop();
}

namespace Jammer {
  void jammerSetup();
  void jammerLoop();
}

// WiFi-related namespaces
namespace WifiScan {
  void wifiscanSetup();
  void wifiscanLoop();
}

namespace Deauther {
  void deautherSetup();
  void deautherLoop();
}

#endif // CONFIG_H
