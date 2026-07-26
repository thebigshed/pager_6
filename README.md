# DAPNET Pager

An ESP32-based DAPNET pager using a CC1101 radio module and ST7789 display.

## Hardware

| Component | Pin |
|-----------|-----|
| CC1101 CS | 27 |
| CC1101 GDO2 | 25 |
| CC1101 SCK | 18 |
| CC1101 MISO | 19 |
| CC1101 MOSI | 23 |
| ST7789 CS | 15 |
| ST7789 DC | 2 |
| ST7789 RST | 4 |

Display: 170×320 ST7789 (landscape)

## Features

- Receives POCSAG on 439.9875 MHz at 1200 baud
- Filters messages to specific capcodes
- Word-wrapped message display with scrolling (up to 80 chars per message)
- Each message shown as a timestamp line followed by the message text
- Real-time clock synced from NTP at boot (UK GMT/BST)
- Falls back to uptime display (`~HH:MM:SS`) if NTP fails
- Permanent status bar showing clock, NTP status and radio status

## Display layout

```
┌─────────────────────────────────┐
│ 14:32:01                        │
│ Hello from DAPNET               │
│ 14:35:12                        │
│ Another message that wraps      │
│ across multiple lines           │
│                                 │
├─────────────────────────────────┤
│ 14:35:12  NTP:OK  R:OK          │
└─────────────────────────────────┘
```

The status bar is permanently reserved at the bottom and cannot be overwritten by messages.

## Configuration

Edit these lines in `pager_6.ino` before flashing:

```cpp
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"
```

To change the capcodes that are displayed:

```cpp
const uint32_t allowedCapcodes[] = {
  341516,
  8,
  123456,
  214
};
```

## Dependencies

- [RadioLib](https://github.com/jgromes/RadioLib)
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit ST7789](https://github.com/adafruit/Adafruit-ST7735-and-ST7789-Library)
