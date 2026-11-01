# nRFBox Carrier Board (encoder build)

A custom carrier PCB for the hand-built nRFBox: an ESP32 DevKit1 (through-hole,
socketed), three nRF24/E01 radios (surface mount), a buck converter and a
charge+boost module (surface mount), a rotary encoder with push switch, one
button, an OLED, and battery power with a hard power switch.

Goal: as small as the parts allow, with room for the three antennas.

## Power architecture

```
            USB-C (charge)
                |
   LiPo <--> [ Charger + Boost module ] --> 5V out --.
                                                      |
   External 5V pads ----------------------------------+--> 5V RAIL
                                                      |
                              +-----------------------+----------------+
                              |                       |                |
                         DevKit VIN (5V)        [ Buck 5V->3.3V ]   NeoPixel 5V
                              |                       |
                       DevKit 3V3 reg           3V3_RADIO RAIL
                              |                       |
                         OLED, logic          3x E01 radio VCC
                                               (10uF + 100nF each)

   GND: common plane. The main battery/charger return runs through the
   SWITCH (two split GND pads) so the whole board powers off.
```

Key points:
- The charger module charges the LiPo over USB-C and **boosts to a steady 5V**,
  so the 5V rail is live on battery or USB. (Confirmed from the module: USB-C,
  "5V" silkscreen, ETA PMIC.)
- The three E01 radios run off a **dedicated 3.3V rail from the buck**, separate
  from the DevKit's own 3V3 regulator. This is the brownout fix: the PA/LNA E01s
  are current-hungry and need a clean, independent supply with local decoupling.
- The OLED and the encoder/button pull-ups reference the DevKit's 3V3 pin (low
  current), not the radio rail.

## Netlist (ESP32 DevKit1 -> peripherals)

Pin map is the firmware's current assignment (config.h), unchanged.

| Net        | ESP32 GPIO | Goes to                                   |
|------------|-----------|--------------------------------------------|
| SPI SCK    | 18        | Radio A/B/C SCK (shared)                   |
| SPI MISO   | 19        | Radio A/B/C MISO (shared)                  |
| SPI MOSI   | 23        | Radio A/B/C MOSI (shared)                  |
| Radio A CE | 5         | Radio A CE                                 |
| Radio A CSN| 17        | Radio A CSN                                |
| Radio B CE | 16        | Radio B CE                                 |
| Radio B CSN| 4         | Radio B CSN                                |
| Radio C CE | 15        | Radio C CE                                 |
| Radio C CSN| 2         | Radio C CSN (also a strapping pin)         |
| OLED SDA   | 21        | SSD1306 SDA                                |
| OLED SCL   | 22        | SSD1306 SCL                                |
| NeoPixel   | 14        | WS2812 DIN                                 |
| Encoder A  | 25        | EC11 phase A                               |
| Encoder B  | 26        | EC11 phase B                               |
| Encoder SW | 27        | EC11 push switch (other leg to GND)        |
| Button     | 32        | Tactile button (other leg to GND)          |
| SD CS      | 13        | microSD CS (footprint only, not populated) |

- All radio IRQ pins: no connect.
- Encoder common (C) and button common: GND.
- Radio VCC: 3V3_RADIO rail. OLED VCC: DevKit 3V3. Radio/OLED GND: common.

## Components and footprints

| Ref | Part | Mount | Footprint status |
|-----|------|-------|------------------|
| U1  | ESP32 DevKit (30-pin DOIT v1, WROOM, micro-USB) | Through-hole, socketed on female headers | 51.53 mm long; NEED width + header row spacing |
| U2-U4 | nRF24 / E01 x3 | Surface mount (edge/castellated pads) | NEED module dims + pad map + antenna type |
| U5  | Buck 5V->3.3V | Surface mount | 11.75 x 17.5 mm; NEED IN/OUT pad positions |
| U6  | Charger + boost | Surface mount | ~26.3 mm one side; NEED 2nd dim + pad map (B+/B-/5V/GND) |
| SW1 | EC11 rotary encoder + switch | Through-hole | Standard EC11 footprint |
| SW2 | Tactile button | TH or SMD | Standard |
| SW3 | Power switch | via split GND pads | 2 pads in battery return |
| J1  | 5V input pads | 2 pads | 5V + GND |
| J2  | Battery (LiPo) | 2 pads | to charger B+/B- |
| C*  | Decoupling | 0805 | 10uF + 100nF per radio, bulk on 5V and 3V3 |
| DISP| SSD1306 OLED 0.96" | TBD | NEED: on carrier or off-board? |

## Open items (needed before layout)

1. CAD/handoff format: KiCad source files, EasyEDA (matches the original
   project's gerber/CPL flow), or a spec + netlist you lay out yourself?
2. Charger module: second dimension + pad/pin locations (B+, B-, 5V out, GND).
3. Buck module: IN+/IN-/OUT+/OUT- pad locations on the 11.75 x 17.5 outline.
4. E01 radio: exact module dims, pad pitch, and antenna type (PCB trace / U.FL /
   SMA). Antennas to the case per your note, so confirm the lead/connector.
5. DevKit: confirmed 30-pin, 51.53 mm long. NEED board width + header
   row-to-row spacing (usually 0.9"/22.86 mm, but the wide variant differs),
   and confirm socketed (removable) vs soldered direct.
6. OLED: mounted on this carrier (front face) or on its own wires?
7. Board outline: target size/shape, and how the antennas attach to the case.
8. Power switch: slide switch on the board, or just pads for an off-board switch?
