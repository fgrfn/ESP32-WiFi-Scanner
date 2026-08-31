#include <FS.h>
using fs::FS;

#include <SPI.h>
#include <SD.h>
#include "tft_setup.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <WebServer.h>
#include <time.h>

// ESP32-2432S028R / CYD pins
#define TOUCH_SCLK 25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32
#define TOUCH_IRQ 36
#define SD_SCLK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5
#define BACKLIGHT_PIN 21
#define BUTTON_PIN 0
#define LED_RED 4
#define LED_GREEN 16
#define LED_BLUE 17

constexpr int W = 240;
constexpr int H = 320;
constexpr int HEADER_H = 25;
constexpr int FOOTER_Y = 288;
constexpr int ROW_H = 51;
constexpr int ROWS = 5;
constexpr int MAX_NETS = 30;
constexpr int HISTORY = 45;

TFT_eSPI tft;
SPIClass auxSPI(VSPI);  // Touch normally; temporarily switched to SD pins for logging.
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
Preferences prefs;
WebServer web(80);

enum View { LIST, CHANNELS, SECURITY, DETAILS, MESH_APS, SETTINGS, LANGUAGE, CALIBRATION };

struct Net {
  String ssid;
  String bssid;
  int32_t rssi;
  uint8_t channel;
  uint8_t auth;
};

struct Group {
  String ssid;
  int best;
  int count;
};

Net nets[MAX_NETS];
Group groups[MAX_NETS];
int netCount = 0;
int groupCount = 0;
int page = 0;
int meshPage = 0;
int settingsPage = 0;
View view = LIST;
View returnView = LIST;

bool scanning = false;
bool paused = false;
bool showHidden = true;
bool groupSsids = true;
bool lightTheme = false;
bool english = false;
bool languageSelected = false;
bool sdLogging = false;
bool sdMounted = false;
bool webEnabled = false;
bool webRunning = false;
bool webRoutesReady = false;
uint8_t brightness = 191;
uint32_t scanMs = 5000;
uint32_t nextScan = 0;
uint32_t lastScanFinished = 0;
uint32_t resetArmedUntil = 0;
uint32_t lastMarqueeAt = 0;
uint32_t lastHeaderAt = 0;
uint16_t marqueeOffset = 0;

String selectedBssid;
String selectedSsid;
int selected = -1;
int8_t history[HISTORY];
uint8_t historyChannels[HISTORY];
int historyCount = 0;
long statSum = 0;
int statSamples = 0;
int statMin = 0;
int statMax = -100;
int outages = 0;
bool previouslyVisible = true;
int lastSelectedChannel = 0;
int channelChanges = 0;
uint16_t historySaveCounter = 0;

bool timeSynced = false;
uint64_t epochAtSync = 0;
uint32_t millisAtSync = 0;
int timezoneOffsetMinutes = 0;

int touchLeft = 200;
int touchRight = 3700;
int touchTop = 240;
int touchBottom = 3800;
int calibrationStep = 0;
TS_Point calibrationFirst;
bool touchCalibrated = false;
bool firstRunCalibration = false;

bool fingerDown = false;
int downX = 0;
int downY = 0;
int lastTouchX = 0;
int lastTouchY = 0;
bool buttonWasDown = false;
uint32_t lastInput = 0;

uint16_t bg() { return lightTheme ? TFT_WHITE : TFT_BLACK; }
uint16_t fg() { return lightTheme ? TFT_BLACK : TFT_WHITE; }
uint16_t panel() { return lightTheme ? 0xC618 : 0x1082; }
uint16_t muted() { return lightTheme ? TFT_DARKGREY : TFT_LIGHTGREY; }
const char *tr(const char *de, const char *en) { return english ? en : de; }

int brightnessPercent() {
  if (brightness < 96) return 25;
  if (brightness < 160) return 50;
  if (brightness < 224) return 75;
  return 100;
}

uint8_t brightnessFromPercent(int percent) {
  if (percent < 38) return 64;
  if (percent < 63) return 128;
  if (percent < 88) return 191;
  return 255;
}

String authName(uint8_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN: return tr("OFFEN", "OPEN");
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA+";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    default: return tr("GES.", "SEC");
  }
}

String authCode(uint8_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA+";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    default: return "SEC";
  }
}

int quality(int rssi) {
  if (rssi <= -100) return 0;
  if (rssi >= -50) return 100;
  return 2 * (rssi + 100);
}

uint16_t strengthColor(int rssi) {
  if (rssi >= -67) return TFT_GREEN;
  if (rssi >= -80) return TFT_YELLOW;
  return TFT_RED;
}

String clipText(String s, int n) {
  if ((int)s.length() <= n) return s;
  return s.substring(0, n - 1) + ".";
}

String vendorName(const String &bssid) {
  String oui = bssid.substring(0, 8);
  oui.toUpperCase();
  struct Vendor { const char *oui; const char *name; };
  static const Vendor vendors[] = {
    {"00:1A:2A", "Cisco"}, {"00:1D:7E", "Cisco"}, {"3C:84:6A", "TP-Link"},
    {"50:C7:BF", "TP-Link"}, {"D8:47:32", "TP-Link"}, {"E8:48:B8", "TP-Link"},
    {"34:31:C4", "AVM"}, {"3C:A6:2F", "AVM"}, {"44:4E:6D", "AVM"},
    {"CC:CE:1E", "AVM"}, {"DC:15:C8", "AVM"}, {"2C:3A:FD", "AVM"},
    {"18:E8:29", "Ubiquiti"}, {"24:5A:4C", "Ubiquiti"}, {"74:83:C2", "Ubiquiti"},
    {"F0:9F:C2", "Ubiquiti"}, {"44:D9:E7", "Ubiquiti"}, {"B4:FB:E4", "Ubiquiti"},
    {"18:31:BF", "ASUS"}, {"2C:FD:A1", "ASUS"}, {"50:46:5D", "ASUS"},
    {"A0:36:BC", "ASUS"}, {"AC:9E:17", "ASUS"}, {"04:D4:C4", "Netgear"},
    {"20:E5:2A", "Netgear"}, {"A0:40:A0", "Netgear"}, {"C4:04:15", "Netgear"},
    {"28:CF:E9", "Apple"}, {"3C:15:C2", "Apple"}, {"F0:18:98", "Apple"},
    {"B8:27:EB", "Raspberry Pi"}, {"DC:A6:32", "Raspberry Pi"}, {"E4:5F:01", "Raspberry Pi"},
    {"24:0A:C4", "Espressif"}, {"24:6F:28", "Espressif"}, {"30:AE:A4", "Espressif"},
    {"A4:CF:12", "Espressif"}, {"C8:C9:A3", "Espressif"}, {"EC:FA:BC", "Espressif"},
    {"00:17:88", "Philips Hue"}, {"EC:B5:FA", "Philips Hue"},
    {"F4:F5:D8", "Google"}, {"54:60:09", "Google"}, {"44:07:0B", "Google"},
    {"00:FC:8B", "Amazon"}, {"40:B4:CD", "Amazon"}, {"FC:65:DE", "Amazon"}
  };
  for (const Vendor &v : vendors) if (oui == v.oui) return v.name;
  return tr("Unbekannt", "Unknown");
}

uint64_t currentEpoch() {
  if (!timeSynced) return 0;
  return epochAtSync + (uint32_t)(millis() - millisAtSync) / 1000ULL;
}

