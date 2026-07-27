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
| ST7789 BL | 32 |
| Clear/wake button | 33 (to GND) |

Display: 170×320 ST7789 (landscape)

## Features

- Receives POCSAG on 439.9875 MHz at 1200 baud
- Filters messages to specific capcodes
- Up to 20 messages queued; inbox-style reading with a single button
- Word-wrapped message text (up to 80 chars per message)
- Real-time clock synced from NTP at boot (UK GMT/BST)
- Falls back to uptime display (`~HH:MM:SS`) if NTP fails
- Permanent status bar showing clock, NTP status, radio status and unread count
- Display and backlight sleep after 20 seconds of inactivity; button or incoming message wakes it

## Message flow

1. **Message arrives** — display wakes and shows centred "New Message"; blanks after 10 seconds but the pager continues receiving in the background.
2. **Press button (if sleeping)** — wakes the display. If messages are waiting, "New Message" is shown.
3. **Press button (awake)** — reads messages one at a time, each showing the timestamp on the first line and message text below.
4. **Press button after last message** — clears all messages and returns to idle.
5. **No activity for 20 seconds** — display and backlight turn off automatically.

## Display layout

```
┌──────────────────────────────────────┐
│                                      │
│           New Message                │
│                                      │
├──────────────────────────────────────┤
│ 14:35:12  NTP:OK  R:OK      MSG:2   │
└──────────────────────────────────────┘
```

While reading a message:

```
┌──────────────────────────────────────┐
│ 14:32:01                             │
│ Hello from DAPNET, this is a longer  │
│ message that wraps onto a second     │
│ line automatically                   │
├──────────────────────────────────────┤
│ 14:35:12  NTP:OK  R:OK      MSG:1   │
└──────────────────────────────────────┘
```

The status bar is permanently reserved at the bottom. `MSG:n` (yellow) shows the unread count and disappears when all messages have been read.

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

## Arduino IDE board settings

| Setting | Value |
|---------|-------|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| CPU Frequency | 240MHz (WiFi/BT) |
| Flash Frequency | 80MHz |
| Flash Mode | DIO |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS) |
| Core Debug Level | None |
| PSRAM | Disabled |

## Dependencies

- [RadioLib](https://github.com/jgromes/RadioLib)
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit ST7789](https://github.com/adafruit/Adafruit-ST7735-and-ST7789-Library)
