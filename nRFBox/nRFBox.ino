/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include "icon.h"
#include "setting.h"
#include "config.h"

extern uint8_t oledBrightness;

const int NUM_ITEMS = 12;
const int MAX_ITEM_LENGTH = 20;

const unsigned char* bitmap_icons[NUM_ITEMS] = {
  bitmap_icon_scanner, bitmap_icon_analyzer, bitmap_icon_jammer, bitmap_icon_kill,
  bitmap_icon_ble_jammer, bitmap_icon_spoofer, bitmap_icon_apple, bitmap_icon_ble,
  bitmap_icon_wifi, bitmap_icon_wifi_jammer, bitmap_icon_about, 
  bitmap_icon_setting
};

char menu_items[NUM_ITEMS][MAX_ITEM_LENGTH] = {  
  "Scanner", "Analyzer", "WLAN Jammer", "Proto Kill", "BLE Jammer",
  "BLE Spoofer", "Sour Apple", "BLE Scan", "WiFi Scan", 
  "Deauther", "About", "Setting"
};

void (*menu_functions[NUM_ITEMS])() = {
  Scanner::scannerSetup, Analyzer::analyzerSetup, Jammer::jammerSetup,
  ProtoKill::blackoutSetup, BleJammer::blejammerSetup, Spoofer::spooferSetup,
  SourApple::sourappleSetup, BleScan::blescanSetup, WifiScan::wifiscanSetup, Deauther::deautherSetup,
  utils, Setting::settingSetup
};

void (*menu_loop_functions[NUM_ITEMS])() = {
  Scanner::scannerLoop, Analyzer::analyzerLoop, Jammer::jammerLoop,
  ProtoKill::blackoutLoop, BleJammer::blejammerLoop, Spoofer::spooferLoop,
  SourApple::sourappleLoop, BleScan::blescanLoop, WifiScan::wifiscanLoop, Deauther::deautherLoop,
  nullptr, Setting::settingLoop
};

int item_selected = 0;
int current_screen = 0;
unsigned long last_button_time = 0;
const unsigned long DEBOUNCE_DELAY = 150; 
const unsigned long POST_PRESS_DELAY = 200;

// ---------------------------------------------------------------------------
// Rotary encoder + single button input
//
// The original firmware polled five push buttons with digitalRead(). Those
// reads were globally redirected to rdBtn(), which synthesizes the five logical
// buttons from one quadrature encoder (with push switch) and one button:
//   rotate CCW -> UP        rotate CW   -> DOWN     encoder press -> SELECT
//   button tap -> LEFT (back)                       button hold   -> RIGHT
// Rotation and the tap are one-shot (consumed on read); the encoder switch
// reports real held state and RIGHT latches while the button is held, so the
// WiFi tools' "still held?" re-checks and "wait for release" loops behave.
// ---------------------------------------------------------------------------

// Real GPIO read. The global digitalRead()->rdBtn() redirect intentionally does
// not touch this wrapper, so the shim and ISR can still read the physical pins.
// always_inline keeps it inside the IRAM ISR that calls it.
static inline int rdBtnRaw(int pin) __attribute__((always_inline));
static inline int rdBtnRaw(int pin) { return digitalRead(pin); }

// Quadrature decode: index by (prev<<2 | curr) of the A/B Gray code. Nonzero
// entries are valid transitions (+1 one way, -1 the other); 4 of them make one
// detent on a typical KY-040. If direction feels reversed, swap ENC_A/ENC_B.
static const int8_t ENC_TABLE[16] = {
  0, -1,  1,  0,
  1,  0,  0, -1,
 -1,  0,  0,  1,
  0,  1, -1,  0
};
volatile uint8_t encState = 0;       // rolling 4-bit A/B history
volatile int8_t  encAccum = 0;       // sub-detent accumulator
volatile int8_t  encoderSteps = 0;   // whole detents pending: + = CW/DOWN, - = CCW/UP

