# nRFBox Carrier PCB — Build Brief

Hand this whole document to a fresh Claude chat, along with the four photos
listed in section 10. It contains everything needed to design the board. Where a
value still needs confirming, a recommended default is given so you can proceed.

---

## 1. What this is

A custom 2-layer carrier PCB for a handheld "nRFBox" (a multi-radio 2.4 GHz
tool). It hosts an ESP32 DevKit (socketed), three nRF24L01/E01 radio modules, a
buck regulator, a LiPo charge+boost module, a rotary encoder with push switch,
one button, and a small OLED. It runs on a LiPo with USB-C charging and a hard
power switch.

**Primary goal: make the board as small as the parts allow**, while leaving room
to mount three antennas (the antennas themselves mount to the enclosure, not the
PCB, so the board needs pads/leads for them, not full antenna footprints).

This board is a power-and-interconnect carrier: it does not re-implement the
ESP32 or the radios, it holds the existing modules and wires them together with
a clean, dedicated power supply for the radios.

## 2. Deliverable

Produce a complete **KiCad 8 project**:
- Schematic (`.kicad_sch`) with the full netlist in section 5.
- PCB layout (`.kicad_pcb`), 2 layers, routed, DRC clean.
- Any custom footprints (`.kicad_mod`) for the modules in section 4.
- Fabrication outputs for **JLCPCB**: gerbers (zip), drill, BOM (csv), and CPL/
  placement (csv) for the surface-mount parts.
- A short README with assembly notes and any assumptions you made.

Design rules: JLCPCB 2-layer defaults are fine (6 mil track/space, 0.3 mm vias,
0.2 mm min drill). Use 1 oz copper. Add mounting holes (M2 or M2.5) in the
corners if the outline allows.

## 3. Power architecture

```
            USB-C (charge in)
                |
   LiPo <--> [ Charger + Boost module ] --> 5V out --.
                                                      |
   External 5V input pads ----------------------------+--> 5V RAIL
                                                      |
                 +------------------------+-----------+-------------+
                 |                        |                         |
            DevKit VIN (5V)        [ Buck 5V -> 3.3V ]        NeoPixel 5V
                 |                        |
          DevKit 3V3 regulator     3V3_RADIO RAIL
                 |                        |
            OLED + logic            3x radio VCC  (10uF + 100nF each)

   GND is a common pour. The main battery/charger return passes through the
   POWER SWITCH (two "split" GND pads in series) so the whole board powers off.
```

Rules:
- The charge+boost module charges the LiPo over USB-C and outputs a steady 5V
  whether on USB or battery. That 5V is the main rail.
- The three radios get their own **3.3V rail from the buck**, separate from the
  DevKit's onboard 3V3 regulator. This is deliberate: the radios (PA/LNA type)
  draw current bursts that brown out a shared 3V3. Give them a clean, dedicated
  supply with local decoupling (section 6).
- The OLED and the encoder/button pull-ups reference the DevKit 3V3 pin.
- Provide external **5V input pads** in parallel with the boost output.
- Provide **two GND pads in series** for an off-board power switch in the battery
  return (so the switch kills everything).

## 4. Bill of materials and footprints

| Ref   | Part | Mount | Size / footprint |
|-------|------|-------|------------------|
| U1 | ESP32 DevKit, 30-pin DOIT v1 (WROOM, micro-USB) | Through-hole, socketed on 2x 1x15 female headers, 2.54 mm | Board 51.53 mm long (measured). Width ~25.4 mm; header rows 0.9" (22.86 mm) apart. CONFIRM width + row spacing from the photo/caliper. |
| U2,U3,U4 | nRF24L01 / E01 radio module x3 | Surface mount (module solders flat to carrier via its 2x4 header pads) | Standard module ~15.5 x 29 mm, 2x4 pin header at 2.54 mm pitch (pins: GND, VCC, CE, CSN, SCK, MOSI, MISO, IRQ). CONFIRM your E01 variant's outline and antenna connector (PCB trace / U.FL / SMA). |
| U5 | Buck converter 5V -> 3.3V | Surface mount | 11.75 x 17.5 mm (measured). Input pads on one short side, output pads on the other. CONFIRM IN+/IN-/OUT+/OUT- pad positions from the photo. |
| U6 | LiPo charger + 5V boost module (USB-C, ETA PMIC) | Surface mount | ~26.3 mm on the measured side (measured). CONFIRM second dimension and pad locations: B+/B- (battery), 5V out, GND. USB-C connector is on the module. |
| SW1 | EC11 rotary encoder with push switch | Through-hole | Standard EC11 footprint: 3 phase pins (A, C, B) on one side at 2.54 mm, 2 switch pins (D, E) on the other. See datasheet SKU SWH1054 / handsontec EC11. |
| SW2 | Tactile button | Through-hole or SMD | 6 mm tactile, 2 effective terminals. |
| SW3 | Power switch | Off-board, via 2 split GND pads | Two pads in series in the battery return; also acceptable to place a slide-switch footprint (e.g. SS12D00). |
| J1 | External 5V input | 2 pads | 5V, GND. |
| J2 | LiPo battery | 2 pads | B+, B- to charger. |
| DISP1 | SSD1306 0.96" OLED, I2C | 4-pin header or cable | GND, VCC (3V3), SCL, SDA. CONFIRM: mounted on the carrier front, or off-board on a cable to the case. |
| C1..C3 | Radio decoupling | 0805 | 10 uF per radio VCC. |
| C4..C6 | Radio decoupling | 0603 | 100 nF per radio VCC. |
| C7 | 5V bulk | 0805/1206 | 100 uF. |
| C8 | 3V3 radio bulk | 0805 | 47 uF. |
| C9 | OLED decoupling | 0603 | 100 nF. |

