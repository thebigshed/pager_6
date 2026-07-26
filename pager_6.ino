#include <RadioLib.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <time.h>

// =====================
// WIFI / NTP
// =====================
#define WIFI_SSID     "the-big-shed"
#define WIFI_PASSWORD "1spider2"
// UK: base UTC+0, DST +1h (handles GMT/BST automatically)
#define NTP_GMT_OFFSET_SEC      0
#define NTP_DAYLIGHT_OFFSET_SEC 3600
#define NTP_SERVER              "pool.ntp.org"

// =====================
// CC1101 + DISPLAY PINS
// =====================
#define CC1101_CS   27
#define CC1101_GDO2 25
#define BTN_CLEAR   33

#define LCD_CS   15
#define LCD_DC   2
#define LCD_RST  4


// =====================
// OBJECTS
// =====================
CC1101 radio = new Module(CC1101_CS, 26, RADIOLIB_NC, CC1101_GDO2);
PagerClient pager(&radio);
Adafruit_ST7789 lcd(LCD_CS, LCD_DC, LCD_RST);

// =====================
// DAPNET SETTINGS
// =====================
const float RX_FREQ = 439.9875;
const uint16_t RX_BAUD = 1200;

// One *real* capcode to lock decoder stability
const uint32_t LOCK_CAPCODE = 341516;

// Capcodes we actually care about
const uint32_t allowedCapcodes[] = {
  341516,
  8,
  123456,
  214
};
const uint8_t capcodeCount = sizeof(allowedCapcodes) / sizeof(allowedCapcodes[0]);

// =====================
// DISPLAY
// =====================
const uint8_t TEXT_SIZE  = 2;
const uint8_t CHAR_W     = 6 * TEXT_SIZE;
const uint8_t CHAR_H     = 8 * TEXT_SIZE;
const uint8_t STATUS_H   = 12;   // px reserved at bottom for status bar

// Scratch buffer for the current screen view (rebuilt on each draw)
const uint8_t MAX_LINES = 8;
String screenBuffer[MAX_LINES];
uint8_t lineCount = 0;

// =====================
// MESSAGE QUEUE
// =====================
struct Message {
  String time;
  String text;
};

const uint8_t MAX_MESSAGES = 20;
Message msgQueue[MAX_MESSAGES];
uint8_t msgCount = 0;   // total messages stored
uint8_t msgRead  = 0;   // how many have been shown to the user

enum DisplayState { IDLE, WAITING, READING };
DisplayState displayState = IDLE;
uint32_t waitingShownMs = 0;   // millis() when waiting screen last appeared

// =====================
// STATUS STATE
// =====================
bool radioOk = false;
bool ntpOk   = false;

// =====================
// TIME (NTP-set, falls back to uptime)
// =====================
String timestamp() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    char buf[10];
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    return String(buf);
  }
  // Fallback: uptime counter prefixed with '~'
  uint32_t t = millis() / 1000;
  char buf[11];
  sprintf(buf, "~%02u:%02u:%02u", (t / 3600) % 24, (t / 60) % 60, t % 60);
  return String(buf);
}

// =====================
// CAPCODE FILTER
// =====================
bool capcodeAllowed(const String& msg) {
  for (uint8_t i = 0; i < capcodeCount; i++) {
    if (msg.indexOf(String(allowedCapcodes[i])) != -1) {
      return true;
    }
  }
  return false;
}

// =====================
// WORD-SAFE LINE WRAPPER
// =====================
uint8_t wrapLine(
  const String& line,
  String* out,
  uint8_t maxLines,
  uint16_t screenWidth
) {
  const uint8_t maxChars = screenWidth / CHAR_W;

  uint8_t count = 0;
  String current = "";

  int i = 0;
  while (i < line.length() && count < maxLines) {
    int nextSpace = line.indexOf(' ', i);
    if (nextSpace == -1) nextSpace = line.length();

    String word = line.substring(i, nextSpace);
    i = nextSpace + 1;

    // Word longer than full line → hard split
    if (word.length() > maxChars) {
      if (current.length()) {
        out[count++] = current;
        current = "";
      }
      for (uint16_t p = 0; p < word.length() && count < maxLines; p += maxChars) {
        out[count++] = word.substring(p, p + maxChars);
      }
      continue;
    }

    if (!current.length()) {
      current = word;
    } else if (current.length() + 1 + word.length() <= maxChars) {
      current += " " + word;
    } else {
      out[count++] = current;
      current = word;
    }
  }

  if (current.length() && count < maxLines) {
    out[count++] = current;
  }

  return count;
}

// =====================
// SCREEN HANDLING
// =====================
void addLine(const String& line) {
  if (lineCount < MAX_LINES) {
    screenBuffer[lineCount++] = line;
  }
}

void showWaiting() {
  uint8_t  unread = msgCount - msgRead;
  String   line1  = String(unread) + (unread == 1 ? " message" : " messages");
  String   line2  = "waiting";
  uint16_t msgH   = lcd.height() - STATUS_H - 1;

  lcd.fillRect(0, 0, lcd.width(), msgH, ST77XX_BLACK);

  uint16_t y  = (msgH - 2 * CHAR_H) / 2;
  uint16_t x1 = (lcd.width() - line1.length() * CHAR_W) / 2;
  uint16_t x2 = (lcd.width() - line2.length() * CHAR_W) / 2;

  lcd.setCursor(x1, y);           lcd.print(line1);
  lcd.setCursor(x2, y + CHAR_H);  lcd.print(line2);

  drawStatusBar();
  lineCount = 0;
  displayState = WAITING;
  waitingShownMs = millis();
}

