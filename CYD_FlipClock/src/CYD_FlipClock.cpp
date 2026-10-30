// CYD Flip Clock 1.1.0 - ESP32-2432S028R / ILI9341
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSans24pt7b.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include <sys/time.h>
#include <Preferences.h> // Library for non-volatile storage

constexpr char SETUP_SSID[] = "CYD-FlipClock";
constexpr char SETUP_PASSWORD[] = "flipclock24";

// Default settings if NVS is empty
char tz_string[64] = "CST6CDT,M3.2.0,M11.1.0"; // Default POSIX TZ
char is_24h_str[4] = "1";                      // "1" = 24h, "0" = 12h
char brightness_str[4] = "200";                // 0 to 255
WiFiManagerParameter custom_tz("tz_id", "POSIX Timezone String", tz_string, 64);
WiFiManagerParameter custom_24h("is_24h_id", "Format (1=24h, 0=12h)", is_24h_str, 4);
WiFiManagerParameter custom_bright("bright_id", "Backlight (10-255)", brightness_str, 4);

SPIClass displaySPI(HSPI);
Adafruit_ILI9341 lcd(&displaySPI, 2, 15, -1);
WiFiManager wifi;
Preferences preferences;

constexpr int W = 92, H = 100, HALF = 50, TOP = 58;
GFXcanvas16 oldCard(W, H), newCard(W, H), frame(W, H);
int shown[3] = {-1, -1, -1};
int lastDate = -1;
bool networkTimeStarted = false;
String serialLine;

// Global settings converted from text strings
bool use24Hour = true;
int currentBrightness = 255;

void setBacklight(int duty)
{
  analogWrite(21, duty); // Pin 21 controls backlight PWM on the CYD
}

void loadSettings()
{
  preferences.begin("clock_settings", false);
  preferences.getString("tz", tz_string, sizeof(tz_string));
  preferences.getString("is_24h", is_24h_str, sizeof(is_24h_str));
  preferences.getString("bright", brightness_str, sizeof(brightness_str));
  preferences.end();

  use24Hour = (String(is_24h_str) == "1");
  currentBrightness = atoi(brightness_str);
  if (currentBrightness < 10)
    currentBrightness = 10; // Keep screen visible
  if (currentBrightness > 255)
    currentBrightness = 255;
}

void saveSettingsCallback()
{
  Serial.println("Settings saving via Web interface...");
  preferences.begin("clock_settings", false);
  preferences.putString("tz", custom_tz.getValue());
  preferences.putString("is_24h", custom_24h.getValue());
  preferences.putString("bright", custom_bright.getValue());
  preferences.end();

  // Reload active configs
  loadSettings();

  // Dynamically apply environment configurations
  setenv("TZ", tz_string, 1);
  tzset();
  setBacklight(currentBrightness);
  networkTimeStarted = false; // Trigger re-sync with NTP under new TZ rules
}

void centered(const char *s, int y, int size, uint16_t color)
{
  lcd.setFont();
  lcd.setTextSize(size);
  lcd.setTextColor(color);
  lcd.setCursor((325 - (int)strlen(s) * 6 * size) / 2, y);
  lcd.print(s);
}

void card(GFXcanvas16 &c, int value)
{
  c.fillScreen(0x0841);
  c.fillRoundRect(0, 0, W, H, 7, 0x2104);
  c.fillRect(3, HALF, W - 6, HALF - 4, 0x18C3);
  char s[4];
  if (value < 0)
    strcpy(s, "--");
  else
    snprintf(s, sizeof(s), "%02d", value);
  c.setFont(&FreeSans24pt7b);
  c.setTextSize(1, 2);
  c.setTextColor(0xFDE0);
  int16_t x, y;
  uint16_t w, h;
  c.getTextBounds(s, 0, 0, &x, &y, &w, &h);
  c.setCursor((W - w) / 2 - x, (H - h) / 2 - y);
  c.print(s);
}

void paint(int index, int before, int after, float progress)
{
  card(oldCard, before);
  card(newCard, after);
  uint16_t *o = oldCard.getBuffer(), *n = newCard.getBuffer(), *f = frame.getBuffer();
  if (progress >= 1)
    memcpy(f, n, W * H * 2);
  else
  {
    memcpy(f, n, W * HALF * 2);
    memcpy(f + W * HALF, o + W * HALF, W * HALF * 2);
    int height = max(1, (int)(fabsf(cosf(progress * PI)) * HALF));
    for (int row = 0; row < height; row++)
    {
      int dst, src;
      if (progress < 0.5f)
      {
        dst = HALF - height + row;
        src = row * HALF / height;
      }
      else
      {
        dst = HALF + row;
        src = HALF + row * HALF / height;
      }
      memcpy(f + dst * W, (progress < 0.5f ? o : n) + src * W, W * 2);
    }
  }
  frame.drawFastHLine(0, HALF - 1, W, 0);
  frame.drawFastHLine(0, HALF, W, 0);
  frame.fillRoundRect(0, HALF - 5, 3, 10, 1, 0x630C);
  frame.fillRoundRect(W - 3, HALF - 5, 3, 10, 1, 0x630C);
  lcd.drawRGBBitmap(12 + index * 102, TOP, f, W, H);
}

