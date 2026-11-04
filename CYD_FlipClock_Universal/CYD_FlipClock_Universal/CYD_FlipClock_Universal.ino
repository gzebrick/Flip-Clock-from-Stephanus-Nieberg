// ═══════════════════════════════════════════════════════════════════════════════
// CYD Flip Clock — Universal Edition (Single .ino)
// Compatible with ALL Cheap Yellow Display (CYD) Boards & ESP32-S3 RGB Displays
// Powered by AMK_CYD_Universal_TFT driver library
// ═══════════════════════════════════════════════════════════════════════════════
// Supported Hardware (Auto-Probed at Boot):
//   - 2.4" CYD (ESP32-2432S024 / ILI9341 / ST7789, 320x240)
//   - 2.8" CYD (ESP32-2432S028R / ILI9341 / ST7789, 320x240, Micro-USB & USB-C)
//   - 3.2" CYD (ESP32-2432S032 / ST7789, 320x240)
//   - 3.5" CYD (ESP32-3248S035 / ST7796, 480x320)
//   - 4.0" CYD (ESP32-3248S040 / ST7796, 480x320)
//   - 4.3", 5.0", 7.0" ESP32-S3 Parallel RGB (800x480 ST7262 + GT911 touch)
// ═══════════════════════════════════════════════════════════════════════════════
// Features:
//  - Autonomous hardware auto-detection (panel type, wiring bus, backlight pin)
//  - Built-in Web Control Page: navigate to clock's IP (e.g. http://192.168.1.50)
//  - Full Timezone selection with automatic Daylight Saving Time (DST) handling
//  - Persistent settings stored in ESP32 NVS flash (survives reboots)
//  - Screen Flip / Rotation toggle (Standard Landscape vs 180° Inverted)
//  - 12-Hour / 24-Hour time format toggle
//  - Curated Color Themes (Classic White, Vintage Amber, Cyberpunk Green, Ice Blue, Sunset Gold, Ruby Night)
//  - Custom NTP Server configuration
//  - Onboard LDR sensor: dynamic auto-dimming screen backlight based on room light
//  - Realistic mechanical 3D flap rotation projection with FreeSans24pt digits
//  - Dynamic auto-scaling vector layout across 320x240, 480x320, and 800x480 panels
//  - Wi-Fi Manager auto-connect AP with instant captive portal redirection to /wifi
//  - Dual 15s hold: Press & hold physical BOOT button OR touch screen to enter AP mode (with live countdown)
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <AMK_CYD_Universal_TFT.h>
#include <ESPmDNS.h>
#include <time.h>
#include <sys/time.h>