void IRAM_ATTR encoderISR() {
  encState = ((encState << 2) | (rdBtnRaw(ENC_A_PIN) << 1) | rdBtnRaw(ENC_B_PIN)) & 0x0F;
  encAccum += ENC_TABLE[encState];
  if (encAccum >= 4)       { encoderSteps++; encAccum = 0; }
  else if (encAccum <= -4) { encoderSteps--; encAccum = 0; }
}

// Input decoder for the rotary encoder (with push switch) + one button.
//   Rotate              -> UP / DOWN (navigate)
//   Encoder SHORT press -> primary action: SELECT (enter) in the icon menu,
//                          RIGHT (activate the highlighted item) inside an app
//   Encoder LONG press  -> back / exit the app (held as a level, so ism's
//                          in-loop bail-out and the outer loop both see it)
//   Button tap          -> LEFT (back / option-left, one-shot)
//   Button hold         -> RIGHT (latched; used for flows like WiFi scan-start)
// The press means "enter/activate" while navigating and "exit" only on a long
// hold; context comes from current_screen (0 = icon menu, 1 = inside an app).
const unsigned long BTN_LONG_MS     = 500; // button hold threshold -> RIGHT
const unsigned long ENC_LONG_MS     = 500; // encoder hold threshold -> exit
const unsigned long BTN_DEB_MS      = 25;  // debounce interval
const unsigned long ENC_TAP_PULSE   = 250; // a tap reads as a RIGHT press held this long
const unsigned long ENC_TAP_MAX_AGE = 300; // drop a tap after this (must exceed the pulse)

// Button (BTN_BACK_PIN) state
bool          btnDown      = false;
unsigned long btnDownAt    = 0;
bool          btnLongFired = false;      // true while a button hold is latched as RIGHT
bool          leftPending  = false;      // one-shot button tap -> LEFT

// Encoder push switch (ENC_SW_PIN), edge-timestamped in an ISR so a quick tap is
// never missed. Polling inside a slow app loop can skip a short press; an
// interrupt catches both edges the instant they happen (same reason rotation
// uses an ISR). Long-press (exit) is derived live from the held state.
volatile bool          encSwDown     = false;  // currently held (ISR)
volatile unsigned long encSwDownAt   = 0;      // press timestamp (ISR)
volatile unsigned long encSwEdgeAt   = 0;      // last accepted edge (ISR debounce)
volatile bool          encTapPending = false;  // short tap completed (ISR)
volatile unsigned long encTapAt      = 0;

void IRAM_ATTR encSwISR() {
  unsigned long now = millis();
  if (now - encSwEdgeAt < 15) return;          // debounce bounce
  encSwEdgeAt = now;
  bool down = (rdBtnRaw(ENC_SW_PIN) == LOW);
  if (down && !encSwDown) {
    encSwDown = true; encSwDownAt = now;
  } else if (!down && encSwDown) {
    encSwDown = false;
    if ((now - encSwDownAt) < ENC_LONG_MS) { encTapPending = true; encTapAt = now; }  // short -> tap
  }
}

void updateButton() {
  unsigned long now = millis();

  // --- single button: tap -> LEFT, hold -> RIGHT ---
  static bool    dbLevel = false, dbRaw = false;
  static unsigned long dbAt = 0;
  bool raw = (rdBtnRaw(BTN_BACK_PIN) == LOW);
  if (raw != dbRaw) { dbRaw = raw; dbAt = now; }
  if ((now - dbAt) >= BTN_DEB_MS) dbLevel = raw;
  bool down = dbLevel;
  if (down && !btnDown) {
    btnDown = true; btnDownAt = now; btnLongFired = false;
  } else if (down && btnDown) {
    if (!btnLongFired && (now - btnDownAt) >= BTN_LONG_MS) btnLongFired = true;
  } else if (!down && btnDown) {
    btnDown = false;
    if (!btnLongFired) leftPending = true;
  }

  // The encoder switch itself is handled in encSwISR(); here we only age an
  // unconsumed tap so a stale press can't fire an action later.
  if (encTapPending && (now - encTapAt) >= ENC_TAP_MAX_AGE) encTapPending = false;
}