String timestampText() {
  uint64_t epoch = currentEpoch();
  if (!epoch) return "U+" + String(millis() / 1000);
  time_t local = (time_t)(epoch - (int64_t)timezoneOffsetMinutes * 60);
  struct tm tmValue;
  gmtime_r(&local, &tmValue);
  char text[24];
  strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%S", &tmValue);
  return String(text);
}

void setLed(bool red, bool green, bool blue) {
  digitalWrite(LED_RED, red ? LOW : HIGH);
  digitalWrite(LED_GREEN, green ? LOW : HIGH);
  digitalWrite(LED_BLUE, blue ? LOW : HIGH);
}

bool hasOpenNetwork() {
  for (int i = 0; i < netCount; ++i) if (nets[i].auth == WIFI_AUTH_OPEN) return true;
  return false;
}

void updateLed() {
  if (paused) setLed(true, false, false);
  else if (scanning) setLed(false, false, true);
  else if (hasOpenNetwork()) setLed(true, true, false);
  else setLed(false, true, false);
}

void applyBrightness() {
  analogWrite(BACKLIGHT_PIN, brightness);
}

void saveSettings() {
  prefs.putUInt("scan", scanMs);
  prefs.putBool("hidden", showHidden);
  prefs.putBool("groups", groupSsids);
  prefs.putBool("light", lightTheme);
  prefs.putBool("english", english);
  prefs.putBool("langSet", languageSelected);
  prefs.putBool("sd", sdLogging);
  prefs.putBool("web", webEnabled);
  prefs.putUChar("bright", brightness);
  prefs.putInt("tx0", touchLeft);
  prefs.putInt("tx1", touchRight);
  prefs.putInt("ty0", touchTop);
  prefs.putInt("ty1", touchBottom);
  prefs.putBool("touchCal", touchCalibrated);
  prefs.putInt("tz", timezoneOffsetMinutes);
}

void loadSettings() {
  scanMs = prefs.getUInt("scan", 5000);
  showHidden = prefs.getBool("hidden", true);
  groupSsids = prefs.getBool("groups", true);
  lightTheme = prefs.getBool("light", false);
  english = prefs.getBool("english", false);
  languageSelected = prefs.getBool("langSet", false);
  sdLogging = prefs.getBool("sd", false);
  webEnabled = prefs.getBool("web", false);
  brightness = brightnessFromPercent((prefs.getUChar("bright", 191) * 100 + 127) / 255);
  touchLeft = prefs.getInt("tx0", 200);
  touchRight = prefs.getInt("tx1", 3700);
  touchTop = prefs.getInt("ty0", 240);
  touchBottom = prefs.getInt("ty1", 3800);
  touchCalibrated = prefs.getBool("touchCal", false);
  timezoneOffsetMinutes = prefs.getInt("tz", 0);
}

void saveMeasurementState() {
  if (!selectedBssid.length()) return;
  prefs.putString("selBssid", selectedBssid);
  prefs.putString("selSsid", selectedSsid);
  prefs.putBytes("hist", history, sizeof(history));
  prefs.putBytes("histCh", historyChannels, sizeof(historyChannels));
  prefs.putInt("histCount", historyCount);
  prefs.putInt("statSum", (int)statSum);
  prefs.putInt("statN", statSamples);
  prefs.putInt("statMin", statMin);
  prefs.putInt("statMax", statMax);
  prefs.putInt("outages", outages);
  prefs.putInt("chChanges", channelChanges);
  prefs.putInt("lastCh", lastSelectedChannel);
  historySaveCounter = 0;
}

void loadMeasurementState() {
  selectedBssid = prefs.getString("selBssid", "");
  selectedSsid = prefs.getString("selSsid", "");
  if (!selectedBssid.length()) return;
  prefs.getBytes("hist", history, sizeof(history));
  prefs.getBytes("histCh", historyChannels, sizeof(historyChannels));
  historyCount = constrain(prefs.getInt("histCount", 0), 0, HISTORY);
  statSum = prefs.getInt("statSum", 0);
  statSamples = prefs.getInt("statN", 0);
  statMin = prefs.getInt("statMin", 0);
  statMax = prefs.getInt("statMax", -100);
  outages = prefs.getInt("outages", 0);
  channelChanges = prefs.getInt("chChanges", 0);
  lastSelectedChannel = prefs.getInt("lastCh", 0);
}

void drawButton(int x, const String &label, bool active = false) {
  uint16_t c = active ? TFT_BLUE : panel();
  tft.fillRect(x, FOOTER_Y, 80, 32, c);
  tft.drawRect(x, FOOTER_Y, 80, 32, muted());
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(active ? TFT_WHITE : fg(), c);
  tft.drawString(label, x + 40, FOOTER_Y + 16);
}

void drawHeader() {
  tft.fillRect(0, 0, W, HEADER_H, panel());
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg(), panel());
  tft.drawString("ESP32 WiFi Scan", 5, 12);
  uint16_t status = paused ? TFT_RED : (scanning ? TFT_YELLOW : TFT_GREEN);
  tft.fillCircle(151, 12, 5, status);
  tft.setTextDatum(MR_DATUM);
  tft.setTextFont(1);
  uint32_t age = lastScanFinished ? (millis() - lastScanFinished) / 1000 : 0;
  String statusText = String(netCount);
  if (!scanning && lastScanFinished) statusText += "/" + String(min((uint32_t)99, age)) + "s";
  tft.drawString(statusText, 198, 12);
  uint16_t setBg = view == SETTINGS ? TFT_BLUE : bg();
  tft.fillRect(203, 2, 35, 21, setBg);
  tft.drawRect(203, 2, 35, 21, muted());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(view == SETTINGS ? TFT_WHITE : fg(), setBg);
  tft.drawString("SET", 220, 12);
}

int itemCount() { return groupSsids ? groupCount : netCount; }

void standardFooter() {
  int pages = max(1, (itemCount() + ROWS - 1) / ROWS);
  String p = "LIST";
  if (pages > 1) p += " " + String(page + 1) + "/" + String(pages);
  drawButton(0, p, view == LIST);
  drawButton(80, view == CHANNELS ? tr("SICHER", "SECURITY") : tr("KANAL", "CHANNEL"),
             view == CHANNELS || view == SECURITY);
  drawButton(160, paused ? "START" : "PAUSE", paused);
}

void buildGroups() {
  groupCount = 0;
  for (int i = 0; i < netCount; ++i) {
    String key = nets[i].ssid.length() ? nets[i].ssid : "<Hidden> " + nets[i].bssid;
    int found = -1;
    for (int g = 0; g < groupCount; ++g) if (groups[g].ssid == key) { found = g; break; }
    if (found < 0) {
      groups[groupCount].ssid = key;
      groups[groupCount].best = i;
      groups[groupCount].count = 1;
      groupCount++;
    } else {
      groups[found].count++;
      if (nets[i].rssi > nets[groups[found].best].rssi) groups[found].best = i;
    }
  }
}

int listNetIndex(int item) {
  return groupSsids ? groups[item].best : item;
}

String marqueeText(String text, int width) {
  if ((int)text.length() <= width) return text;
  String loop = text + "   ";
  String result;
  int start = marqueeOffset % loop.length();
  for (int i = 0; i < width; ++i) result += loop[(start + i) % loop.length()];
  return result;
}

void drawLockIcon(int x, int y, bool open, uint16_t color, uint16_t background) {
  tft.fillRect(x, y + 6, 9, 7, color);
  tft.drawRect(x + (open ? 4 : 2), y + 1, 6, 7, color);
  if (open) tft.fillRect(x, y + 1, 5, 6, background);
}