// Days of the week abbreviations
const char* const DAYS_OF_WEEK[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

// ── 1. TIMEZONES WITH AUTOMATIC DAYLIGHT SAVING TIME (DST) ───────────────────
struct TimeZoneOption {
  const char* label;
  const char* posix;
};

const TimeZoneOption TIMEZONES[] = {
  { "(UTC+1/+2) Amsterdam, Berlin, Paris, Rome [DST]",   "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "(UTC+0/+1) London, Dublin [DST]",                   "GMT0BST,M3.5.0/1,M10.5.0" },
  { "(UTC-5/-4) New York, Toronto, Miami [DST]",         "EST5EDT,M3.2.0,M11.1.0" },
  { "(UTC-6/-5) Chicago, Dallas, Houston [DST]",         "CST6CDT,M3.2.0,M11.1.0" },
  { "(UTC-7/-6) Denver, Salt Lake, Calgary [DST]",       "MST7MDT,M3.2.0,M11.1.0" },
  { "(UTC-8/-7) Los Angeles, SF, Seattle [DST]",         "PST8PDT,M3.2.0,M11.1.0" },
  { "(UTC-9/-8) Anchorage, Alaska [DST]",                "AKST9AKDT,M3.2.0,M11.1.0" },
  { "(UTC-10) Honolulu, Hawaii",                         "HST10" },
  { "(UTC+0) UTC / Greenwich Mean Time",                 "UTC0" },
  { "(UTC+2/+3) Athens, Cairo, Helsinki [DST]",          "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "(UTC+3) Moscow, Riyadh, Istanbul",                  "MSK-3" },
  { "(UTC+4) Dubai, Abu Dhabi, Muscat",                  "GST-4" },
  { "(UTC+5:30) India / New Delhi, Mumbai",              "IST-5:30" },
  { "(UTC+5:45) Nepal / Kathmandu",                      "NPT-5:45" },
  { "(UTC+6:30) Myanmar / Yangon",                       "MMT-6:30" },
  { "(UTC+7) Bangkok, Hanoi, Jakarta",                   "ICT-7" },
  { "(UTC+8) Singapore, Kuala Lumpur, Taipei, Perth",    "SGT-8" },
  { "(UTC+8) Hong Kong, Beijing, Shanghai",              "HKT-8" },
  { "(UTC+9) Tokyo, Seoul",                              "JST-9" },
  { "(UTC+9:30/+10:30) Adelaide [DST]",                  "ACST-9:30ACDT,M10.1.0,M4.1.0/3" },
  { "(UTC+10/+11) Sydney, Melbourne, Canberra [DST]",    "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "(UTC+10) Brisbane (no DST)",                        "AEST-10" },
  { "(UTC+12/+13) Auckland, Wellington [DST]",           "NZST-12NZDT,M9.5.0,M4.1.0/3" }
};
const int NUM_TIMEZONES = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);

// ── COLOR THEMES (Curated luminous palettes with dark 3D flap cards) ───────────
struct ClockTheme {
  const char* name;
  uint16_t digitColor;   // Flap digits
  uint16_t labelColor;   // Column headers: HOURS, MINUTES, SECONDS
  uint16_t dateColor;    // Date readout text
  uint16_t statusColor;  // Wi-Fi & IP readout color
};

const ClockTheme THEMES[] = {
  // Name,                       Digits,  Labels,  Date,    Status
  { "Classic White",             0xFFFF,  0x8410,  0xFFFF,  0x39E7 }, // Crisp white digits, emerald green status
  { "Vintage Amber (Nixie)",     0xFD00,  0xB400,  0xFE48,  0xFD00 }, // Warm 1970s Nixie amber glow
  { "Cyberpunk Green (Matrix)",  0x3FE2,  0x1C64,  0x87F0,  0x3FE2 }, // 1980s retro phosphor green
  { "Glacier Ice Blue",          0x07FF,  0x33B9,  0xC7FF,  0x07FF }, // Clean futuristic ice cyan
  { "Sunset Gold",               0xFFE0,  0xA500,  0xFFF0,  0xFFE0 }, // Warm golden luxury look
  { "Ruby Night Mode",           0xF862,  0x7800,  0xF9A4,  0xF862 }  // Astronomy-safe deep crimson (no blue light)
};
const int NUM_THEMES = sizeof(THEMES) / sizeof(THEMES[0]);

// Wi-Fi Access Point fallback configuration
constexpr char SETUP_SSID[]     = "CYD-FlipClock";
constexpr char SETUP_PASSWORD[] = "flipclock24";

// ── 2. GLOBAL HARDWARE & SERVER OBJECTS ────────────────────────────────────────
AMK_CYD_display display;
AMK_CYD_touch   touch;
TFT_eSPI        tft;
WebServer       server(80);
Preferences     prefs;
WiFiUDP         dnsUdp;
const byte      DNS_PORT = 53;

// Wi-Fi & AP Mode State
bool          apModeActive          = false;
unsigned long lastStationActiveTime = 0;
String        savedSSID             = "";
String        savedPass             = "";

// ── 3. DYNAMIC LAYOUT METRICS (Computed dynamically based on detected panel) ───
int SW, SH;                                 // Screen dimensions
int W, H, HALF;                             // Flip card dimensions & hinge row
int GAP, START_X, TOP;                      // Spacing and position
int fontScale;                              // 1x, 2x, or 3x for FreeSans24pt7b
int titleY, titleTextSize;                  // Header title
int labelY, labelTextSize;                  // Flap column labels
int dateY, dateTextSize;                    // Date readout row
int statusY1, statusY2, statusTextSize;     // Wi-Fi & NTP status lines
int hingePinW, hingePinH;                   // Split flap mechanical hinge pins

// ── 4. OFF-SCREEN SPRITE BUFFERS ──────────────────────────────────────────────
TFT_eSprite oldCard  = TFT_eSprite(&tft);
TFT_eSprite newCard  = TFT_eSprite(&tft);
TFT_eSprite frameSpr = TFT_eSprite(&tft);

// ── 5. RUNTIME STATE & LDR CONTROL ────────────────────────────────────────────
int    shown[3] = {-1, -1, -1};
int    lastDate = -1;
int    lastAmPmShown = -1;
String lastStatusText = "";
bool   networkTimeStarted = false;
bool   serverStarted      = false;
String serialLine;

// Persistent Settings (loaded from NVS flash)
String currentTZ        = "MMT-6:30";
bool   autoBrightness   = true;
int    manualBrightness = 180;
int    activeBrightness = 180;
int    screenRotation   = 3;           // 3 = Standard Landscape, 1 = Flipped 180°
bool   use24Hour        = true;        // true = 24h, false = 12h
int    currentTheme     = 0;           // 0 = Classic White, 1 = Amber, 2 = Green, 3 = Blue, 4 = Gold, 5 = Ruby
String ntpServer        = "pool.ntp.org";
int    lastLDRValue     = 0;

// LDR sensor definitions
constexpr uint8_t LDR_PIN          = 34; // Onboard Light Dependent Resistor
constexpr int     NUM_LDR_READINGS = 10; // Number of readings to average for stability

// ── 6. LDR AUTO-BACKLIGHT CONTROL FUNCTION ────────────────────────────────────
void adjustBrightness() {
  static int readings[NUM_LDR_READINGS] = {0};
  static int readIndex = 0;
  static long total = 0;
  static unsigned long lastReadTime = 0;

  if (millis() - lastReadTime > 80) {
    lastReadTime = millis();
    total -= readings[readIndex];
    readings[readIndex] = analogRead(LDR_PIN);
    total += readings[readIndex];
    readIndex = (readIndex + 1) % NUM_LDR_READINGS;
    lastLDRValue = readings[readIndex];

    if (autoBrightness) {
      long avg = total / NUM_LDR_READINGS;
      // Map ESP32 12-bit ADC (0-4095): dark room (~3600) -> 20 PWM, bright room (~200) -> 255 PWM
      int target = constrain(map(avg, 3600, 200, 20, 255), 20, 255);

      // 4-unit hysteresis deadband prevents light sensor flicker
      if (abs(target - activeBrightness) >= 4) {
        activeBrightness = target;
        uint8_t blPin = display.backlightPin();
        if (blPin > 0) {
          analogWrite(blPin, activeBrightness);
        } else {
          display.setBacklight(activeBrightness > 20);
        }
      }
    } else {
      if (activeBrightness != manualBrightness) {
        activeBrightness = manualBrightness;
        uint8_t blPin = display.backlightPin();
        if (blPin > 0) {
          analogWrite(blPin, activeBrightness);
        } else {
          display.setBacklight(activeBrightness > 20);
        }
      }
    }
  }
}

// ── 7. UI HELPER & RENDERING FUNCTIONS ────────────────────────────────────────
void centered(const char *s, int y, int size, uint16_t color, uint16_t bg = 0x0841) {
  tft.setTextFont(0);
  tft.setTextSize(size);
  tft.setTextColor(color, bg);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(s, SW / 2, y);
}

void centeredFit(const char *s, int y, int preferredSize, uint16_t color, uint16_t bg = 0x0841) {
  tft.setTextFont(0);
  int fitSize = preferredSize;
  int maxW = (SW > 0 ? SW : tft.width()) - 16;
  while (fitSize > 1) {
    tft.setTextSize(fitSize);
    if (tft.textWidth(s) <= maxW) break;
    fitSize--;
  }
  tft.setTextSize(fitSize);
  tft.setTextColor(color, bg);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(s, (SW > 0 ? SW : tft.width()) / 2, y);
}

void renderStatusRow(const String &line1, const String &line2 = "", uint16_t color1 = 0x8410, uint16_t color2 = 0x8410) {
  int statusBoxH = SH - (statusY1 - 4);
  tft.fillRect(0, statusY1 - 4, SW, statusBoxH, 0x0841);

  int maxW = (SW > 0 ? SW : tft.width()) - 16;
  tft.setTextFont(0);

  if (line2.length() == 0) {
    // Single line mode: Find largest font size (up to screen targetSize) that fits
    int targetSize = (SW >= 700) ? 3 : ((SW >= 450) ? 2 : 1);
    while (targetSize > 1) {
      tft.setTextSize(targetSize);
      if (tft.textWidth(line1.c_str()) <= maxW) break;
      targetSize--;
    }
    int yPos = (statusY2 > statusY1) ? ((statusY1 + statusY2) / 2) : statusY1;
    tft.setTextSize(targetSize);
    tft.setTextColor(color1, 0x0841);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(line1, (SW > 0 ? SW : tft.width()) / 2, yPos);
  } else {
    // Two lines mode: Auto-scale both lines so neither drops below readable size
    int targetSize1 = (SW >= 700) ? 3 : ((SW >= 450) ? 2 : 1);
    while (targetSize1 > 1) {
      tft.setTextSize(targetSize1);
      if (tft.textWidth(line1.c_str()) <= maxW) break;
      targetSize1--;
    }
    int targetSize2 = (SW >= 700) ? 3 : ((SW >= 450) ? 2 : 1);
    while (targetSize2 > 1) {
      tft.setTextSize(targetSize2);
      if (tft.textWidth(line2.c_str()) <= maxW) break;
      targetSize2--;
    }
    int finalSize = min(targetSize1, targetSize2);
    tft.setTextSize(finalSize);
    tft.setTextDatum(TC_DATUM);

    tft.setTextColor(color1, 0x0841);
    tft.drawString(line1, (SW > 0 ? SW : tft.width()) / 2, statusY1);

    tft.setTextColor(color2, 0x0841);
    tft.drawString(line2, (SW > 0 ? SW : tft.width()) / 2, statusY2);
  }
}

void card(TFT_eSprite &c, int value) {
  c.fillScreen(0x0841);                         // Background canvas (dark slate)
  c.fillRoundRect(0, 0, W, H, 8, 0x2104);       // Outer card body (dark charcoal)
  c.fillRect(3, HALF, W - 6, HALF - 4, 0x18C3); // Bottom flap split shading

  char s[4];
  if (value < 0) strcpy(s, "--");
  else snprintf(s, sizeof(s), "%02d", value);

  c.setFreeFont(&FreeSans24pt7b);
  c.setTextSize(fontScale);                     // Dynamically scaled vector digits
  c.setTextColor(THEMES[currentTheme].digitColor); // Dynamic theme digit color (Amber, White, Green, etc.)
  c.setTextDatum(MC_DATUM);                     // Middle-Center alignment
  c.drawString(s, W / 2, H / 2);                // Perfectly centered on flap
}

void paint(int index, int before, int after, float progress) {
  card(oldCard, before);
  card(newCard, after);
  uint16_t *o = (uint16_t*)oldCard.getPointer();
  uint16_t *n = (uint16_t*)newCard.getPointer();
  uint16_t *f = (uint16_t*)frameSpr.getPointer();

  if (progress >= 1.0f) {
    memcpy(f, n, W * H * sizeof(uint16_t));
  } else {
    // Upper flap: reveals new card
    memcpy(f, n, W * HALF * sizeof(uint16_t));
    // Lower flap: holds old card until flap covers it
    memcpy(f + W * HALF, o + W * HALF, W * HALF * sizeof(uint16_t));

    // Perspective projection of rotating half-card around center hinge
    int height = max(1, (int)(fabsf(cosf(progress * PI)) * HALF));
    for (int row = 0; row < height; row++) {
      int dst, src;
      if (progress < 0.5f) {
        // First half: top half of oldCard folds downwards
        dst = HALF - height + row;
        src = row * HALF / height;
        memcpy(f + dst * W, o + src * W, W * sizeof(uint16_t));
      } else {
        // Second half: bottom half of newCard unfolds downwards
        dst = HALF + row;
        src = HALF + row * HALF / height;
        memcpy(f + dst * W, n + src * W, W * sizeof(uint16_t));
      }
    }
  }

  // Split flap horizontal seam
  frameSpr.drawFastHLine(0, HALF - 1, W, 0x0000);
  frameSpr.drawFastHLine(0, HALF, W, 0x0000);

  // Mechanical hinge pins on left and right
  frameSpr.fillRoundRect(0, HALF - hingePinH / 2, hingePinW, hingePinH, 1, 0x630C);
  frameSpr.fillRoundRect(W - hingePinW, HALF - hingePinH / 2, hingePinW, hingePinH, 1, 0x630C);

  // Blit frame to screen using native sprite push (automatically byte-swaps to big-endian RGB565)
  frameSpr.pushSprite(START_X + index * (W + GAP), TOP);
}

void flipTo(int *values) {
  bool changed = false;
  for (int i = 0; i < 3; i++) {
    if (shown[i] != values[i]) changed = true;
  }
  if (!changed) return;

  if (shown[0] < 0) {
    // Initial draw without animation
    for (int i = 0; i < 3; i++) {
      paint(i, values[i], values[i], 1.0f);
      shown[i] = values[i];
    }
    return;
  }

  // Realistic gravity-accelerated mechanical flap turnover
  int steps = 14;
  for (int step = 1; step <= steps; step++) {
    float progress = (float)step / (float)steps;
    for (int i = 0; i < 3; i++) {
      if (shown[i] != values[i]) {
        paint(i, shown[i], values[i], progress);
      }
    }
    delay(12);
  }

  for (int i = 0; i < 3; i++) {
    shown[i] = values[i];
  }
}

// ── 8. DYNAMIC LAYOUT CALCULATION ─────────────────────────────────────────────
void computeLayout() {
  SW = tft.width();
  SH = tft.height();

  if (SW >= 700) {
    // 800x480 ESP32-S3 Parallel RGB displays (4.3", 5.0", 7.0")
    W = 226; H = 220; GAP = 30;
    fontScale = 3;
    titleY = 20; titleTextSize = 2;
    labelY = 68; labelTextSize = 2;
    TOP = 115;
    dateY = 360; dateTextSize = 3;
    statusY1 = 412; statusY2 = 442; statusTextSize = 3;
    hingePinW = 6; hingePinH = 20;
  } else if (SW >= 450) {
    // 480x320 3.5" and 4.0" CYD (ST7796)
    W = 136; H = 140; GAP = 18;
    fontScale = 2;
    titleY = 16; titleTextSize = 2;
    labelY = 46; labelTextSize = 2;
    TOP = 74;
    dateY = 232; dateTextSize = 3;
    statusY1 = 274; statusY2 = 296; statusTextSize = 2;
    hingePinW = 4; hingePinH = 14;
  } else {
    // 320x240 2.4", 2.8", and 3.2" CYD (ILI9341 / ST7789)
    W = 92; H = 98; GAP = 10;
    fontScale = 1;
    titleY = 19; titleTextSize = 1;
    labelY = 43; labelTextSize = 1;
    TOP = 58;
    dateY = 182; dateTextSize = 2;
    statusY1 = 215; statusY2 = 228; statusTextSize = 1;
    hingePinW = 3; hingePinH = 10;
  }

  HALF = H / 2;
  START_X = (SW - (W * 3 + GAP * 2)) / 2;
}

void updateHoursLabel(int hour24) {
  int currentState = 0; // 0 = 24h, 1 = AM, 2 = PM
  if (!use24Hour) {
    currentState = (hour24 >= 12) ? 2 : 1;
  }
  if (currentState == lastAmPmShown) return;
  lastAmPmShown = currentState;

  String hLabel = "HOURS";
  if (!use24Hour) {
    hLabel += (currentState == 2) ? " [PM]" : " [AM]";
  }

  tft.setTextFont(0);
  tft.setTextSize(labelTextSize);
  tft.setTextColor(THEMES[currentTheme].labelColor, 0x0841); // Themed column label color
  tft.setTextDatum(TC_DATUM);
  int labelAreaW = W + GAP;
  int labelAreaX = START_X - GAP / 2;
  tft.fillRect(labelAreaX, labelY - 2, labelAreaW, (labelTextSize == 2) ? 22 : 14, 0x0841);
  tft.drawString(hLabel, START_X + W / 2, labelY);
}

void drawUI() {
  tft.fillScreen(0x0841);
  centered("F L I P   C L O C K", titleY, titleTextSize, 0xAD55);

  // Card Labels
  tft.setTextFont(0);
  tft.setTextSize(labelTextSize);
  tft.setTextColor(THEMES[currentTheme].labelColor, 0x0841);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("MINUTES", START_X + 1 * (W + GAP) + W / 2, labelY);
  tft.drawString("SECONDS", START_X + 2 * (W + GAP) + W / 2, labelY);
  lastAmPmShown = -1;
  updateHoursLabel(0);

  // Initial placeholder cards and subtle shelf accent lines
  for (int i = 0; i < 3; i++) {
    paint(i, -1, -1, 1.0f);
    int cardX = START_X + i * (W + GAP);
    for (int j = 0; j < 3; j++) {
      tft.drawFastHLine(cardX + 8, TOP + H + 4 + j * 3, W - 16, 0x3186);
    }
  }

  shown[0] = shown[1] = shown[2] = -1;
  lastDate = -1;
  lastStatusText = "";
}

// ── 8. NATIVE CAPTIVE PORTAL DNS & AP MANAGEMENT ──────────────────────────────
void processDNS() {
  if (!apModeActive) return;

  int packetSize = dnsUdp.parsePacket();
  if (packetSize > 0 && packetSize <= 512) {
    unsigned char buf[512];
    dnsUdp.read(buf, 512);

    // Validate standard DNS query format (QR bit = 0)
    if ((buf[2] & 0x80) == 0) {
      // Reconfigure the buffer to generate a spoofed A record response pointing to SoftAP IP
      buf[2] = 0x81;  // Response flags: Standard DNS response
      buf[3] = 0x80;  // Recursion available, no errors
      buf[7] = 0x01;  // Answer Record Count = 1

      // Navigate past the variable length question structure to place the answer block
      int pos = 12;  // Index offset past static 12-byte header
      while (pos < packetSize && buf[pos] != 0) {
        if (pos + buf[pos] + 1 >= packetSize) return;  // Guard against malformed string length overflow
        pos += buf[pos] + 1;                           // Walk query domain strings
      }
      pos += 5;  // Move past null terminator, QTYPE, and QCLASS records

      if (pos + 16 > 512) return;  // Guard against array overflow during response payload construction

      // Build the compression pointer indicating query name index 12
      buf[pos++] = 0xC0;
      buf[pos++] = 0x0C;

      // Setup structural DNS response details: Type A, Class IN
      buf[pos++] = 0x00;
      buf[pos++] = 0x01;
      buf[pos++] = 0x00;
      buf[pos++] = 0x01;

      // Set TTL to 60 seconds
      buf[pos++] = 0x00;
      buf[pos++] = 0x00;
      buf[pos++] = 0x00;
      buf[pos++] = 0x3C;

      // Set data segment length to 4 bytes for standard IPv4 values
      buf[pos++] = 0x00;
      buf[pos++] = 0x04;

      // Inject soft AP IP address as the destination
      IPAddress ip = WiFi.softAPIP();
      buf[pos++] = ip[0];
      buf[pos++] = ip[1];
      buf[pos++] = ip[2];
      buf[pos++] = ip[3];

      // Send response back to source socket address
      dnsUdp.beginPacket(dnsUdp.remoteIP(), dnsUdp.remotePort());
      dnsUdp.write(buf, pos);
      dnsUdp.endPacket();
    }
  }
}

void startAPMode() {
  if (apModeActive) return;
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_SSID, SETUP_PASSWORD);
  dnsUdp.begin(DNS_PORT);
  apModeActive = true;
  lastStationActiveTime = millis();
  lastStatusText = ""; // Force display status bar refresh immediately
  Serial.println(F("\n================------------------------"));
  Serial.println(F(" [NET] AP MODE STARTED (PORTAL READY)"));
  Serial.print(F(" [INFO] AP SSID    : ")); Serial.println(SETUP_SSID);
  Serial.print(F(" [INFO] AP IP      : ")); Serial.println(WiFi.softAPIP().toString());
  Serial.println(F("================------------------------\n"));
}

void stopAPMode() {
  if (!apModeActive) return;
  dnsUdp.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apModeActive = false;
  lastStatusText = "";
  Serial.println(F("[NET] AP Mode Closed (Station Mode Only)"));
}

void handleWifiScan() {
  int status = WiFi.scanComplete();
  if (status == -2) {
    // Scan not running: trigger asynchronous scan in background
    WiFi.scanNetworks(true);
    server.send(200, "application/json", "{\"status\":\"scanning\"}");
    return;
  }
  if (status == -1) {
    // Scan currently in progress
    server.send(200, "application/json", "{\"status\":\"scanning\"}");
    return;
  }
  // status >= 0: Scan complete!
  String json = "{\"status\":\"done\",\"nets\":[";
  for (int i = 0; i < status; i++) {
    if (i > 0) json += ",";
    json += "{\"s\":\"" + WiFi.SSID(i) + "\",\"r\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]}";
  WiFi.scanDelete(); // Free memory
  server.send(200, "application/json", json);
}

// ── 9. WEB CONTROL DASHBOARD HANDLERS ─────────────────────────────────────────
void handleRoot() {
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  char timeBuf[16];
  if (use24Hour) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", local.tm_hour, local.tm_min);
  } else {
    int h = local.tm_hour;
    const char* ampm = (h >= 12) ? "PM" : "AM";
    if (h == 0) h = 12;
    else if (h > 12) h -= 12;
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", h, local.tm_min, ampm);
  }
  char dateBuf[48];
  snprintf(dateBuf, sizeof(dateBuf), "%s   %02d / %02d / %04d", DAYS_OF_WEEK[local.tm_wday % 7], local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);

  String html;
  html.reserve(9000);
  html += F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>");
  html += F("<title>CYD Flip Clock Web Control</title><style>");
  html += F(":root{--bg:#0f1218;--card-bg:#1a1f29;--border:#2b3342;--accent:#00d2ff;--gold:#ffb84d;--text:#e6edf3;--muted:#8b949e;}");
  html += F("*{box-sizing:border-box;margin:0;padding:0;}body{background:var(--bg);color:var(--text);font-family:-apple-system,sans-serif;padding:16px;display:flex;justify-content:center;}");
  html += F(".wrap{width:100%;max-width:540px;}.hdr{text-align:center;padding:12px 0 20px;}.hdr h1{font-size:22px;letter-spacing:2px;color:var(--accent);text-transform:uppercase;}");
  html += F(".hdr p{color:var(--muted);font-size:13px;margin-top:4px;}");
  html += F(".toast-popup{position:fixed;top:20px;left:50%;transform:translateX(-50%);max-width:92%;width:380px;padding:12px 16px;border-radius:10px;box-shadow:0 8px 24px rgba(0,0,0,0.6);display:flex;align-items:center;justify-content:space-between;z-index:9999;font-size:14px;font-weight:600;animation:toastIn 0.3s ease-out;transition:opacity 0.4s,transform 0.4s;}");
  html += F(".toast-ok{background:#1b4332;border:1px solid #2d6a4f;color:#d8f3dc;}");
  html += F(".toast-info{background:#1e3a5f;border:1px solid #2b5288;color:#d0e1fd;}");
  html += F(".toast-close{background:none;border:none;color:inherit;font-size:18px;cursor:pointer;padding:0 0 0 12px;opacity:0.75;line-height:1;}");
  html += F(".toast-close:hover{opacity:1;}");
  html += F("@keyframes toastIn{from{opacity:0;transform:translate(-50%,-20px);}to{opacity:1;transform:translate(-50%,0);}}");
  html += F(".banner{background:linear-gradient(135deg,#151821,#1f2533);border:1px solid var(--border);border-radius:12px;padding:16px;text-align:center;margin-bottom:16px;}");
  html += F(".time{font-size:38px;font-weight:700;font-family:monospace;letter-spacing:3px;color:#fff;}.date{font-size:14px;color:var(--gold);margin-top:4px;font-weight:600;}");
  html += F(".card{background:var(--card-bg);border:1px solid var(--border);border-radius:12px;padding:18px;margin-bottom:16px;}");
  html += F(".title{font-size:15px;font-weight:600;color:var(--accent);margin-bottom:12px;}");
  html += F(".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;}.item{background:#11141c;padding:10px 12px;border-radius:8px;border:1px solid #232a36;}");
  html += F(".lbl{font-size:11px;text-transform:uppercase;color:var(--muted);}.val{font-size:13px;font-weight:600;margin-top:3px;word-break:break-all;}");
  html += F("label{display:block;font-size:13px;margin-bottom:6px;color:var(--text);}");
  html += F("select,input[type='text'],input[type='range']{width:100%;padding:10px 12px;background:#11141c;border:1px solid var(--border);border-radius:8px;color:#fff;font-size:14px;outline:none;}");
  html += F(".radios{display:flex;gap:16px;margin:10px 0 14px;}.rlbl{display:flex;align-items:center;gap:6px;font-size:14px;cursor:pointer;}");
  html += F(".slider-box{background:#11141c;padding:12px;border-radius:8px;border:1px solid #232a36;margin-top:10px;}");
  html += F(".slider-hdr{display:flex;justify-content:space-between;font-size:12px;color:var(--muted);margin-bottom:6px;}");
  html += F(".btn{width:100%;padding:12px;border-radius:8px;border:none;font-size:14px;font-weight:600;cursor:pointer;margin-top:10px;}");
  html += F(".btn-primary{background:#0077b6;color:#fff;}.btn-primary:hover{background:#0096c7;}");
  html += F(".btn-outline{background:transparent;border:1px solid var(--border);color:var(--text);}");
  html += F(".btn-danger{background:transparent;border:1px solid #782828;color:#ff6b6b;}");
  html += F(".notice{background:#162433;border-left:3px solid var(--accent);padding:10px;border-radius:0 6px 6px 0;font-size:12px;color:#b0c4de;margin-top:10px;line-height:1.4;}");
  html += F("</style></head><body><div class='wrap'>");
  html += F("<div class='hdr'><h1>UNIVERSAL FLIP CLOCK</h1><p>Web Control &amp; Configuration</p></div>");

  if (server.hasArg("saved")) {
    html += F("<div id='toast' class='toast-popup toast-ok'><span>💾 Settings saved &amp; applied successfully!</span><button type='button' class='toast-close' onclick='closeToast()'>✖</button></div>");
  } else if (server.hasArg("synced")) {
    html += F("<div id='toast' class='toast-popup toast-info'><span>🔄 Time synchronized with NTP server!</span><button type='button' class='toast-close' onclick='closeToast()'>✖</button></div>");
  }

  if (apModeActive) {
    html += F("<div class='notice' style='margin-bottom:16px;background:#2a1b00;border-left:3px solid #ffb84d;color:#ffe6b3;'><b>📶 Wi-Fi AP Setup Mode Active:</b> You are connected to the clock's setup Access Point. Select your home Wi-Fi network below, enter the password, and click Save to connect.</div>");
  }

  html += F("<div class='banner'><div class='time' id='liveTime'>");
  html += timeBuf;
  html += F("</div><div class='date' id='liveDate'>");
  html += dateBuf;
  html += F("</div></div>");

  html += F("<div class='card'><div class='title'>Device &amp; Status</div><div class='grid'>");
  html += F("<div class='item'><div class='lbl'>Wi-Fi Network</div><div class='val' id='sSSID'>");
  html += (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : (apModeActive ? String("AP: ") + SETUP_SSID : String("Offline"));
  html += F("</div></div><div class='item'><div class='lbl'>IP Address</div><div class='val' id='sIP'>");
  html += (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : (apModeActive ? WiFi.softAPIP().toString() : String("0.0.0.0"));
  html += F("</div></div><div class='item'><div class='lbl'>Hardware / Panel</div><div class='val'>");
  html += String(SW) + "x" + String(SH) + " (" + display.panelName() + ")";
  html += F("</div></div><div class='item'><div class='lbl'>LDR Light Sensor</div><div class='val' id='sLDR'>");
  html += String(lastLDRValue);
  html += F("</div></div><div class='item'><div class='lbl'>Screen Backlight</div><div class='val' id='sBL'>");
  html += String(activeBrightness);
  html += F(" / 255</div></div><div class='item'><div class='lbl'>Signal Strength</div><div class='val'>");
  html += String(WiFi.RSSI()) + " dBm";
  html += F("</div></div></div></div>");

  html += F("<form method='POST' action='/save' onsubmit='return validateForm();'>");

  html += F("<div class='card'><div class='title'>📶 Wi-Fi Network Setup</div>");
  html += F("<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px;'>");
  html += F("<label style='margin:0;'>Select Wi-Fi Network:</label>");
  html += F("<button type='button' id='btnScan' onclick='scanWifi()' style='font-size:12px;color:var(--accent);background:none;border:1px solid var(--border);border-radius:4px;padding:4px 10px;cursor:pointer;'>🔍 Scan for Networks</button>");
  html += F("</div>");
  html += F("<select id='ssidSel' name='ssid_sel' onchange='checkSSID(this);'>");
  html += "<option value=''>-- Keep Current (" + (savedSSID.length() > 0 ? savedSSID : "None (Enter Manually)") + ") --</option>";
  html += F("<option value='__man__'>Enter SSID Manually...</option></select>");
  html += F("<input type='text' id='ssidMan' name='ssid_man' placeholder='Type Network SSID' style='display:none;margin-top:10px;'>");
  html += F("<label style='margin-top:12px;'>Wi-Fi Password:</label>");
  html += F("<input type='password' id='wifiPass' name='pass' autocomplete='new-password' placeholder='Leave empty to keep current'>");
  html += F("<div style='margin-top:6px;display:flex;align-items:center;gap:6px;font-size:13px;color:var(--muted);'>");
  html += F("<input type='checkbox' id='showPass' onclick='togglePass();'><label for='showPass' style='margin:0;cursor:pointer;'>Show Wi-Fi Password</label></div></div>");

  html += F("<div class='card'><div class='title'>Timezone &amp; Automatic DST</div>");
  html += F("<label for='tzSelect'>Select World Timezone:</label><select id='tzSelect' onchange='document.getElementById(\"tzInput\").value=this.value;'>");
  for (int i = 0; i < NUM_TIMEZONES; i++) {
    html += "<option value='";
    html += TIMEZONES[i].posix;
    html += "'";
    if (currentTZ == TIMEZONES[i].posix) html += " selected";
    html += ">";
    html += TIMEZONES[i].label;
    html += "</option>";
  }
  html += F("</select><label for='tzInput' style='margin-top:12px;'>POSIX String (auto/custom):</label>");
  html += "<input type='text' id='tzInput' name='tz' value='" + currentTZ + "'>";
  html += F("<div class='notice'><b>Automatic DST:</b> Regions marked with <code>[DST]</code> handle Daylight Saving Time changes automatically. The clock shifts forward/backward on official change dates.</div></div>");

  html += F("<div class='card'><div class='title'>Display &amp; Orientation Settings</div><div class='grid'>");
  html += F("<div class='item'><div class='lbl'>Screen Flip</div><select name='screen_rot'>");
  html += "<option value='3'" + String(screenRotation == 3 ? " selected" : "") + ">Standard Landscape</option>";
  html += "<option value='1'" + String(screenRotation == 1 ? " selected" : "") + ">Flipped 180° Landscape</option>";
  html += F("</select></div><div class='item'><div class='lbl'>Time Format</div><select name='time_fmt'>");
  html += "<option value='24h'" + String(use24Hour ? " selected" : "") + ">24-Hour (00 - 23)</option>";
  html += "<option value='12h'" + String(!use24Hour ? " selected" : "") + ">12-Hour (01 - 12)</option>";
  html += F("</select></div><div class='item'><div class='lbl'>Color Theme</div><select name='theme_idx'>");
  for (int i = 0; i < NUM_THEMES; i++) {
    html += "<option value='" + String(i) + "'";
    if (currentTheme == i) html += " selected";
    html += ">" + String(THEMES[i].name) + "</option>";
  }
  html += F("</select></div><div class='item'><div class='lbl'>NTP Server</div>");
  html += "<input type='text' name='ntp_srv' value='" + ntpServer + "'></div></div></div>";

  html += F("<div class='card'><div class='title'>Screen Backlight Control</div><div class='radios'>");
  html += "<label class='rlbl'><input type='radio' name='auto_bl' value='1'" + String(autoBrightness ? " checked" : "") + " onchange='document.getElementById(\"sBox\").style.display=\"none\";'> Automatic (LDR Sensor)</label>";
  html += "<label class='rlbl'><input type='radio' name='auto_bl' value='0'" + String(!autoBrightness ? " checked" : "") + " onchange='document.getElementById(\"sBox\").style.display=\"block\";'> Manual Brightness</label></div>";
  html += "<div class='slider-box' id='sBox' style='display:" + String(autoBrightness ? "none" : "block") + ";'>";
  html += "<div class='slider-hdr'><span>Brightness Level</span><span id='sVal'>" + String(manualBrightness) + " / 255</span></div>";
  html += "<input type='range' id='blRng' name='manual_bl' min='20' max='255' value='" + String(manualBrightness) + "' oninput='liveBrightness(this.value);'></div></div>";

  html += F("<button type='submit' class='btn btn-primary'>💾 Save &amp; Apply Changes</button></form>");

  html += F("<div class='card' style='margin-top:16px;'><div class='title'>Quick Actions</div>");
  html += F("<form method='POST' action='/api/sync'><button type='submit' class='btn btn-outline'>🔄 Sync Time with NTP Now</button></form>");
  html += F("<form method='POST' action='/api/ap'><button type='submit' class='btn btn-outline' onclick='return confirm(\"Open Wi-Fi Access Point portal?\");'>📶 Enter Wi-Fi Setup (AP Mode)</button></form>");
  html += F("<form method='POST' action='/api/restart'><button type='submit' class='btn btn-danger' onclick='return confirm(\"Restart Clock now?\");'>🔁 Restart Device</button></form>");
  html += F("</div>");

  html += "<script>var g_savedSSID='" + savedSSID + "';";
  html += F("function closeToast(){var t=document.getElementById('toast');if(t){t.style.opacity='0';t.style.transform='translate(-50%,-20px)';setTimeout(function(){t.remove();},400);}}");
  html += F("if(document.getElementById('toast')){setTimeout(closeToast,3500);}");
  html += F("if(window.location.search){window.history.replaceState({},document.title,window.location.pathname);}");
  html += F("function togglePass(){var p=document.getElementById('wifiPass');p.type=(p.type==='password')?'text':'password';}");
  html += F("function checkSSID(s){var m=document.getElementById('ssidMan');if(s.value==='__man__'){m.style.display='block';m.required=true;m.focus();}else{m.style.display='none';m.required=false;}}");
  html += F("function scanWifi(){var b=document.getElementById('btnScan');b.innerText='⏳ Scanning...';b.disabled=true;fetch('/api/wifi_scan').then(r=>r.json()).then(d=>{if(d.status==='scanning'){setTimeout(pollWifi,1500);}else{fillNets(d.nets);}}).catch(()=>{b.innerText='🔍 Scan Failed (Retry)';b.disabled=false;});}");
  html += F("function pollWifi(){fetch('/api/wifi_scan').then(r=>r.json()).then(d=>{if(d.status==='done'&&d.nets){fillNets(d.nets);}else{setTimeout(pollWifi,1200);}}).catch(()=>{var b=document.getElementById('btnScan');b.innerText='🔍 Scan for Networks';b.disabled=false;});}");
  html += F("function fillNets(nets){var b=document.getElementById('btnScan');b.innerText='✅ Scan Complete';b.disabled=false;var sel=document.getElementById('ssidSel');var curText=(g_savedSSID.length>0)?g_savedSSID:'None (Enter Manually)';var h='<option value=\"\">-- Keep Current ('+curText+') --</option>';if(nets&&nets.length>0){for(var i=0;i<nets.length;i++){var n=nets[i];h+='<option value=\"'+n.s+'\">'+n.s+' ('+n.r+' dBm)</option>';}}h+='<option value=\"__man__\">Enter SSID Manually...</option>';sel.innerHTML=h;}");
  html += F("function validateForm(){var sSel=document.getElementById('ssidSel').value;var sMan=document.getElementById('ssidMan').value.trim();var pVal=document.getElementById('wifiPass').value;var hasSaved=(g_savedSSID.length>0);if(!hasSaved){var chosen=(sSel==='__man__')?sMan:sSel;if(!chosen){alert('⚠️ First Time Setup: Please select or enter a Wi-Fi SSID!');document.getElementById('ssidSel').focus();return false;}if(!pVal){alert('⚠️ First Time Setup: Please enter the Wi-Fi Password!');document.getElementById('wifiPass').focus();return false;}}else{if(sSel==='__man__'&&!sMan){alert('⚠️ Please enter the SSID name.');document.getElementById('ssidMan').focus();return false;}}return true;}");
  html += F("var blTimer=null;function liveBrightness(v){");
  html += F("document.getElementById('sVal').innerText=v+' / 255';");
  html += F("document.getElementById('sBL').innerText=v+' / 255';");
  html += F("clearTimeout(blTimer);blTimer=setTimeout(function(){");
  html += F("var fd=new FormData();fd.append('val',v);fetch('/api/brightness',{method:'POST',body:fd}).catch(()=>{});");
  html += F("},30);}");
  html += F("setInterval(function(){fetch('/api/status').then(r=>r.json()).then(d=>{");
  html += F("document.getElementById('liveTime').innerText=d.time;");
  html += F("document.getElementById('liveDate').innerText=d.date;");
  html += F("document.getElementById('sLDR').innerText=d.ldr;");
  html += F("if(document.querySelector('input[name=\"auto_bl\"]:checked').value==='1'){");
  html += F("document.getElementById('sBL').innerText=d.brightness+' / 255';");
  html += F("}});},2000);");
  html += F("</script></div></body></html>");

  server.send(200, "text/html", html);
}

void handleSave() {
  bool wifiChanged = false;
  String newSSID = "";
  if (server.hasArg("ssid_sel") && server.arg("ssid_sel") != "" && server.arg("ssid_sel") != "__man__") {
    newSSID = server.arg("ssid_sel");
  } else if (server.hasArg("ssid_man") && server.arg("ssid_man") != "") {
    newSSID = server.arg("ssid_man");
    newSSID.trim();
  }

  // Only consider Wi-Fi changed if a new different SSID was selected/entered
  if (newSSID.length() > 0 && newSSID != savedSSID) {
    savedSSID = newSSID;
    wifiChanged = true;
  }

  // Only consider Wi-Fi changed if a non-empty password was provided and differs from current
  if (server.hasArg("pass")) {
    String p = server.arg("pass");
    if (p.length() > 0 && p != savedPass) {
      savedPass = p;
      wifiChanged = true;
    }
  }

  if (server.hasArg("tz")) {
    String newTZ = server.arg("tz");
    newTZ.trim();
    if (newTZ.length() > 0) {
      currentTZ = newTZ;
      setenv("TZ", currentTZ.c_str(), 1);
      tzset();
    }
  }
  if (server.hasArg("auto_bl")) {
    autoBrightness = (server.arg("auto_bl") == "1");
  }
  if (server.hasArg("manual_bl")) {
    manualBrightness = constrain(server.arg("manual_bl").toInt(), 20, 255);
    if (!autoBrightness) {
      activeBrightness = manualBrightness;
      uint8_t bl = display.backlightPin();
      if (bl > 0) analogWrite(bl, activeBrightness);
      else display.setBacklight(activeBrightness > 20);
    }
  }
  if (server.hasArg("screen_rot")) {
    int newRot = server.arg("screen_rot").toInt();
    if (newRot == 1 || newRot == 3) {
      if (screenRotation != newRot) {
        screenRotation = newRot;
        tft.setRotation(screenRotation);
        tft.setSwapBytes(true);
        touch.setRotation(screenRotation);
        computeLayout();
        drawUI();
      }
    }
  }
  if (server.hasArg("time_fmt")) {
    use24Hour = (server.arg("time_fmt") == "24h");
  }
  if (server.hasArg("theme_idx")) {
    int newTheme = server.arg("theme_idx").toInt();
    if (newTheme >= 0 && newTheme < NUM_THEMES) {
      if (currentTheme != newTheme) {
        currentTheme = newTheme;
        shown[0] = shown[1] = shown[2] = -1;
        lastDate = -1;
        lastAmPmShown = -1;
        lastStatusText = "";
        drawUI();
      }
    }
  }
  if (server.hasArg("ntp_srv")) {
    String newNtp = server.arg("ntp_srv");
    newNtp.trim();
    if (newNtp.length() > 0 && newNtp != ntpServer) {
      ntpServer = newNtp;
      configTzTime(currentTZ.c_str(), ntpServer.c_str(), "time.google.com", "time.nist.gov");
    }
  }

  // Persist all settings to NVS flash
  prefs.begin("flipclock", false);
  prefs.putString("ssid", savedSSID);
  prefs.putString("pass", savedPass);
  prefs.putString("tz", currentTZ);
  prefs.putBool("auto_bl", autoBrightness);
  prefs.putInt("manual_bl", manualBrightness);
  prefs.putInt("rotation", screenRotation);
  prefs.putBool("use_24h", use24Hour);
  prefs.putInt("theme", currentTheme);
  prefs.putString("ntp", ntpServer);
  prefs.end();

  if (wifiChanged || apModeActive) {
    String html = F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
      "<meta http-equiv='refresh' content='7;url=/'>"
      "<title>Settings Saved</title><style>"
      "body{background:#0f1218;color:#e6edf3;font-family:-apple-system,sans-serif;padding:24px;text-align:center;display:flex;justify-content:center;align-items:center;min-height:90vh;margin:0;}"
      ".card{background:#1a1f29;border:1px solid #2b3342;border-radius:12px;padding:32px 24px;max-width:440px;box-shadow:0 8px 24px rgba(0,0,0,0.5);width:100%;box-sizing:border-box;}"
      "h2{color:#00d2ff;margin-bottom:12px;font-size:22px;}p{font-size:14px;color:#8b949e;line-height:1.6;margin-bottom:16px;}"
      ".badge{background:#11141c;border:1px solid #232a36;padding:8px 14px;border-radius:6px;font-family:monospace;color:#ffb84d;font-size:15px;display:inline-block;margin:6px 0;word-break:break-all;}"
      ".note{background:#162433;border-left:3px solid #00d2ff;padding:12px;text-align:left;font-size:13px;color:#b0c4de;border-radius:0 6px 6px 0;margin-top:16px;line-height:1.5;}"
      ".spin{display:inline-block;width:36px;height:36px;border:3px solid #2b3342;border-top-color:#00d2ff;border-radius:50%;animation:sp 1s linear infinite;margin:16px 0;}"
      ".btn-dash{display:inline-block;background:#0077b6;color:#fff;text-decoration:none;font-weight:600;font-size:14px;padding:12px 24px;border-radius:8px;margin-top:16px;transition:0.2s;}"
      ".btn-dash:hover{background:#0096c7;}"
      "@keyframes sp{to{transform:rotate(360deg);}}"
      "</style></head><body><div class='card'>"
      "<div class='spin'></div>"
      "<h2>💾 Settings Saved!</h2>");

    if (apModeActive) {
      html += F("<p>Connecting to Wi-Fi:<br><span class='badge'>");
      html += savedSSID.length() > 0 ? savedSSID : F("Configured Network");
      html += F("</span></p>"
        "<div class='note'>"
        "<b>What happens next:</b><br>"
        "1. The setup Access Point (<code>CYD-FlipClock</code>) will turn OFF.<br>"
        "2. The clock screen will show the newly assigned router IP in green.<br>"
        "3. Reconnect your phone/PC to your home Wi-Fi and open the Web Controller at the IP shown on screen."
        "</div>"
        "<div style='margin-top:16px;font-size:13px;color:#8b949e;'>Redirecting in <span id='cnt' style='color:#00d2ff;font-weight:bold;'>8</span>s...</div>"
        "<a href='/' class='btn-dash'>Open Clock Dashboard</a>");
    } else {
      html += F("<p>Wi-Fi configuration updated. Clock is reconnecting to:<br><span class='badge'>");
      html += savedSSID.length() > 0 ? savedSSID : F("Configured Network");
      html += F("</span></p>"
        "<p style='color:#a0aec0;'>Returning to dashboard in <span id='cnt' style='color:#00d2ff;font-weight:bold;'>6</span> seconds...</p>"
        "<a href='/' class='btn-dash'>Return to Dashboard Now</a>");
    }

    html += F("</div><script>"
      "var c=document.getElementById('cnt');var s=");
    html += apModeActive ? "8;" : "6;";
    html += F("var it=setInterval(function(){s--;if(c)c.innerText=s;if(s<=0){clearInterval(it);window.location.href='/?saved=1';}},1000);"
      "setTimeout(function(){var pIt=setInterval(function(){fetch('/api/status',{cache:'no-store'}).then(function(r){if(r.ok){clearInterval(pIt);clearInterval(it);window.location.href='/?saved=1';}}).catch(function(){});},1000);},3500);"
      "</script></body></html>");

    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
  } else {
    shown[0] = shown[1] = shown[2] = -1;
    lastDate = -1;
    lastAmPmShown = -1;
    lastStatusText = "";
    server.sendHeader("Location", "/?saved=1");
    server.send(303);
  }
}

void handleStatus() {
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  char timeBuf[16];
  if (use24Hour) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", local.tm_hour, local.tm_min);
  } else {
    int h = local.tm_hour;
    const char* ampm = (h >= 12) ? "PM" : "AM";
    if (h == 0) h = 12;
    else if (h > 12) h -= 12;
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", h, local.tm_min, ampm);
  }
  char dateBuf[48];
  snprintf(dateBuf, sizeof(dateBuf), "%s   %02d / %02d / %04d", DAYS_OF_WEEK[local.tm_wday % 7], local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);

  String json = "{";
  json += "\"time\":\"" + String(timeBuf) + "\",";
  json += "\"date\":\"" + String(dateBuf) + "\",";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"ldr\":" + String(lastLDRValue) + ",";
  json += "\"brightness\":" + String(activeBrightness) + ",";
  json += "\"auto_bl\":" + String(autoBrightness ? "true" : "false") + ",";
  json += "\"theme\":" + String(currentTheme) + ",";
  json += "\"tz\":\"" + currentTZ + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSync() {
  configTzTime(currentTZ.c_str(), ntpServer.c_str(), "time.google.com", "time.nist.gov");
  server.sendHeader("Location", "/?synced=1");
  server.send(303);
}

void handleEnterAP() {
  String html = F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<title>Entering Setup Mode</title><style>"
    "body{background:#0f1218;color:#e6edf3;font-family:-apple-system,sans-serif;padding:20px;display:flex;justify-content:center;align-items:center;min-height:90vh;margin:0;}"
    ".box{background:#1a1f29;border:1px solid #2b3342;border-radius:12px;padding:28px;max-width:440px;text-align:center;box-sizing:border-box;}"
    "h2{color:#00d2ff;margin-bottom:12px;font-size:22px;}p{font-size:14px;color:#8b949e;line-height:1.6;margin-bottom:16px;}"
    ".badge{background:#11141c;border:1px solid #232a36;padding:8px 14px;border-radius:6px;font-family:monospace;color:#ffb84d;font-size:14px;display:inline-block;margin:4px;}"
    ".note{font-size:12px;color:#6b7280;margin-top:16px;border-top:1px solid #232a36;padding-top:12px;}"
    "</style></head><body><div class='box'>"
    "<h2>📶 Entering Wi-Fi AP Mode</h2>"
    "<p>The clock has activated its setup Access Point.<br>Connect your phone or PC to:</p>"
    "<div><span class='badge'>SSID: CYD-FlipClock</span><span class='badge'>PW: flipclock24</span></div>"
    "<p style='margin-top:14px;'>Portal URL: <a href='http://192.168.4.1' style='color:#00d2ff;'>http://192.168.4.1</a></p>"
    "<div class='note'>If no device connects within 5 minutes, AP mode will automatically close and return to normal operation.</div>"
    "</div></body></html>");
  server.send(200, "text/html", html);
  delay(500);
  startAPMode();
}

void handleRestart() {
  String html = F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<meta http-equiv='refresh' content='6;url=/'>"
    "<title>Restarting Clock</title><style>"
    "body{background:#0f1218;color:#e6edf3;font-family:-apple-system,sans-serif;padding:20px;display:flex;justify-content:center;align-items:center;min-height:90vh;margin:0;}"
    ".box{background:#1a1f29;border:1px solid #2b3342;border-radius:12px;padding:32px;max-width:400px;text-align:center;box-sizing:border-box;}"
    "h2{color:#00d2ff;margin-bottom:12px;font-size:22px;}p{font-size:14px;color:#8b949e;line-height:1.5;}"
    ".spin{display:inline-block;width:34px;height:34px;border:3px solid #2b3342;border-top-color:#00d2ff;border-radius:50%;animation:sp 1s linear infinite;margin:18px 0;}"
    "@keyframes sp{to{transform:rotate(360deg);}}"
    "</style></head><body><div class='box'>"
    "<div class='spin'></div>"
    "<h2>Restarting Clock...</h2>"
    "<p>Please wait a moment.<br>You will be redirected back to the dashboard automatically.</p>"
    "</div></body></html>");
  server.send(200, "text/html", html);
  delay(1000);
  ESP.restart();
}

// ── 10. SETUP ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(100);

  // 1. Load persistent settings from NVS flash
  prefs.begin("flipclock", false);
  savedSSID        = prefs.getString("ssid", "");
  savedPass        = prefs.getString("pass", "");
  currentTZ        = prefs.getString("tz", "MMT-6:30");
  autoBrightness   = prefs.getBool("auto_bl", true);
  manualBrightness = prefs.getInt("manual_bl", 180);
  screenRotation   = prefs.getInt("rotation", 3);
  use24Hour        = prefs.getBool("use_24h", true);
  currentTheme     = prefs.getInt("theme", 0);
  if (currentTheme < 0 || currentTheme >= NUM_THEMES) currentTheme = 0;
  ntpServer        = prefs.getString("ntp", "pool.ntp.org");
  prefs.end();

  // Set system timezone with auto DST
  setenv("TZ", currentTZ.c_str(), 1);
  tzset();

  // 2. Initialize Hardware & Display
  pinMode(0, INPUT_PULLUP);                     // BOOT button
  display.begin(screenRotation);
  tft.init();
  tft.setRotation(screenRotation);
  tft.setSwapBytes(true);                       // Big-Endian RGB565 byte swap
  tft.fillScreen(0x0841);                       // Retro dark slate background

  // 3. Dynamic layout calculation based on detected panel
  computeLayout();

  // 4. Initialize LDR photoresistor and backlight PWM
  pinMode(LDR_PIN, INPUT);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);
  uint8_t bl = display.backlightPin();
  if (bl > 0) {
    activeBrightness = autoBrightness ? 128 : manualBrightness;
    analogWrite(bl, activeBrightness);
  } else {
    display.setBacklight(true);
  }

  // 5. Initialize touch controller
  touch.begin(display);
  touch.setRotation(screenRotation);
  touch.loadCalibration();

  // 6. Allocate off-screen sprite buffers
  if (!oldCard.createSprite(W, H) || !newCard.createSprite(W, H) || !frameSpr.createSprite(W, H)) {
    centered("ERROR: Not enough memory", SH / 2, 2, 0xF800);
    while (true) delay(1000);
  }

  // 7. Draw Initial UI
  drawUI();

  // Initial placeholders
  centered("Waiting for time", dateY, (dateTextSize > 2) ? 2 : 1, THEMES[currentTheme].dateColor);
  renderStatusRow("Connecting to Wi-Fi...", "", 0x8410);

  // 8. Network Initialization (Native AP / Station Mode)
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  if (savedSSID.length() == 0) {
    startAPMode();
  } else {
    WiFi.mode(WIFI_STA);
    apModeActive = false;
    Serial.print(F("Connecting to WiFi: ")); Serial.println(savedSSID);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    unsigned long startWait = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startWait < 5000) {
      delay(100);
      adjustBrightness();
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("[WIFI] Connected Successfully!"));
      Serial.print(F("[WIFI] IP: ")); Serial.println(WiFi.localIP().toString());
    } else {
      Serial.println(F("[WIFI] Connection timed out (Offline Mode). Background reconnecting..."));
    }
  }

  // 9. Register Web Control routes (Single WebServer for both AP and Station mode)
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/sync", HTTP_POST, handleSync);
  server.on("/api/ap", HTTP_POST, handleEnterAP);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/wifi_scan", HTTP_GET, handleWifiScan);
  server.on("/api/brightness", HTTP_POST, []() {
    if (server.hasArg("val")) {
      int b = constrain(server.arg("val").toInt(), 20, 255);
      manualBrightness = b;
      activeBrightness = b;
      autoBrightness = false;
      uint8_t blPin = display.backlightPin();
      if (blPin > 0) analogWrite(blPin, activeBrightness);
      else display.setBacklight(activeBrightness > 20);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing val");
    }
  });

  server.onNotFound([]() {
    // Captive portal 302 redirect to root
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirect");
  });

  MDNS.begin("flipclock");
  MDNS.addService("http", "tcp", 80);
  server.begin();
  serverStarted = true;

  Serial.println("\n=======================================================");
  Serial.printf(" UNIVERSAL CYD FLIP CLOCK READY (%dx%d)\n", SW, SH);
  Serial.printf(" Detected Hardware: %s | %s\n", display.modelName(), display.panelName());
  Serial.printf(" Active Timezone: %s\n", currentTZ.c_str());
  Serial.println(" Set USB time: T<Unix seconds> then newline.");
  Serial.println(" Hold BOOT for 15s to enter Wi-Fi AP setup mode.");
  Serial.println(" LDR auto-backlight enabled on GPIO 34.");
  Serial.println(" Web Control available once connected to Wi-Fi or in AP mode.");
  Serial.println("=======================================================\n");
}