## 5. Netlist

ESP32 GPIO assignments (these are fixed by the firmware, do not change them):

| Net | ESP32 GPIO | Connects to |
|-----|-----------|-------------|
| SPI_SCK  | 18 | U2/U3/U4 SCK (shared) |
| SPI_MISO | 19 | U2/U3/U4 MISO (shared) |
| SPI_MOSI | 23 | U2/U3/U4 MOSI (shared) |
| RADIO_A_CE  | 5  | U2 CE |
| RADIO_A_CSN | 17 | U2 CSN |
| RADIO_B_CE  | 16 | U3 CE |
| RADIO_B_CSN | 4  | U3 CSN |
| RADIO_C_CE  | 15 | U4 CE |
| RADIO_C_CSN | 2  | U4 CSN |
| OLED_SDA | 21 | DISP1 SDA |
| OLED_SCL | 22 | DISP1 SCL |
| NEOPIXEL | 14 | WS2812 DIN (if a NeoPixel is fitted; else leave a pad) |
| ENC_A | 25 | SW1 phase A |
| ENC_B | 26 | SW1 phase B |
| ENC_SW | 27 | SW1 switch (D); other switch leg (E) to GND |
| BUTTON | 32 | SW2; other leg to GND |
| SD_CS | 13 | microSD CS pad (footprint optional, not populated) |

Notes:
- All radio IRQ pins: no connect.
- Encoder common (C) to GND. Button and encoder-switch return to GND.
- Radio VCC to the 3V3_RADIO rail (buck output). OLED VCC to DevKit 3V3.
- GPIO2 is a boot strapping pin; it is used as RADIO_C_CSN. Keep it free of
  pull-downs that would block boot (a plain CSN trace is fine).

## 6. Power and decoupling rules

- Place 10 uF + 100 nF right at each radio module's VCC/GND pads.
- Bulk 100 uF on the 5V rail near the buck input; 47 uF on the 3V3_RADIO rail at
  the buck output.
- Keep the 3V3_RADIO rail as a short, wide trace or local pour to the three
  radios. Star the grounds back to a common point.
- Keep the switching nodes of the buck/charger modules away from the SPI traces
  and the antenna leads.

## 7. Mechanical and layout

- **Smallest practical outline.** Suggested arrangement: socket the DevKit down
  the center (it is the biggest part at 51.5 mm). Its radio-side GPIOs (2, 4, 5,
  15, 16, 17, 18, 19, 23) are all on one header, so place the three radios along
  that edge for short SPI runs. The encoder (25/26/27), button (32), and
  NeoPixel (14) are on the other header; put the encoder and button on the front
  edge. OLED (21/22) is on the DevKit's 3V3/right side.
- The three radios sit at one end so their antennas exit the same side of the
  case. Antennas mount to the enclosure; provide a labeled pad or short pigtail
  landing for each antenna feed if the modules use external connectors.
- Expose the DevKit micro-USB and the charger USB-C at the board edge.
- Add corner mounting holes if space allows.

## 8. Items to confirm (defaults in section 4 if you cannot)

1. DevKit width + header row spacing (default: 25.4 mm wide, 0.9" rows).
2. Charger module: second dimension + pad map (B+, B-, 5V, GND).
3. Buck module: IN/OUT pad positions.
4. E01 module: outline, pad pitch, antenna connector type.
5. OLED: on the carrier or off-board on a cable.
6. Power switch: off-board via pads (default) or an on-board slide switch.

## 9. Firmware context (for reference only)

The firmware already drives this exact pin map. Input is a rotary encoder
(rotate = navigate, press = select) plus one button (tap = back, hold = right).
No PCB change affects the firmware as long as the GPIO map in section 5 holds.

## 10. Photos to attach to the chat

1. Buck converter with calipers (shows the 11.75 x 17.5 mm module and its pads).
2. Charger + boost module with calipers (USB-C module, ~26.3 mm).
3. ESP32 DevKit with calipers (30-pin, 51.53 mm long).
4. The rotary encoder module (EC11, to confirm the footprint).