int rdBtn(int id) {
  updateButton();
  switch (id) {
    case BUTTON_UP_PIN: {
      bool hit = false;
      noInterrupts();
      if (encoderSteps < 0) { encoderSteps++; hit = true; }
      interrupts();
      return hit ? LOW : HIGH;
    }
    case BUTTON_DOWN_PIN: {
      bool hit = false;
      noInterrupts();
      if (encoderSteps > 0) { encoderSteps--; hit = true; }
      interrupts();
      return hit ? LOW : HIGH;
    }
    case BUTTON_SELECT_PIN:
      // Icon menu: short press enters the app. Inside an app: a long hold exits,
      // derived live from the held state so ism's bail-out and the outer loop
      // both see it while the switch is down past the threshold.
      if (current_screen == 0) {
        if (encTapPending) { encTapPending = false; return LOW; }
        return HIGH;
      }
      return (encSwDown && (millis() - encSwDownAt) >= ENC_LONG_MS) ? LOW : HIGH;
    case BTN_PIN_RIGHT:
      // Inside an app, a short encoder press acts as a brief RIGHT press-and-hold
      // (a timed pulse, not a one-shot) so apps that re-check "still held?" and
      // wait for release -- e.g. WiFi scan-start -- see a real press. The button
      // hold is also RIGHT (latched).
      if (current_screen == 1 && encTapPending && (millis() - encTapAt) < ENC_TAP_PULSE) return LOW;
      return btnLongFired ? LOW : HIGH;
    case BTN_PIN_LEFT:
      if (leftPending) { leftPending = false; return LOW; }     // one-shot button tap
      return HIGH;
    default:
      return rdBtnRaw(id);   // safety: treat as a real pin (should not happen)
  }
}

void inputSetup() {
  pinMode(ENC_A_PIN, INPUT_PULLUP);
  pinMode(ENC_B_PIN, INPUT_PULLUP);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);
  pinMode(BTN_BACK_PIN, INPUT_PULLUP);
  encState = (rdBtnRaw(ENC_A_PIN) << 1) | rdBtnRaw(ENC_B_PIN);
  attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_SW_PIN), encSwISR, CHANGE);
}

void drawMenu() {
  u8g2.clearBuffer();
  if (current_screen != 0) return;

  u8g2.setFont(u8g2_font_5x7_tf); 
  u8g2.drawBox(0, 0, 128, 8); 
  u8g2.setDrawColor(0); 
  char versionStr[16];
  for (size_t i = 0; i < sizeof(txt_v); i++) {
    versionStr[i] = (char)txt_v[i];
  }
  versionStr[sizeof(txt_v)] = '\0';

  u8g2.setDrawColor(0);

  Str(2, 7, txt_n, sizeof(txt_n));
  int version_width = u8g2.getUTF8Width(versionStr);
  Str(128 - version_width - 2, 7, txt_v, sizeof(txt_v));
  u8g2.setDrawColor(1); 
  u8g2.drawHLine(0, 8, 128); 

  const int icons_per_row = 3;
  const int icons_per_col = 2;
  const int max_display_items = icons_per_row * icons_per_col; 

  int selected_col = item_selected % icons_per_row;
  int selected_row = (item_selected / icons_per_row) % icons_per_col;
  if (item_selected == 12) {
    selected_row = 1; 
  }
  int highlight_x = 13 + selected_col * 40;
  int highlight_y = 14 + selected_row * 24;

  int start_row = (item_selected / icons_per_row) - selected_row;
  if (start_row < 0) start_row = 0;
  int total_rows = (NUM_ITEMS + icons_per_row - 1) / icons_per_row;
  if (start_row > total_rows - icons_per_col) start_row = total_rows - icons_per_col;
  if (start_row < 0) start_row = 0; 
  int start_item = start_row * icons_per_row;
  int end_item = min(NUM_ITEMS, start_item + max_display_items);

  for (int i = start_item; i < end_item; i++) {
    int idx = i - start_item; 
    int row = idx / icons_per_row;
    int col = idx % icons_per_row;
    int x_pos = 13 + col * 40;
    int y_pos = 14 + row * 24;
    u8g2.drawXBMP(x_pos, y_pos, 16, 16, bitmap_icons[i]);
  }

  u8g2.drawRFrame(highlight_x - 3, highlight_y - 3, 22, 22, 3); 
  u8g2.setDrawColor(0);
  u8g2.drawRFrame(highlight_x - 2, highlight_y - 2, 22, 22, 3); 
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_5x8_tf); 
  int name_width = u8g2.getUTF8Width(menu_items[item_selected]);
  int name_x = (128 - name_width) / 2;
  u8g2.drawStr(name_x, 64, menu_items[item_selected]); 

  u8g2.drawFrame(124, 18, 4, 38); 
  int bar_height = 38 / total_rows;
  u8g2.drawBox(124, 18 + (bar_height * start_row), 4, bar_height); 

  if (start_row > 0) {
    u8g2.drawStr(124, 15, "."); 
  }
  if (start_row < total_rows - 1) {
    u8g2.drawStr(124, 64, "."); 
  }
  u8g2.sendBuffer();

  setRadiosNeutralState();
}