void flipTo(int *values)
{
  bool changed = false;
  for (int i = 0; i < 3; i++)
    if (shown[i] != values[i])
      changed = true;
  if (!changed)
    return;
  if (shown[0] < 0)
    for (int i = 0; i < 3; i++)
      paint(i, values[i], values[i], 1);
  else
  {
    unsigned long start = millis();
    float p;
    do
    {
      p = min(1.0f, (millis() - start) / 340.0f);
      for (int i = 0; i < 3; i++)
        if (shown[i] != values[i])
          paint(i, shown[i], values[i], p);
      delay(1);
    } while (p < 1);
  }
  for (int i = 0; i < 3; i++)
    shown[i] = values[i];
}

void setup()
{
  Serial.begin(115200);
  setCpuFrequencyMhz(80);
  pinMode(0, INPUT_PULLUP);

  // Configure Brightness pin (PWM)
  pinMode(21, OUTPUT);

  // Load config from NVS
  loadSettings();
  setBacklight(currentBrightness);

  for (int p : {4, 16, 17})
  {
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
  }
  displaySPI.begin(14, 12, 13, 15);
  lcd.begin(40000000);
  lcd.setRotation(1);
  lcd.fillScreen(0x0841);
  if (!oldCard.getBuffer() || !newCard.getBuffer() || !frame.getBuffer())
  {
    centered("Not enough memory", 100, 1, 0xFFFF);
    while (true)
      delay(1000);
  }
  centered("F L I P C L O C K", 19, 1, 0xAD55);
  lcd.setTextSize(1);
  lcd.setTextColor(0x8410);
  lcd.setCursor(43, 43);
  lcd.print("HOURS");
  lcd.setCursor(137, 43);
  lcd.print("MINUTES");
  lcd.setCursor(239, 43);
  lcd.print("SECONDS");
  for (int i = 0; i < 3; i++)
  {
    paint(i, -1, -1, 1);
    for (int j = 0; j < 3; j++)
      lcd.drawFastHLine(16 + i * 102, 160 + j * 3, 84, 0x3186);
  }
  centered("Waiting for time", 182, 1, 0xFFFF);
  centered("Connecting to Wi-Fi...", 215, 1, 0x8410);

  setenv("TZ", tz_string, 1);
  tzset();

  // Attach custom setup fields to web layout
  wifi.addParameter(&custom_tz);
  wifi.addParameter(&custom_24h);
  wifi.addParameter(&custom_bright);

  wifi.setSaveParamsCallback(saveSettingsCallback);
  wifi.setDebugOutput(false);
  wifi.setConfigPortalBlocking(false);
  wifi.setConnectTimeout(8);
  wifi.setWiFiAutoReconnect(true);
  wifi.autoConnect(SETUP_SSID, SETUP_PASSWORD);

  Serial.println("FLIP CLOCKREADY. Hold BOOT pin 3s to alter settings via Web UI.");
}

void loop()
{
  wifi.process();

  static unsigned long pressedAt = 0;
  static bool handled = false;
  if (digitalRead(0) == LOW)
  {
    if (!pressedAt)
      pressedAt = millis();
    if (!handled && millis() - pressedAt >= 3000)
    {
      handled = true;
      if (!wifi.getConfigPortalActive())
        wifi.startConfigPortal(SETUP_SSID, SETUP_PASSWORD);
    }
  }
  else
  {
    pressedAt = 0;
    handled = false;
  }

  while (Serial.available())
  {
    char c = Serial.read();
    if (c == '\n')
    {
      if (serialLine.startsWith("T"))
      {
        time_t t = strtoll(serialLine.c_str() + 1, nullptr, 10);
        if (t > 1700000000LL && t < 4102444800LL)
        {
          timeval tv = {t, 0};
          settimeofday(&tv, nullptr);
          Serial.println("TIME SET OK");
        }
      }
      serialLine = "";
    }
    else if (c != '\r' && serialLine.length() < 40)
      serialLine += c;
  }

  if (WiFi.status() == WL_CONNECTED && !networkTimeStarted)
  {
    configTzTime(tz_string, "pool.ntp.org", "time.nist.gov");
    networkTimeStarted = true;
  }

  time_t now = time(nullptr);
  bool valid = now > 1700000000LL;
  if (valid)
  {
    struct tm local;
    localtime_r(&now, &local);

    // 12/24 hour display mapping adaptation
    int visualHour = local.tm_hour;
    if (!use24Hour)
    {
      visualHour = visualHour % 12;
      if (visualHour == 0)
        visualHour = 12;
    }

    int values[] = {visualHour, local.tm_min, local.tm_sec};
    flipTo(values);

    int dateKey = local.tm_year * 400 + local.tm_yday;
    if (lastDate != dateKey)
    {
      char date[24];
      snprintf(date, sizeof(date), "%d %d %d", local.tm_mon + 1, local.tm_mday, local.tm_year + 1900);
      lcd.fillRect(0, 181, 320, 66, 0x0841);
      centered(date, 182, 4, 0x532E);
      lastDate = dateKey;
    }
  }

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 2000)
  {
    lastStatus = millis();
    lcd.fillRect(0, 213, 320, 27, 0x0841);
    if (wifi.getConfigPortalActive())
    {
      centered("Wi-Fi: CYD-FlipClock / flipclock24", 215, 1, 0x8410);
      centered("Setup: 192.168.4.1", 228, 1, 0x8410);
    }
    else if (WiFi.status() != WL_CONNECTED)
      centered("Offline - hold BOOT for Wi-Fi setup", 215, 1, 0x8410);
    else if (!valid)
      centered("Synchronizing time...", 215, 1, 0x8410);
  }
  delay(5);
}