void drawWifiIcon(int x, int y, uint16_t color, uint16_t background) {
  tft.drawCircle(x, y, 7, color);
  tft.fillRect(x - 8, y - 8, 17, 8, background);
  tft.drawCircle(x, y, 4, color);
  tft.fillRect(x - 5, y - 5, 11, 5, background);
  tft.fillCircle(x, y, 1, color);
}

void drawVisibleNames() {
  if (view != LIST || itemCount() == 0) return;
  int first = page * ROWS;
  int last = min(first + ROWS, itemCount());
  for (int item = first; item < last; ++item) {
    int row = item - first;
    int y = HEADER_H + row * ROW_H;
    int ni = listNetIndex(item);
    uint16_t rowBg = row % 2 ? panel() : bg();
    String name = groupSsids ? groups[item].ssid : (nets[ni].ssid.length() ? nets[ni].ssid : "<Hidden>");
    int chars = groupSsids ? 13 : 17;
    int pixels = groupSsids ? 108 : 150;
    tft.fillRect(5, y + 2, pixels, 18, rowBg);
    tft.setTextFont(2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(nets[ni].auth == WIFI_AUTH_OPEN ? TFT_GREEN : fg(), rowBg);
    tft.drawString(marqueeText(name, chars), 5, y + 3);
  }
}

void drawList() {
  tft.fillRect(0, HEADER_H, W, FOOTER_Y - HEADER_H, bg());
  drawHeader();
  if (itemCount() == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_YELLOW, bg());
    tft.drawString(scanning ? tr("SCAN LAEUFT", "SCANNING") : tr("KEINE NETZE", "NO NETWORKS"), 120, 135);
    tft.setTextFont(2);
    tft.setTextColor(muted(), bg());
    tft.drawString(tr("Ergebnisse erscheinen automatisch", "Results appear automatically"), 120, 170);
    standardFooter();
    return;
  }
  int pages = max(1, (itemCount() + ROWS - 1) / ROWS);
  if (page >= pages) page = 0;
  int first = page * ROWS;
  int last = min(first + ROWS, itemCount());
  for (int item = first; item < last; ++item) {
    int row = item - first;
    int y = HEADER_H + row * ROW_H;
    int ni = listNetIndex(item);
    Net &n = nets[ni];
    uint16_t rowBg = row % 2 ? panel() : bg();
    tft.fillRect(0, y, W, ROW_H, rowBg);
    tft.setTextFont(2);
    if (groupSsids && groups[item].count > 1) {
      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(TFT_CYAN, rowBg);
      tft.drawString(String(groups[item].count) + " AP", 159, y + 3);
    }
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(strengthColor(n.rssi), rowBg);
    tft.drawString(String(n.rssi) + " dBm", 235, y + 3);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(n.auth == WIFI_AUTH_OPEN ? TFT_RED : TFT_YELLOW, rowBg);
    drawLockIcon(6, y + 21, n.auth == WIFI_AUTH_OPEN, n.auth == WIFI_AUTH_OPEN ? TFT_RED : TFT_YELLOW, rowBg);
    tft.drawString(authName(n.auth), 20, y + 21);
    tft.setTextColor(TFT_CYAN, rowBg);
    drawWifiIcon(70, y + 31, TFT_CYAN, rowBg);
    tft.drawString("CH " + String(n.channel), 78, y + 21);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(strengthColor(n.rssi), rowBg);
    tft.drawString(String(quality(n.rssi)) + "%", 235, y + 21);
    tft.drawRect(5, y + 41, 230, 7, muted());
    tft.fillRect(6, y + 42, map(quality(n.rssi), 0, 100, 0, 228), 5, strengthColor(n.rssi));
  }
  drawVisibleNames();
  standardFooter();
}

int channelScore(int candidate) {
  static const int overlap[5] = {5, 4, 3, 2, 1};
  int score = 0;
  for (int i = 0; i < netCount; ++i) {
    int d = abs(candidate - (int)nets[i].channel);
    if (d <= 4) score += quality(nets[i].rssi) * overlap[d];
  }
  return score;
}

int bestChannel() {
  int candidates[3] = {1, 6, 11};
  int best = 1;
  int score = 2147483647;
  for (int c : candidates) {
    int s = channelScore(c);
    if (s < score) { score = s; best = c; }
  }
  return best;
}

void drawChannels() {
  tft.fillRect(0, HEADER_H, W, FOOTER_Y - HEADER_H, bg());
  drawHeader();
  int counts[14] = {0};
  int load[14] = {0};
  for (int i = 0; i < netCount; ++i) if (nets[i].channel >= 1 && nets[i].channel <= 13) {
    counts[nets[i].channel]++;
    load[nets[i].channel] += quality(nets[i].rssi);
  }
  int maxLoad = 1;
  for (int c = 1; c <= 13; ++c) maxLoad = max(maxLoad, load[c]);
  int recommended = bestChannel();
  tft.setTextFont(2);
  for (int c = 1; c <= 13; ++c) {
    int y = 29 + (c - 1) * 18;
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(c == recommended ? TFT_GREEN : fg(), bg());
    tft.drawString(String(c), 22, y + 7);
    int width = map(load[c], 0, maxLoad, 0, 175);
    uint16_t bar = load[c] < maxLoad / 3 ? TFT_GREEN : (load[c] < maxLoad * 2 / 3 ? TFT_YELLOW : TFT_RED);
    tft.drawRect(29, y + 2, 177, 12, muted());
    if (width) tft.fillRect(30, y + 3, width, 10, bar);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(c == recommended ? TFT_GREEN : muted(), bg());
    tft.drawString(String(counts[c]), 213, y + 7);
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, bg());
  tft.drawString(String(tr("Empfehlung: Kanal ", "Recommended: channel ")) + String(recommended), 130, 276);
  standardFooter();
}

void securityBar(int y, const String &name, int count, uint16_t color) {
  int width = netCount ? map(count, 0, netCount, 0, 125) : 0;
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg(), bg());
  tft.drawString(name, 10, y + 10);
  tft.drawRect(78, y + 3, 127, 15, muted());
  if (width) tft.fillRect(79, y + 4, width, 13, color);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, bg());
  tft.drawString(String(count), 214, y + 10);
}

