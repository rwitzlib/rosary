# Cyber Rosary Starter Wiring

This starter firmware targets a Seeed Studio XIAO ESP32S3 wired to a Waveshare 1.54 inch SPI e-paper module. The common 1.54 inch 200x200 black/white Waveshare modules use an SPI header labeled `VCC`, `GND`, `DIN`, `CLK`, `CS`, `DC`, `RST`, and `BUSY`.

## Pin Table

| Waveshare e-paper pin | ESP32-S3 GPIO | Firmware symbol | Notes |
| --- | ---: | --- | --- |
| `VCC` | `3V3` | - | Use 3.3 V, not 5 V. |
| `GND` | `GND` | - | Common ground. |
| `DIN` / `MOSI` | `GPIO2` | `EPD_MOSI` | SPI data from ESP32 to display. |
| `CLK` / `SCK` | `GPIO1` | `EPD_SCK` | SPI clock. |
| `CS` | `GPIO3` | `EPD_CS` | E-paper chip select. |
| `DC` | `GPIO4` | `EPD_DC` | Data/command select. |
| `RST` | `GPIO5` | `EPD_RST` | Display reset. |
| `BUSY` | `GPIO6` | `EPD_BUSY` | Display busy signal to ESP32. |
| Button side A | `GPIO8` | `NEXT_BUTTON` | Uses internal pull-up; press connects to ground. |
| Button side B | `GND` | - | No external resistor needed for first prototype. |

This leaves `GPIO7`, `GPIO9`, `GPIO43`, and `GPIO44` unused. `GPIO43` and `GPIO44` are often used for serial, so avoid them for the first prototype.

## Text Schematic

```text
ESP32-S3                         Waveshare 1.54" e-paper
--------                         ------------------------
3V3       ---------------------> VCC
GND       ---------------------> GND
GPIO2     ---------------------> DIN / MOSI
GPIO1     ---------------------> CLK / SCK
GPIO3     ---------------------> CS
GPIO4     ---------------------> DC
GPIO5     ---------------------> RST
GPIO6     <--------------------- BUSY

ESP32-S3                         Momentary push button
--------                         ----------------------
GPIO8     ---------------------> side A
GND       ---------------------> side B
```

## Power Notes

- Power the e-paper module from `3V3`.
- Connect a protected 3.7 V LiPo only to the board's battery pads or battery connector, not directly to `3V3`.
- For the first bring-up, test from USB power before adding the battery.
- The sketch updates the display, stays awake while button presses continue, then enters deep sleep after 90 seconds with no button activity. Pressing the button wakes the ESP32 and advances the rosary state.
- Hearing the USB disconnect sound when it enters deep sleep is normal. The ESP32-S3 turns off its USB connection while sleeping.

## Button Troubleshooting

If the board wakes immediately every 15 seconds and increments without a press, the wake pin is being read as `LOW`.

Check these first:

- The button should connect `GPIO8` to `GND` only while pressed.
- On a 4-leg tactile switch, use legs on opposite sides of the switch, not two legs on the same side.
- There should be no connection from `GPIO8` to `GND` when the button is released.
- If you have a multimeter, continuity between `GPIO8` and `GND` should happen only when the button is pressed.
- If the problem continues, move the button from `GPIO8` to `GPIO7` and change `NEXT_BUTTON` in `hardware/src/main.cpp` to `7`.

## Display Driver Note

The firmware uses `GxEPD2_154_D67`, which matches many Waveshare 1.54 inch 200x200 black/white modules. If the display stays blank after wiring is confirmed, your module may be an older revision. In that case, change this line in `hardware/src/main.cpp`:

```cpp
using Display154 = GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>;
```

to the older 1.54 inch driver class supported by GxEPD2, commonly `GxEPD2_154`.
