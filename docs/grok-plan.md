# Cyber Rosary – Project Plan

**A pocket-sized digital rosary with ESP32-S3 Supermini, e-ink display, and Cybertruck-inspired angular metal enclosure.**

**Goal:** Create a simple, low-power device that feels like a traditional rosary but with a futuristic Cybertruck aesthetic. The device sleeps most of the time and wakes only on button press to advance the prayer, update the e-ink display, and optionally send usage stats (total rosaries prayed) via BLE to a phone.

## 1. Project Overview
- **Name:** Cyber Rosary
- **Theme:** Miniaturized Cybertruck exoskeleton – sharp angular facets, brushed stainless steel or aluminum finish, visible screws, chamfered edges, raw industrial look.
- **Core Functionality:**
  - Button press → advance bead/decade/mystery
  - Update 1.54" e-ink display (shows current count, prayer text, total prayed)
  - Optional short BLE burst to sync total rosaries to phone
  - Immediate return to deep sleep
- **Power:** Rechargeable small LiPo via the board’s USB-C port
- **Target Size:** ~55 mm × 40 mm × 12 mm (pocketable, slightly thicker than a credit card)

## 2. Hardware Components
- **Microcontroller:** ESP32-S3 Supermini (you already have several)  
  - Size: ~23.5 mm × 18 mm  
  - Built-in USB-C and LiPo charging circuit (connect battery to BAT+/BAT- pads on the back)
- **Display:** 1.54" e-ink module (200×200 resolution, black/white or tri-color)  
  - Outline: ~48 mm × 33 mm  
  - Active area: ~27.6 mm × 27.6 mm  
  - Driver: Usually SSD1680 (use GxEPD2 library)
- **Battery:** 200–500 mAh 3.7V LiPo pouch cell with protection circuit (e.g., 301020, 401020, or 402030 size)  
  - Connect directly to board’s battery pads; charge via USB-C
- **Buttons:** 1–2 tactile switches (one for “next bead”, optional second for sync or mode)
- **Other:** Thin gasket for dust resistance, M2 screws for assembly, optional clear acrylic window over e-ink

## 3. Enclosure (Chassis) Design – Cybertruck Inspired
- **Style:** Sharp polygonal facets, 45° chamfers, flat planes, visible corner screws, brushed metal finish
- **Construction:** Two-part clamshell (bottom tray + top lid)
  - Bottom: Holds Supermini, battery, and wiring
  - Top: Precise cutout for e-ink + button holes with angular guards
- **Recommended Build Process:**
  1. Design in **Fusion 360** (free for hobbyists)
  2. 3D print plastic prototypes (PLA/PETG) to test fit and iterate
  3. Final version: CNC mill from aluminum or stainless steel block, or sheet-metal fabrication (laser cut + bend)
- **Services for metal:** Xometry, SendCutSend, JLCPCB, or local makerspaces in Madison, WI
- **Finishing:** Brushed + clear coat or bead-blasted for authentic Cybertruck look

**External Dimensions (approximate):** 55 mm (L) × 40 mm (W) × 12 mm (H)

## 4. Software / Firmware Plan
- **IDE:** Arduino IDE (easiest) or ESP-IDF
- **Key Libraries:**
  - GxEPD2 (for e-ink, supports partial updates)
  - NimBLE-Arduino or ESP32 BLE (for low-power advertising/notifications)
  - Preferences or RTC memory for persistent prayer counter
- **Main Flow (on button press):**
  1. Wake from deep sleep via GPIO interrupt
  2. Increment rosary state (bead → decade → mystery logic)
  3. Update e-ink display (large font for count, smaller text for context)
  4. Optional: Quick BLE advertise/connect → send total rosaries prayed → disconnect
  5. Enter deep sleep (`esp_deep_sleep_start()`)
- **Power Optimizations:** Disable radios when idle, short active bursts only, use deep sleep (~10–50 µA target)
- **Expected Battery Life:** Several months to over a year on a 300–500 mAh LiPo with daily use (thanks to infrequent wakes and e-ink’s zero-power image retention)

## 5. Build Steps
1. **Prototype Electronics**  
   - Solder LiPo to BAT+/BAT- pads  
   - Wire e-ink via SPI (choose available GPIOs on Supermini)  
   - Wire buttons to GPIO pins with internal pull-ups  
   - Test charging via USB-C and basic deep sleep

2. **Develop Firmware**  
   - Start with e-ink + button wake example  
   - Add rosary logic and persistent storage  
   - Implement optional BLE sync

3. **Design & Prototype Enclosure**  
   - Create Fusion 360 model with angular facets and cutouts  
   - 3D print and test-fit all components  
   - Iterate design

4. **Fabricate Metal Case**  
   - Export STEP file  
   - Get quotes for CNC milling (aluminum or stainless) or sheet metal

5. **Final Assembly**  
   - Mount components in metal chassis  
   - Add screws and optional gasket  
   - Flash final firmware

6. **Testing & Polish**  
   - Measure current draw in sleep/active modes  
   - Refine UI on e-ink (fonts, layout)  
   - Test BLE with phone (nRF Connect or custom app)

## 6. Estimated Cost (excluding tools you already have)
- ESP32-S3 Supermini: Already owned
- 1.54" e-ink: $8–15
- 300–500 mAh LiPo: $3–6
- Buttons, wires, screws: $5
- Plastic prototypes: <$5
- Metal CNC enclosure (one-off): $30–100
- **Total rough estimate:** $50–150 depending on metal finish

## 7. Next Actions / Tips
- Confirm exact pinout of your Supermini boards (especially BAT pads and free GPIOs for SPI + buttons)
- Choose e-ink driver chip and test with GxEPD2 first
- Start the Fusion 360 model with a simple chamfered box before adding complex facets
- Document battery voltage monitoring if desired (via voltage divider on a GPIO)
- Safety: Use protected LiPo cells only

**Project Status:** Planning phase – ready for prototyping.

---

*Created for Rob Witzlib – April 2026*  
*“Pray like it’s 2030.”*