void drawSecurity() {
  tft.fillRect(0, HEADER_H, W, FOOTER_Y - HEADER_H, bg());
  drawHeader();
  int open = 0, wep = 0, wpa = 0, wpa2 = 0, wpa3 = 0;
  for (int i = 0; i < netCount; ++i) {
    switch (nets[i].auth) {
      case WIFI_AUTH_OPEN: open++; break;
      case WIFI_AUTH_WEP: wep++; break;
      case WIFI_AUTH_WPA_PSK: case WIFI_AUTH_WPA_WPA2_PSK: wpa++; break;
      case WIFI_AUTH_WPA3_PSK: wpa3++; break;
      default: wpa2++; break;
    }
  }
  int secureScore = netCount ? ((wpa2 + wpa3) * 100 / netCount) : 0;
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(secureScore >= 75 ? TFT_GREEN : (secureScore >= 50 ? TFT_YELLOW : TFT_RED), bg());
  tft.drawString("Security " + String(secureScore) + "%", 120, 48);
  securityBar(83, "WPA3", wpa3, TFT_GREEN);
  securityBar(116, "WPA2", wpa2, TFT_CYAN);
  securityBar(149, "WPA", wpa, TFT_YELLOW);
  securityBar(182, "WEP", wep, TFT_ORANGE);
  securityBar(215, "OFFEN", open, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(open ? TFT_RED : TFT_GREEN, bg());
  tft.drawString(open ? String(open) + tr(" offene Netze gefunden", " open networks found")
                      : tr("Keine offenen Netze", "No open networks"), 120, 270);
  standardFooter();
}

int findSelected() {
  for (int i = 0; i < netCount; ++i) if (nets[i].bssid == selectedBssid) return i;
  return -1;
}

int selectedApCount() {
  if (!selectedSsid.length()) return 1;
  int count = 0;
  for (int i = 0; i < netCount; ++i) if (nets[i].ssid == selectedSsid) count++;
  return max(1, count);
}

void addMeasurement(bool visible, int rssi = -100, int channel = 0) {
  int value = visible ? constrain(rssi, -100, -30) : -100;
  if (historyCount < HISTORY) {
    history[historyCount] = value;
    historyChannels[historyCount] = visible ? channel : 0;
    historyCount++;
  }
  else {
    for (int i = 1; i < HISTORY; ++i) {
      history[i - 1] = history[i];
      historyChannels[i - 1] = historyChannels[i];
    }
    history[HISTORY - 1] = value;
    historyChannels[HISTORY - 1] = visible ? channel : 0;
  }
  if (visible) {
    if (lastSelectedChannel && channel && channel != lastSelectedChannel) channelChanges++;
    if (channel) lastSelectedChannel = channel;
    statSum += rssi;
    statSamples++;
    if (statSamples == 1) statMin = statMax = rssi;
    else { statMin = min(statMin, rssi); statMax = max(statMax, rssi); }
  } else if (previouslyVisible) outages++;
  previouslyVisible = visible;
  if (++historySaveCounter >= 60) saveMeasurementState();
}

void resetMeasurement(int rssi, int channel) {
  historyCount = statSamples = outages = 0;
  statSum = 0;
  statMin = 0;
  statMax = -100;
  channelChanges = 0;
  lastSelectedChannel = channel;
  previouslyVisible = true;
  addMeasurement(true, rssi, channel);
}

void drawGraph() {
  int x = 8, y = 183, width = 224, height = 92;
  tft.drawRect(x, y, width, height, muted());
  for (int r = -40; r >= -100; r -= 20) {
    int py = map(r, -30, -100, y + 3, y + height - 3);
    tft.drawFastHLine(x + 1, py, width - 2, panel());
  }
  for (int i = 1; i < historyCount; ++i) {
    int x1 = map(i - 1, 0, HISTORY - 1, x + 3, x + width - 4);
    int x2 = map(i, 0, HISTORY - 1, x + 3, x + width - 4);
    int y1 = map(history[i - 1], -30, -100, y + 3, y + height - 3);
    int y2 = map(history[i], -30, -100, y + 3, y + height - 3);
    tft.drawLine(x1, y1, x2, y2, strengthColor(history[i]));
  }
}

void drawDetails() {
  tft.fillRect(0, HEADER_H, W, FOOTER_Y - HEADER_H, bg());
  drawHeader();
  selected = findSelected();
  bool visible = selected >= 0;
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(visible ? fg() : TFT_RED, bg());
  tft.drawString(clipText(selectedSsid.length() ? selectedSsid : "<Hidden>", 15), 8, 31);
  tft.setTextFont(2);
  if (visible) {
    Net &n = nets[selected];
    tft.setTextColor(muted(), bg());
    tft.drawString(String(tr("Hersteller: ", "Vendor: ")) + vendorName(n.bssid), 8, 61);
    tft.drawString(n.bssid, 8, 81);
    tft.drawString("CH " + String(n.channel) + "  " + authName(n.auth) + "  " + String(selectedApCount()) + " AP", 8, 101);
    tft.setTextColor(strengthColor(n.rssi), bg());
    tft.drawString(String(n.rssi) + " dBm / " + String(quality(n.rssi)) + "%", 8, 121);
  } else {
    tft.setTextColor(TFT_RED, bg());
    tft.drawString(tr("Momentan nicht sichtbar", "Currently not visible"), 8, 72);
  }
  int average = statSamples ? statSum / statSamples : -100;
  tft.setTextColor(muted(), bg());
  tft.drawString("Min " + String(statMin) + "  Max " + String(statMax) + "  Avg " + String(average), 8, 143);
  tft.drawString(String(tr("Mess ", "Samples ")) + String(statSamples) + tr("  Aus ", "  Out ") +
                 String(outages) + tr("  CH-Wechsel ", "  CH changes ") + String(channelChanges), 8, 163);
  drawGraph();
  drawButton(0, tr("ZURUECK", "BACK"));
  drawButton(80, tr("AP-LISTE", "AP LIST"));
  drawButton(160, "RESET");
}

int meshItemCount() {
  int count = 0;
  for (int i = 0; i < netCount; ++i) {
    if (selectedSsid.length() ? nets[i].ssid == selectedSsid : nets[i].bssid == selectedBssid) count++;
  }
  return count;
}

int meshNetIndex(int target) {
  int count = 0;
  for (int i = 0; i < netCount; ++i) {
    if (selectedSsid.length() ? nets[i].ssid == selectedSsid : nets[i].bssid == selectedBssid) {
      if (count++ == target) return i;
    }
  }
  return -1;
}

void drawMeshAps() {
  tft.fillRect(0, HEADER_H, W, FOOTER_Y - HEADER_H, bg());
  drawHeader();
  int count = meshItemCount();
  int pages = max(1, (count + ROWS - 1) / ROWS);
  if (meshPage >= pages) meshPage = 0;
  int first = meshPage * ROWS;
  int last = min(first + ROWS, count);
  for (int item = first; item < last; ++item) {
    int ni = meshNetIndex(item);
    if (ni < 0) continue;
    int row = item - first;
    int y = HEADER_H + row * ROW_H;
    uint16_t rowBg = row % 2 ? panel() : bg();
    tft.fillRect(0, y, W, ROW_H, rowBg);
    tft.setTextFont(2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(nets[ni].bssid == selectedBssid ? TFT_GREEN : fg(), rowBg);
    tft.drawString(nets[ni].bssid, 5, y + 3);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(strengthColor(nets[ni].rssi), rowBg);
    tft.drawString(String(nets[ni].rssi) + " dBm", 235, y + 3);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(muted(), rowBg);
    tft.drawString(vendorName(nets[ni].bssid), 5, y + 23);
    tft.setTextColor(TFT_CYAN, rowBg);
    tft.drawString("CH " + String(nets[ni].channel), 112, y + 23);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(nets[ni].auth == WIFI_AUTH_OPEN ? TFT_RED : TFT_YELLOW, rowBg);
    tft.drawString(authName(nets[ni].auth), 235, y + 23);
  }
  drawButton(0, tr("ZURUECK", "BACK"));
  drawButton(80, pages > 1 ? String(meshPage + 1) + "/" + String(pages) : "APs " + String(count));
  drawButton(160, paused ? "START" : "PAUSE", paused);
}

void settingRow(int index, const String &name, const String &value, bool on = false) {
  int y = 28 + index * 32;
  uint16_t c = index % 2 ? panel() : bg();
  tft.fillRect(0, y, W, 31, c);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg(), c);
  tft.drawString(name, 7, y + 15);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(on ? TFT_GREEN : muted(), c);
  tft.drawString(value, 233, y + 15);
}

void drawSettings() {
  tft.fillRect(0, HEADER_H, W, FOOTER_Y - HEADER_H, bg());
  drawHeader();
  if (settingsPage == 0) {
    settingRow(0, tr("Scanintervall", "Scan interval"), String(scanMs / 1000) + " s");
    settingRow(1, tr("SSID-Gruppen", "SSID groups"), groupSsids ? tr("AN", "ON") : tr("AUS", "OFF"), groupSsids);
    settingRow(2, tr("Hidden Netze", "Hidden networks"), showHidden ? tr("AN", "ON") : tr("AUS", "OFF"), showHidden);
    settingRow(3, tr("Helligkeit", "Brightness"), String(brightnessPercent()) + "%");
    settingRow(4, tr("Farbschema", "Color theme"), lightTheme ? tr("HELL", "LIGHT") : tr("DUNKEL", "DARK"), !lightTheme);
    settingRow(5, "SD CSV-Log", sdLogging ? tr("AN", "ON") : tr("AUS", "OFF"), sdLogging && sdMounted);
    settingRow(6, "Web-Dashboard", webEnabled ? tr("AN", "ON") : tr("AUS", "OFF"), webRunning);
    settingRow(7, "Touch", tr("KALIBRIEREN", "CALIBRATE"));
  } else {
    uint32_t age = lastScanFinished ? (millis() - lastScanFinished) / 1000 : 0;
    settingRow(0, tr("Uhrzeit", "Clock"), timeSynced ? timestampText().substring(11) : tr("Browser oeffnen", "Open browser"), timeSynced);
    settingRow(1, tr("Scan-Alter", "Scan age"), String(age) + " s");
    settingRow(2, tr("Verlauf", "History"), String(historyCount) + tr(" Werte", " values"));
    settingRow(3, tr("Kanalwechsel", "Channel changes"), String(channelChanges));
    settingRow(4, tr("Sprache", "Language"), english ? "ENGLISH" : "DEUTSCH", true);
    settingRow(5, tr("Werkseinstellungen", "Factory settings"), millis() < resetArmedUntil ? tr("NOCHMAL", "AGAIN") : "RESET", millis() < resetArmedUntil);
    settingRow(6, tr("Web-Adresse", "Web address"), webRunning ? "192.168.4.1" : tr("AUS", "OFF"), webRunning);
    settingRow(7, "Firmware", "V2.1");
  }
  drawButton(0, tr("ZURUECK", "BACK"));
  drawButton(80, settingsPage == 0 ? tr("MEHR 1/2", "MORE 1/2") : tr("MEHR 2/2", "MORE 2/2"), true);
  drawButton(160, paused ? "START" : "PAUSE", paused);
}

void drawLanguage() {
  tft.fillScreen(bg());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg(), bg());
  tft.setTextFont(4);
  tft.drawString("Sprache / Language", 120, 72);
  tft.setTextFont(2);
  tft.setTextColor(muted(), bg());
  tft.drawString("Bitte waehlen / Please select", 120, 108);
  tft.fillRoundRect(12, 142, 103, 92, 8, TFT_BLUE);
  tft.fillRoundRect(125, 142, 103, 92, 8, TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(4);
  tft.drawString("DE", 63, 177);
  tft.drawString("EN", 176, 177);
  tft.setTextFont(2);
  tft.drawString("Deutsch", 63, 213);
  tft.drawString("English", 176, 213);
}

void drawCalibration() {
  tft.fillScreen(bg());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg(), bg());
  tft.setTextFont(4);
  tft.drawString(tr("Touch-Kalibrierung", "Touch calibration"), 120, 130);
  tft.setTextFont(2);
  tft.drawString(calibrationStep == 0 ? tr("Kreuz oben links beruehren", "Touch the upper-left cross")
                                      : tr("Kreuz unten rechts beruehren", "Touch the lower-right cross"), 120, 165);
  int x = calibrationStep == 0 ? 20 : 219;
  int y = calibrationStep == 0 ? 20 : 299;
  tft.drawCircle(x, y, 8, TFT_RED);
  tft.drawFastHLine(x - 12, y, 25, TFT_RED);
  tft.drawFastVLine(x, y - 12, 25, TFT_RED);
}