void showMessage(uint8_t idx) {
  lineCount = 0;
  addLine(msgQueue[idx].time);
  String wrapped[4];
  uint8_t lines = wrapLine(msgQueue[idx].text, wrapped, 4, lcd.width());
  for (uint8_t i = 0; i < lines; i++) addLine(wrapped[i]);
  displayState = READING;
}

void drawStatusBar() {
  uint16_t sepY = lcd.height() - STATUS_H - 1;
  uint16_t barY = lcd.height() - STATUS_H;

  lcd.drawFastHLine(0, sepY, lcd.width(), ST77XX_CYAN);
  lcd.fillRect(0, barY, lcd.width(), STATUS_H, ST77XX_BLACK);

  lcd.setTextSize(1);
  lcd.setCursor(2, barY + 2);

  lcd.setTextColor(ST77XX_WHITE);
  lcd.print(timestamp());

  lcd.print("  NTP:");
  lcd.setTextColor(ntpOk ? ST77XX_GREEN : ST77XX_RED);
  lcd.print(ntpOk ? "OK" : "--");

  lcd.setTextColor(ST77XX_WHITE);
  lcd.print("  R:");
  lcd.setTextColor(radioOk ? ST77XX_GREEN : ST77XX_RED);
  lcd.print(radioOk ? "OK" : "!!");

  uint8_t unread = msgCount - msgRead;
  if (unread > 0) {
    String label = "MSG:" + String(unread);
    lcd.setCursor(lcd.width() - label.length() * 6 - 2, barY + 2);
    lcd.setTextColor(ST77XX_YELLOW);
    lcd.print(label);
  }

  // restore defaults for message area
  lcd.setTextSize(TEXT_SIZE);
  lcd.setTextColor(ST77XX_WHITE);
}

void redrawScreen() {
  uint16_t msgH      = lcd.height() - STATUS_H - 1;  // px available for messages
  uint8_t  maxVisible = msgH / CHAR_H;

  lcd.fillRect(0, 0, lcd.width(), msgH, ST77XX_BLACK);
  lcd.setCursor(0, 0);

  uint8_t start = (lineCount > maxVisible) ? lineCount - maxVisible : 0;
  for (uint8_t i = start; i < lineCount; i++) {
    lcd.println(screenBuffer[i]);
  }

  drawStatusBar();
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);

  // Kill WiFi hardware before SPI starts — its background tasks corrupt SPI
  // register writes during radio init. Re-enable below for NTP only.
  WiFi.mode(WIFI_OFF);
  delay(100);

  SPI.begin(18, 19, 23, CC1101_CS);

  lcd.init(170, 320);
  lcd.setRotation(1);
  lcd.setTextSize(TEXT_SIZE);
  lcd.setTextColor(ST77XX_WHITE);
  lcd.setTextWrap(false);
  lcd.fillScreen(ST77XX_BLACK);
  lcd.setCursor(0, 0);
  lcd.println("DAPNET Pager");
  lcd.println("Initialising...");

  int radioState = radio.begin();
  if (radioState != RADIOLIB_ERR_NONE) {
    lcd.print("Radio FAIL: ");
    lcd.println(radioState);
    while (true);
  }
  radioOk = true;
  pager.begin(RX_FREQ, RX_BAUD);
  radio.setRxBandwidth(45.0);
  radio.setFrequencyDeviation(15.0);

  // --- NTP time sync ---
  lcd.print("WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // reduce peak current to avoid brownout
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    lcd.println("OK");
    configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    lcd.print("NTP...");
    struct tm timeinfo;
    ntpOk = getLocalTime(&timeinfo, 8000);
    if (ntpOk) {
      char tbuf[20];
      strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &timeinfo);
      lcd.println(tbuf);
    } else {
      lcd.println("FAIL");
    }
    WiFi.disconnect(true);
  } else {
    lcd.println("FAIL");
  }
  // ---------------------

  pinMode(BTN_CLEAR, INPUT_PULLUP);

  pager.startReceive(CC1101_GDO2, LOCK_CAPCODE);

  // Clear startup text and draw the initial status bar
  lcd.fillRect(0, 0, lcd.width(), lcd.height() - STATUS_H - 1, ST77XX_BLACK);
  drawStatusBar();
}

// =====================
// LOOP
// =====================
void loop() {
  static uint32_t lastStatusMs = 0;
  if (millis() - lastStatusMs >= 1000) {
    drawStatusBar();
    lastStatusMs = millis();
  }

  if (displayState == WAITING && millis() - waitingShownMs >= 10000) {
    uint16_t msgH = lcd.height() - STATUS_H - 1;
    lcd.fillRect(0, 0, lcd.width(), msgH, ST77XX_BLACK);
    displayState = IDLE;
  }

  // Button: step through messages, then clear on final press
  static bool lastBtn = HIGH;
  bool btn = digitalRead(BTN_CLEAR);
  if (lastBtn == HIGH && btn == LOW) {
    if (msgRead < msgCount) {
      showMessage(msgRead++);
      redrawScreen();
    } else {
      msgCount = 0;
      msgRead  = 0;
      lineCount = 0;
      displayState = IDLE;
      redrawScreen();
    }
  }
  lastBtn = btn;

  if (pager.available() >= 3) {
    String msg;
    int state = pager.readData(msg);

    Serial.print("[Pager] state=");
    Serial.print(state);
    Serial.print(" msg='");
    Serial.print(msg);
    Serial.println("'");

    if (state == RADIOLIB_ERR_NONE && msg.length() > 0) {
      if (msgCount < MAX_MESSAGES) {
        msgQueue[msgCount].time = timestamp();
        msgQueue[msgCount].text = msg;
        msgCount++;
      }
      // Only interrupt the display if the user isn't mid-reading
      if (displayState != READING) {
        showWaiting();
      }
    }
  }
}