// ── 11. MAIN LOOP ─────────────────────────────────────────────────────────────
void loop() {
  adjustBrightness();

  // 1. AP Mode Management
  if (apModeActive) {
    processDNS();
    int stations = WiFi.softAPgetStationNum();
    if (stations > 0) lastStationActiveTime = millis();

    // Auto-timeout: if Wi-Fi credentials exist and no client connects for 5 minutes, turn off AP mode
    if (savedSSID.length() > 0) {
      if (millis() - lastStationActiveTime > 300000UL) {
        Serial.println(F("[NET] AP Mode Closed (Timeout)"));
        stopAPMode();
      }
    }
  }

  // 2. Web Control Server handling (Single server handles both AP mode & normal mode)
  server.handleClient();

  // 3. Background Wi-Fi Station Reconnect (if disconnected and credentials exist)
  static unsigned long lastWifiAttempt = 0;
  if (!apModeActive && savedSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiAttempt > 30000UL) {
      lastWifiAttempt = millis();
      Serial.println(F("[WIFI] Offline. Background reconnecting..."));
      WiFi.disconnect();
      WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    }
  }

  // ── Hold BOOT Button OR Touch Screen for 15s to enter AP Mode ─────────────
  static unsigned long pressedAt = 0;
  static bool handled = false;
  static int lastSecShown = -1;

  uint16_t tx = 0, ty = 0;
  bool touchPressed = touch.read(tx, ty);
  static unsigned long touchDebounceStart = 0;
  bool touchActive = false;
  if (touchPressed) {
    if (!touchDebounceStart) touchDebounceStart = millis();
    if (millis() - touchDebounceStart > 250) touchActive = true;
  } else {
    touchDebounceStart = 0;
  }

  bool bootActive  = (digitalRead(0) == LOW);
  bool isTriggered = (bootActive || touchActive);

  if (isTriggered) {
    if (!pressedAt) {
      pressedAt = millis();
      lastSecShown = -1;
    }
    unsigned long elapsed = millis() - pressedAt;
    int remainingSec = 15 - (int)(elapsed / 1000);
    if (remainingSec < 0) remainingSec = 0;

    // Live on-screen countdown feedback while holding BOOT or Touch
    if (remainingSec != lastSecShown) {
      lastSecShown = remainingSec;
      char countBuf[64];
      const char* triggerSource = bootActive ? "BOOT" : "Touch";
      if (remainingSec > 0) {
        snprintf(countBuf, sizeof(countBuf), "Hold %s for AP: %ds...", triggerSource, remainingSec);
      } else {
        snprintf(countBuf, sizeof(countBuf), "RELEASING TO START AP MODE!");
      }
      renderStatusRow(countBuf, "", 0xFFE0); // Bright yellow text with auto-scaling
    }

    if (!handled && elapsed >= 15000) {
      handled = true;
      if (!apModeActive) {
        startAPMode();
      }
    }
  } else {
    if (pressedAt != 0 && !handled) {
      // Released early: cancelled
      lastStatusText = ""; // Force status row to refresh immediately
    }
    pressedAt = 0;
    handled = false;
    lastSecShown = -1;
  }

  // Handle USB Serial manual time input (e.g. T1761998400)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (serialLine.startsWith("T")) {
        time_t t = strtoll(serialLine.c_str() + 1, nullptr, 10);
        if (t > 1700000000LL && t < 4102444800LL) {
          timeval tv = {t, 0};
          settimeofday(&tv, nullptr);
          Serial.println("TIME SET OK");
        }
      }
      serialLine = "";
    } else if (c != '\r' && serialLine.length() < 40) {
      serialLine += c;
    }
  }

  // Start NTP once connected to Wi-Fi
  if (WiFi.status() == WL_CONNECTED && !networkTimeStarted) {
    configTzTime(currentTZ.c_str(), ntpServer.c_str(), "time.google.com", "time.nist.gov");
    networkTimeStarted = true;
  }

  // Update clock when valid time is present
  time_t now = time(nullptr);
  bool valid = (now > 1700000000LL);
  if (valid) {
    struct tm local;
    localtime_r(&now, &local);
    int h = local.tm_hour;
    if (!use24Hour) {
      if (h == 0) h = 12;
      else if (h > 12) h -= 12;
    }
    int values[] = {h, local.tm_min, local.tm_sec};
    flipTo(values);
    updateHoursLabel(local.tm_hour);

    int dateKey = local.tm_year * 400 + local.tm_yday;
    if (lastDate != dateKey) {
      const char* dayStr = DAYS_OF_WEEK[local.tm_wday % 7];
      char date[48];
      snprintf(date, sizeof(date), "%s   %02d / %02d / %04d", dayStr, local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);
      tft.fillRect(0, dateY - 4, SW, (dateTextSize == 3) ? 34 : 26, 0x0841);
      centeredFit(date, dateY, dateTextSize, THEMES[currentTheme].dateColor);
      lastDate = dateKey;
    }
  }

  // Periodic bottom status readout (Redraws ONLY when text changes - ZERO FLICKER)
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 500 || lastStatusText == "") {
    lastStatus = millis();
    String line1 = "";
    String line2 = "";
    uint16_t color1 = 0x8410;
    uint16_t color2 = 0x8410;

    if (apModeActive) {
      char apInfo[64];
      snprintf(apInfo, sizeof(apInfo), "AP: %s (PW: %s)", SETUP_SSID, SETUP_PASSWORD);
      line1 = String(apInfo);
      line2 = "Setup URL: 192.168.4.1";
      color1 = 0x8410;
      color2 = 0x8410;
    } else if (WiFi.status() != WL_CONNECTED) {
      line1 = "Offline - Hold BOOT or Touch for AP";
      line2 = "";
      color1 = 0x8410;
    } else if (!valid) {
      line1 = "Syncing with NTP server...";
      line2 = "";
      color1 = 0x8410;
    } else {
      String ssid = WiFi.SSID();
      String ip = WiFi.localIP().toString();
      String combined = "WiFi: " + ssid + " | IP: " + ip;

      // Check if combined string fits comfortably at target font size
      int targetSize = (SW >= 700) ? 3 : ((SW >= 450) ? 2 : 1);
      tft.setTextFont(0);
      tft.setTextSize(targetSize);
      int maxW = (SW > 0 ? SW : tft.width()) - 16;

      if (tft.textWidth(combined.c_str()) <= maxW) {
        // Fits on a single line at full target size!
        line1 = combined;
        line2 = "";
      } else {
        // Exceeds width at full target size: cleanly split into two lines so neither shrinks!
        line1 = "WiFi: " + ssid;
        line2 = "IP: " + ip;
      }
      color1 = THEMES[currentTheme].statusColor;
      color2 = THEMES[currentTheme].statusColor;
    }

    String statusKey = line1 + "##" + line2;
    if (statusKey != lastStatusText) {
      lastStatusText = statusKey;
      renderStatusRow(line1, line2, color1, color2);
    }
  }

  delay(5);
}