bool readButton(int pin) {
  if (rdBtn(pin) == LOW && (millis() - last_button_time > DEBOUNCE_DELAY)) {
    last_button_time = millis();
    delay(POST_PRESS_DELAY); 
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  neopixelSetup();
  initAllRadios();
  EEPROM.begin(512);
  oledBrightness = EEPROM.read(1);
  if (oledBrightness < 10) oledBrightness = 180;  // guard: avoid invisible OLED from stale/zero NVS contrast
  u8g2.begin();
  u8g2.setContrast(oledBrightness);
  conf();
  inputSetup();
  drawMenu();
}

void loop() {
  if (current_screen == 0) {
    const int icons_per_row = 3;

    if (readButton(BTN_PIN_LEFT)) {
      item_selected = max(0, item_selected - 1);
      drawMenu();
    }
    if (readButton(BTN_PIN_RIGHT)) {
      item_selected = min(NUM_ITEMS - 1, item_selected + 1);
      drawMenu();
    }
    if (readButton(BUTTON_UP_PIN)) {
      item_selected = max(0, item_selected - 1);          // encoder step: one icon back
      drawMenu();
    }
    if (readButton(BUTTON_DOWN_PIN)) {
      item_selected = min(NUM_ITEMS - 1, item_selected + 1);  // encoder step: one icon forward
      drawMenu();
    }
    if (readButton(BUTTON_SELECT_PIN)) {
      current_screen = 1;
      for (int cycle = 0; cycle < 2; cycle++) { 
        for (int i = 0; i < 3; i++) {
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_6x10_tr);
          u8g2.drawStr(30, 32, "Loading");
    
          String dots = "";
          for (int j = 0; j <= i; j++) {
            dots += ".";
            setNeoPixelColour("white");
          }
          setNeoPixelColour("0");
          
          u8g2.drawStr(73, 32, dots.c_str()); 
    
          u8g2.sendBuffer();
          delay(200); 
        }
      }
      menu_functions[item_selected]();
      
      while (current_screen == 1) {
        if (menu_loop_functions[item_selected]) {
          menu_loop_functions[item_selected]();
        }
        if (readButton(BUTTON_SELECT_PIN)) {   // encoder long-press -> exit
          current_screen = 0;
          break;
        }
        // Button = back: if the app didn't consume the tap (it's at its top level
        // or doesn't use LEFT), bubble it up and exit to the icon menu.
        if (leftPending) {
          leftPending = false;
          current_screen = 0;
          break;
        }
      }
      // Returning to the menu: flush any input the app left pending so the cursor
      // doesn't jump (e.g. encoder steps racked up while changing a jammer mode).
      noInterrupts(); encoderSteps = 0; interrupts();
      encTapPending = false; leftPending = false; btnLongFired = false;
      drawMenu();
    }
  }
}