void render() {
  switch (view) {
    case LIST: drawList(); break;
    case CHANNELS: drawChannels(); break;
    case SECURITY: drawSecurity(); break;
    case DETAILS: drawDetails(); break;
    case MESH_APS: drawMeshAps(); break;
    case SETTINGS: drawSettings(); break;
    case LANGUAGE: drawLanguage(); break;
    case CALIBRATION: drawCalibration(); break;
  }
}

void switchToSdBus() {
  digitalWrite(TOUCH_CS, HIGH);
  auxSPI.end();
  auxSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
}

void switchToTouchBus() {
  digitalWrite(SD_CS, HIGH);
  auxSPI.end();
  auxSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
}

bool ensureSd() {
  switchToSdBus();
  if (!sdMounted) sdMounted = SD.begin(SD_CS, auxSPI, 10000000);
  if (sdMounted && !SD.exists("/wifi-scanner.csv")) {
    File file = SD.open("/wifi-scanner.csv", FILE_WRITE);
    if (file) { file.println("timestamp,ssid,bssid,vendor,channel,rssi,quality,security"); file.close(); }
  }
  if (sdMounted && !SD.exists("/wifi-history.csv")) {
    File file = SD.open("/wifi-history.csv", FILE_WRITE);
    if (file) { file.println("timestamp,ssid,bssid,channel,rssi,visible,channel_changes,outages"); file.close(); }
  }
  switchToTouchBus();
  return sdMounted;
}

void logToSd() {
  if (!sdLogging || !ensureSd()) return;
  switchToSdBus();
  File file = SD.open("/wifi-scanner.csv", FILE_APPEND);
  if (file) {
    for (int i = 0; i < netCount; ++i) {
      String ssid = nets[i].ssid;
      ssid.replace(",", "_");
      file.printf("%s,%s,%s,%s,%u,%d,%d,%s\n", timestampText().c_str(), ssid.c_str(), nets[i].bssid.c_str(),
                  vendorName(nets[i].bssid).c_str(), nets[i].channel, static_cast<int>(nets[i].rssi),
                  quality(nets[i].rssi), authCode(nets[i].auth).c_str());
    }
    file.close();
  }
  if (selectedBssid.length()) {
    File historyFile = SD.open("/wifi-history.csv", FILE_APPEND);
    if (historyFile) {
      int current = findSelected();
      String historySsid = selectedSsid; historySsid.replace(",", "_");
      historyFile.printf("%s,%s,%s,%d,%d,%d,%d,%d\n", timestampText().c_str(), historySsid.c_str(),
                         selectedBssid.c_str(), current >= 0 ? nets[current].channel : 0,
                         current >= 0 ? static_cast<int>(nets[current].rssi) : -100, current >= 0,
                         channelChanges, outages);
      historyFile.close();
    }
  }
  switchToTouchBus();
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  return s;
}

String networksJson() {
  String out;
  out.reserve(6500);
  uint32_t age = lastScanFinished ? (millis() - lastScanFinished) / 1000 : 0;
  out = "{\"suggestedChannel\":" + String(bestChannel()) + ",\"scanAge\":" + String(age) +
        ",\"timeSynced\":" + String(timeSynced ? "true" : "false") +
        ",\"settings\":{\"scan\":" + String(scanMs) + ",\"groups\":" + String(groupSsids ? "true" : "false") +
        ",\"hidden\":" + String(showHidden ? "true" : "false") + ",\"light\":" +
        String(lightTheme ? "true" : "false") + ",\"sd\":" + String(sdLogging ? "true" : "false") +
        ",\"brightness\":" + String(brightnessPercent()) + ",\"language\":\"" +
        String(english ? "en" : "de") + "\"},\"networks\":[";
  for (int i = 0; i < netCount; ++i) {
    if (i) out += ',';
    out += "{\"ssid\":\"" + jsonEscape(nets[i].ssid.length() ? nets[i].ssid : "<Hidden>") + "\",";
    out += "\"bssid\":\"" + nets[i].bssid + "\",\"vendor\":\"" + vendorName(nets[i].bssid) +
           "\",\"channel\":" + String(nets[i].channel) + ',';
    out += "\"rssi\":" + String(nets[i].rssi) + ",\"quality\":" + String(quality(nets[i].rssi)) + ',';
    out += "\"security\":\"" + authCode(nets[i].auth) + "\"}";
  }
  out += "]}";
  return out;
}

String currentCsv() {
  String out = "timestamp,ssid,bssid,vendor,channel,rssi,quality,security\n";
  out.reserve(4500);
  for (int i = 0; i < netCount; ++i) {
    String ssid = nets[i].ssid; ssid.replace(",", "_");
    out += timestampText() + ',' + ssid + ',' + nets[i].bssid + ',' + vendorName(nets[i].bssid) + ',' +
           String(nets[i].channel) + ',' + String(nets[i].rssi) + ',' + String(quality(nets[i].rssi)) + ',' +
           authCode(nets[i].auth) + '\n';
  }
  return out;
}

String historyCsv() {
  String out = "sample,ssid,bssid,channel,rssi\n";
  out.reserve(2500);
  for (int i = 0; i < historyCount; ++i) {
    out += String(i + 1) + ',' + selectedSsid + ',' + selectedBssid + ',' + String(historyChannels[i]) + ',' +
           String(history[i]) + '\n';
  }
  return out;
}

const char DASHBOARD[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>ESP32 WiFi Scanner</title><style>
body{font-family:system-ui;background:#0c1117;color:#e8eef5;margin:18px}.box{max-width:1000px;margin:auto}h1,h2{color:#48d597}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px}.card,table{background:#151d27;border:1px solid #33404d;border-radius:8px;padding:12px}table{width:100%;border-collapse:collapse;padding:0}th,td{padding:8px;border-bottom:1px solid #33404d;text-align:left}.bar{height:9px;background:#48d597}.channel{display:flex;gap:8px;align-items:center;margin:4px}.channel span{width:24px}.channel i{height:12px;background:#50b7ff;display:block}a{color:#50b7ff}.open{color:#ff5f62}button,select,input{padding:7px;margin:4px;background:#263442;color:#fff;border:1px solid #526477;border-radius:5px}small{color:#9cabb9}
</style></head><body><div class=box><h1>ESP32 WiFi Scanner</h1><p id=summary></p>
<div class=grid><section class=card><h2 id=channelsTitle></h2><div id=channels></div></section><section class=card><h2 id=securityTitle></h2><div id=security></div></section>
<section class=card><h2 id=settingsTitle></h2><label><span id=scanLabel></span> <select id=scan onchange=save()><option value=3000>3 s</option><option value=5000>5 s</option><option value=10000>10 s</option></select></label><br>
<label><input type=checkbox id=groups onchange=save()><span id=groupsLabel></span></label><br><label><input type=checkbox id=hidden onchange=save()><span id=hiddenLabel></span></label><br>
<label><input type=checkbox id=light onchange=save()><span id=lightLabel></span></label><br><label><input type=checkbox id=sd onchange=save()><span id=sdLabel></span></label><br>
<label><span id=brightnessLabel></span> <select id=bright onchange=save()><option value=25>25%</option><option value=50>50%</option><option value=75>75%</option><option value=100>100%</option></select></label><br>
<label><span id=languageLabel></span> <select id=langSel onchange=save()><option value=de>Deutsch</option><option value=en>English</option></select></label><br><button id=scanNow onclick=fetch('/api/scan',{method:'POST'})></button></section>
<section class=card><h2 id=exportTitle></h2><p><a id=currentCsvLink href=/api/csv></a></p><p><a id=historyCsvLink href=/api/history.csv></a></p><button id=resetButton onclick="if(confirm(T[lang].resetConfirm))fetch('/api/factory-reset',{method:'POST'})"></button></section></div>
<h2 id=networksTitle></h2><table><thead><tr><th id=ssidHeading></th><th id=channelHeading></th><th id=signalHeading></th><th id=securityHeading></th></tr></thead><tbody id=rows></tbody></table></div>
<script>
const T={de:{channels:'Kanaele',security:'Sicherheit',settings:'Einstellungen',scan:'Scan',groups:'SSID gruppieren',hidden:'Hidden anzeigen',light:'Helles Display',sd:'SD-Protokoll',brightness:'Helligkeit',language:'Sprache',scanNow:'Jetzt scannen',export:'Export',currentCsv:'Aktueller Scan (CSV)',historyCsv:'Signalverlauf (CSV)',factory:'Werkseinstellungen',networks:'Netzwerke',ssid:'SSID / Hersteller',channel:'Kanal',signal:'Signal',open:'OFFEN',recommend:'Empfehlung Kanal',age:'Scan vor',clock:'Uhr nicht synchron',resetConfirm:'Alle Einstellungen, Sprache und Kalibrierung loeschen?'},en:{channels:'Channels',security:'Security',settings:'Settings',scan:'Scan',groups:'Group SSIDs',hidden:'Show hidden networks',light:'Light display theme',sd:'SD logging',brightness:'Brightness',language:'Language',scanNow:'Scan now',export:'Export',currentCsv:'Current scan (CSV)',historyCsv:'Signal history (CSV)',factory:'Factory reset',networks:'Networks',ssid:'SSID / vendor',channel:'Channel',signal:'Signal',open:'OPEN',recommend:'Recommended channel',age:'Scan age',clock:'Clock not synchronized',resetConfirm:'Delete all settings, language and calibration?'}};
let first=true,lang='de';function applyLanguage(value){lang=value;document.documentElement.lang=value;let t=T[lang];channelsTitle.textContent=t.channels;securityTitle.textContent=t.security;settingsTitle.textContent=t.settings;scanLabel.textContent=t.scan;groupsLabel.textContent=t.groups;hiddenLabel.textContent=t.hidden;lightLabel.textContent=t.light;sdLabel.textContent=t.sd;brightnessLabel.textContent=t.brightness;languageLabel.textContent=t.language;scanNow.textContent=t.scanNow;exportTitle.textContent=t.export;currentCsvLink.textContent=t.currentCsv;historyCsvLink.textContent=t.historyCsv;resetButton.textContent=t.factory;networksTitle.textContent=t.networks;ssidHeading.textContent=t.ssid;channelHeading.textContent=t.channel;signalHeading.textContent=t.signal;securityHeading.textContent=t.security;langSel.value=value}
async function syncTime(){await fetch('/api/time?epoch='+Math.floor(Date.now()/1000)+'&offset='+new Date().getTimezoneOffset(),{method:'POST'})}
async function save(){let q=new URLSearchParams({scan:scan.value,groups:groups.checked?1:0,hidden:hidden.checked?1:0,light:light.checked?1:0,sd:sd.checked?1:0,brightness:bright.value,language:langSel.value});await fetch('/api/settings?'+q,{method:'POST'});load()}
async function load(){let d=await(await fetch('/api/networks')).json();applyLanguage(d.settings.language);let t=T[lang];summary.textContent=d.networks.length+' '+t.networks+' - '+t.recommend+' '+d.suggestedChannel+' - '+t.age+' '+d.scanAge+' s'+(d.timeSynced?'':' - '+t.clock);
let cc=Array(14).fill(0);d.networks.forEach(n=>{if(n.channel>0&&n.channel<14)cc[n.channel]+=n.quality});let mx=Math.max(1,...cc);channels.innerHTML=cc.slice(1).map((v,i)=>`<div class=channel><span>${i+1}</span><i style="width:${Math.round(v/mx*190)}px"></i><b>${v}</b></div>`).join('');
let sec={WPA3:0,WPA2:0,WPA:0,WEP:0,OPEN:0};d.networks.forEach(n=>sec[n.security in sec?n.security:(n.security.startsWith('WPA2')?'WPA2':'WPA')]++);security.innerHTML=Object.entries(sec).map(x=>`<p class="${x[0]=='OPEN'?'open':''}">${x[0]=='OPEN'?t.open:x[0]}: <b>${x[1]}</b></p>`).join('');
rows.innerHTML=d.networks.map(n=>`<tr><td>${n.ssid}<small><br>${n.bssid} - ${n.vendor}</small></td><td>${n.channel}</td><td>${n.rssi} dBm<div class=bar style="width:${n.quality}%"></div></td><td class="${n.security==='OPEN'?'open':''}">${n.security==='OPEN'?t.open:n.security}</td></tr>`).join('');
if(first){scan.value=d.settings.scan;groups.checked=d.settings.groups;hidden.checked=d.settings.hidden;light.checked=d.settings.light;sd.checked=d.settings.sd;first=false}bright.value=d.settings.brightness}
syncTime().then(load);setInterval(load,3000)
</script></body></html>)HTML";

void prepareWebRoutes() {
  if (webRoutesReady) return;
  web.on("/", []() { web.send_P(200, "text/html", DASHBOARD); });
  web.on("/api/networks", []() { web.send(200, "application/json", networksJson()); });
  web.on("/api/csv", []() {
    web.sendHeader("Content-Disposition", "attachment; filename=esp32-wifi-scanner.csv");
    web.send(200, "text/csv", currentCsv());
  });
  web.on("/api/history.csv", []() {
    web.sendHeader("Content-Disposition", "attachment; filename=wifi-history.csv");
    web.send(200, "text/csv", historyCsv());
  });
  web.on("/api/time", HTTP_POST, []() {
    if (web.hasArg("epoch")) {
      epochAtSync = strtoull(web.arg("epoch").c_str(), nullptr, 10);
      timezoneOffsetMinutes = web.hasArg("offset") ? web.arg("offset").toInt() : 0;
      millisAtSync = millis();
      timeSynced = epochAtSync > 1700000000ULL;
      saveSettings();
    }
    web.send(200, "text/plain", timeSynced ? "OK" : "INVALID");
  });
  web.on("/api/settings", HTTP_POST, []() {
    if (web.hasArg("scan")) scanMs = constrain(web.arg("scan").toInt(), 3000, 60000);
    if (web.hasArg("groups")) groupSsids = web.arg("groups") == "1";
    if (web.hasArg("hidden")) showHidden = web.arg("hidden") == "1";
    if (web.hasArg("light")) lightTheme = web.arg("light") == "1";
    if (web.hasArg("language")) {
      english = web.arg("language") == "en";
      languageSelected = true;
    }
    if (web.hasArg("sd")) { sdLogging = web.arg("sd") == "1"; if (sdLogging) ensureSd(); }
    if (web.hasArg("brightness")) {
      brightness = brightnessFromPercent(web.arg("brightness").toInt());
      applyBrightness();
    }
    page = 0;
    saveSettings();
    render();
    web.send(200, "application/json", "{\"ok\":true}");
  });
  web.on("/api/scan", HTTP_POST, []() {
    paused = false;
    nextScan = millis();
    web.send(202, "application/json", "{\"scanning\":true}");
  });
  web.on("/api/factory-reset", HTTP_POST, []() {
    web.send(200, "text/plain", "RESET");
    delay(200);
    prefs.clear();
    ESP.restart();
  });
  webRoutesReady = true;
}

void applyWebSetting() {
  if (webEnabled && !webRunning) {
    WiFi.mode(WIFI_AP_STA);
    prepareWebRoutes();
    webRunning = WiFi.softAP("ESP32-WiFi-Scanner", "scanner1234");
    if (webRunning) web.begin();
  } else if (!webEnabled && webRunning) {
    web.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    webRunning = false;
  }
}

void sortNets() {
  for (int i = 1; i < netCount; ++i) {
    Net item = nets[i];
    int j = i - 1;
    while (j >= 0 && nets[j].rssi < item.rssi) { nets[j + 1] = nets[j]; --j; }
    nets[j + 1] = item;
  }
}

void collectResults(int count) {
  netCount = min(count, MAX_NETS);
  for (int i = 0; i < netCount; ++i) {
    nets[i] = {WiFi.SSID(i), WiFi.BSSIDstr(i), WiFi.RSSI(i),
               static_cast<uint8_t>(WiFi.channel(i)), WiFi.encryptionType(i)};
  }
  sortNets();
  buildGroups();
  selected = findSelected();
  if (selectedBssid.length()) {
    addMeasurement(selected >= 0, selected >= 0 ? nets[selected].rssi : -100,
                   selected >= 0 ? nets[selected].channel : 0);
  }
}

void startScan() {
  if (paused || scanning) return;
  int result = WiFi.scanNetworks(true, showHidden);
  scanning = result != WIFI_SCAN_FAILED;
  if (!scanning) nextScan = millis() + 2000;
  updateLed();
  if (netCount == 0) render(); else drawHeader();
}

void finishScan() {
  if (!scanning) return;
  int result = WiFi.scanComplete();
  if (result >= 0) {
    collectResults(result);
    WiFi.scanDelete();
    scanning = false;
    lastScanFinished = millis();
    nextScan = millis() + scanMs;
    logToSd();
    updateLed();
    render();
  } else if (result == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
    scanning = false;
    nextScan = millis() + 2000;
    updateLed();
    render();
  }
}

void togglePause() {
  paused = !paused;
  if (paused) saveMeasurementState();
  if (!paused) nextScan = millis();
  updateLed();
  render();
}

void forceScan() {
  paused = false;
  nextScan = millis();
  if (!scanning) startScan();
  render();
}

void openSettings() {
  if (view == SETTINGS) view = returnView;
  else { returnView = view; view = SETTINGS; }
  render();
}

void changeSetting(int row) {
  if (settingsPage == 1) {
    if (row == 2) saveMeasurementState();
    if (row == 4) {
      english = !english;
      languageSelected = true;
      saveSettings();
    }
    if (row == 5) {
      if (millis() < resetArmedUntil) {
        saveMeasurementState();
        prefs.clear();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(tr("Neustart ...", "Restarting ..."), 120, 160, 4);
        delay(500);
        ESP.restart();
      } else resetArmedUntil = millis() + 5000;
    }
    render();
    return;
  }
  switch (row) {
    case 0: scanMs = scanMs == 3000 ? 5000 : (scanMs == 5000 ? 10000 : 3000); break;
    case 1: groupSsids = !groupSsids; page = 0; break;
    case 2: showHidden = !showHidden; forceScan(); break;
    case 3:
      brightness = brightnessFromPercent(brightnessPercent() == 25 ? 50 :
                                        brightnessPercent() == 50 ? 75 :
                                        brightnessPercent() == 75 ? 100 : 25);
      applyBrightness();
      break;
    case 4: lightTheme = !lightTheme; break;
    case 5:
      sdLogging = !sdLogging;
      if (sdLogging) ensureSd();
      break;
    case 6:
      webEnabled = !webEnabled;
      applyWebSetting();
      break;
    case 7:
      returnView = SETTINGS;
      view = CALIBRATION;
      calibrationStep = 0;
      firstRunCalibration = false;
      fingerDown = false;
      drawCalibration();
      return;
  }
  saveSettings();
  render();
}

void handleTap(int x, int y) {
  if (y < HEADER_H && x >= 198) { openSettings(); return; }
  if (view == SETTINGS && y >= 28 && y < 284) { changeSetting((y - 28) / 32); return; }
  if (view == LIST && y >= HEADER_H && y < FOOTER_Y) {
    int item = page * ROWS + (y - HEADER_H) / ROW_H;
    if (item < itemCount()) {
      int ni = listNetIndex(item);
      saveMeasurementState();
      selectedBssid = nets[ni].bssid;
      selectedSsid = nets[ni].ssid;
      selected = ni;
      resetMeasurement(nets[ni].rssi, nets[ni].channel);
      view = DETAILS;
      render();
    }
    return;
  }
  if (view == MESH_APS && y >= HEADER_H && y < FOOTER_Y) {
    int item = meshPage * ROWS + (y - HEADER_H) / ROW_H;
    int ni = meshNetIndex(item);
    if (ni >= 0) {
      saveMeasurementState();
      selectedBssid = nets[ni].bssid;
      selectedSsid = nets[ni].ssid;
      selected = ni;
      resetMeasurement(nets[ni].rssi, nets[ni].channel);
      view = DETAILS;
      render();
    }
    return;
  }
  if (y < FOOTER_Y) return;
  if (view == DETAILS) {
    if (x < 80) view = LIST;
    else if (x < 160) { meshPage = 0; view = MESH_APS; }
    else if (selected >= 0) resetMeasurement(nets[selected].rssi, nets[selected].channel);
    render(); return;
  }
  if (view == MESH_APS) {
    if (x < 80) view = DETAILS;
    else if (x < 160) {
      int pages = max(1, (meshItemCount() + ROWS - 1) / ROWS);
      meshPage = (meshPage + 1) % pages;
    } else { togglePause(); return; }
    render(); return;
  }
  if (view == SETTINGS) {
    if (x < 80) view = returnView;
    else if (x < 160) { settingsPage = 1 - settingsPage; render(); return; }
    else { togglePause(); return; }
    render(); return;
  }
  if (x < 80) {
    int pages = max(1, (itemCount() + ROWS - 1) / ROWS);
    if (view == LIST && pages > 1) page = (page + 1) % pages;
    view = LIST;
  } else if (x < 160) {
    view = view == CHANNELS ? SECURITY : CHANNELS;
  } else { togglePause(); return; }
  render();
}

bool readMappedTouch(int &x, int &y) {
  if (!touch.touched()) return false;
  TS_Point p = touch.getPoint();
  if (p.z <= 150) return false;
  x = constrain((int)map(p.x, touchLeft, touchRight, 20, 219), 0, W - 1);
  y = constrain((int)map(p.y, touchTop, touchBottom, 20, 299), 0, H - 1);
  return true;
}

void handleCalibrationInput() {
  bool pressed = touch.touched();
  if (pressed && !fingerDown && millis() - lastInput > 300) {
    TS_Point p = touch.getPoint();
    if (p.z > 150) {
      lastInput = millis();
      fingerDown = true;
      if (calibrationStep == 0) {
        calibrationFirst = p;
        calibrationStep = 1;
        drawCalibration();
      } else {
        touchLeft = calibrationFirst.x;
        touchTop = calibrationFirst.y;
        touchRight = p.x;
        touchBottom = p.y;
        touchCalibrated = true;
        saveSettings();
        view = firstRunCalibration ? LIST : SETTINGS;
        firstRunCalibration = false;
        nextScan = millis();
        fingerDown = false;
        render();
      }
    }
  } else if (!pressed) fingerDown = false;
}

void handleLanguageInput() {
  bool pressed = touch.touched();
  if (pressed && !fingerDown && millis() - lastInput > 300) {
    TS_Point p = touch.getPoint();
    if (p.z > 150) {
      lastInput = millis();
      fingerDown = true;
      english = p.x >= 2000;
      languageSelected = true;
      saveSettings();
      if (touchCalibrated) {
        view = LIST;
        nextScan = millis();
        render();
      } else {
        firstRunCalibration = true;
        calibrationStep = 0;
        view = CALIBRATION;
        drawCalibration();
      }
    }
  } else if (!pressed) fingerDown = false;
}

void handleInput() {
  if (view == LANGUAGE) { handleLanguageInput(); return; }
  if (view == CALIBRATION) { handleCalibrationInput(); return; }
  int x = 0, y = 0;
  bool pressed = readMappedTouch(x, y);
  if (pressed && !fingerDown) {
    fingerDown = true;
    downX = x; downY = y;
    lastTouchX = x; lastTouchY = y;
  } else if (pressed && fingerDown) {
    lastTouchX = x; lastTouchY = y;
  } else if (!pressed && fingerDown) {
    fingerDown = false;
    if (millis() - lastInput > 180) {
      lastInput = millis();
      int dx = lastTouchX - downX;
      if (abs(dx) > 55 && view == LIST) {
        int pages = max(1, (itemCount() + ROWS - 1) / ROWS);
        page = (page + (dx < 0 ? 1 : pages - 1)) % pages;
        render();
      } else handleTap(downX, downY);
    }
  }
  bool buttonDown = digitalRead(BUTTON_PIN) == LOW;
  if (buttonDown && !buttonWasDown && millis() - lastInput > 250) { lastInput = millis(); togglePause(); }
  buttonWasDown = buttonDown;
}

void setup() {
  Serial.begin(115200);
  // Keep the legacy namespace so existing installations retain their settings.
  prefs.begin("wifi-radar", false);
  loadSettings();
  loadMeasurementState();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  tft.init();
  tft.setRotation(0);
  applyBrightness();
  tft.fillScreen(bg());

  auxSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(auxSPI);
  touch.setRotation(0);

  // A missing NVS marker means a fresh or erased preferences partition.
  // Calibrate before the first scan and remember the successful result.
  if (!languageSelected) {
    view = LANGUAGE;
    fingerDown = false;
  } else if (!touchCalibrated) {
    firstRunCalibration = true;
    calibrationStep = 0;
    view = CALIBRATION;
  }

  WiFi.mode(webEnabled ? WIFI_AP_STA : WIFI_STA);
  WiFi.disconnect(true, false);
  applyWebSetting();
  if (sdLogging) ensureSd();

  updateLed();
  render();
  nextScan = millis();
}

void loop() {
  handleInput();
  finishScan();
  if (webRunning) web.handleClient();
  if (view == LIST && millis() - lastMarqueeAt >= 550) {
    lastMarqueeAt = millis();
    marqueeOffset++;
    drawVisibleNames();
  }
  if (view != CALIBRATION && millis() - lastHeaderAt >= 1000) {
    lastHeaderAt = millis();
    drawHeader();
  }
  if (view != CALIBRATION && !paused && !scanning && (int32_t)(millis() - nextScan) >= 0) startScan();
  delay(8);
}
