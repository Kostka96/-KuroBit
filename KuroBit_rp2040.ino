#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "bitmaps.h"

// ---------- HID Report Descriptor ----------
uint8_t const desc_hid_report[] = {
  0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
  0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01,
  0x95, 0x10, 0x75, 0x01, 0x81, 0x02,
  0x05, 0x01, 0x09, 0x01, 0xA1, 0x00,
    0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F,
    0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  0xC0,
  0x09, 0x01, 0xA1, 0x00,
    0x09, 0x32, 0x09, 0x33, 0x15, 0x81, 0x25, 0x7F,
    0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
  0xC0,
  0xC0
};

Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 10, false);

// ---------- Дисплей ----------
Adafruit_SSD1306 display(128, 32, &Wire1, -1);

// ---------- Состояние ----------
uint8_t leftBtns = 0, leftX = 128, leftY = 128;
uint8_t rightBtns = 0, rightX = 128, rightY = 128;

bool     rightActive = false, leftActive = false;
uint32_t lastRightMs = 0,     lastLeftMs = 0;
const uint32_t JOYCON_TIMEOUT_MS = 500;

// ---------- Состояние кнопок и таймеры ----------
const unsigned char* currentLeftIcon = nullptr;
const unsigned char* currentRightIcon = nullptr;
uint32_t lastLeftBtnMs = 0;
uint32_t lastRightBtnMs = 0;
const uint32_t BTN_DISPLAY_MS = 5000;

// ---------- Температура ----------
float    rpiTemp   = 0.0f;
String   serialBuf = "";
uint32_t lastRpiMs = 0;
const uint32_t RPI_TIMEOUT_MS = 5000;

// ---------- Батарея ----------
const int   BAT_PIN      = 26;
const float BAT_FULL     = 8.4f;
const float BAT_EMPTY    = 6.0f;
static int  lastPercent  = -1;
uint32_t    lastBatMs    = 0;
const uint32_t BAT_INTERVAL_MS = 5000;  // батарею меряем раз в 5 сек
int         cachedBat    = 0;

// ---------- Дисплей ----------
uint32_t lastDisplayMs = 0;
const uint32_t DISPLAY_INTERVAL_MS = 100;  // 100мс для плавной анимации

// ---------- Анимация ----------
bool animDone = false;
const uint32_t CAT_PHASE_MS   = 5000;   // кот идёт
const uint32_t DOCK_PHASE_MS  = 3000;   // стыковка
const uint32_t HOLD_PHASE_MS  = 8000;   // застывание
const uint32_t ANIM_DURATION_MS = CAT_PHASE_MS + DOCK_PHASE_MS + HOLD_PHASE_MS;
uint32_t animStartMs = 0;

// ---------- Прототипы ----------
bool checkUART(arduino::HardwareSerial *uart, uint8_t &btns, uint8_t &x, uint8_t &y);
void readRpiSerial();
void drawAnimation(uint32_t elapsed);
void drawMainScreen();
int  getBatteryPercent();

// ================================================================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  Wire1.setSDA(2);
  Wire1.setSCL(3);
  Wire1.begin();
  Wire1.setClock(400000);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setRotation(2);  // 180°
  display.clearDisplay();
  display.display();

  Serial1.setRX(1);
  Serial1.begin(115200);

  Serial2.setRX(5);
  Serial2.begin(115200);

  usb_hid.begin();
  TinyUSBDevice.setManufacturerDescriptor("KuroCube");
  TinyUSBDevice.setProductDescriptor("KuroBit Gamepad");
  TinyUSBDevice.setSerialDescriptor("KB-001");

  animStartMs = millis();
}

// ================================================================
void loop() {
  uint32_t now = millis();

// Для правого джойкона
  if (checkUART(&Serial1, rightBtns, rightX, rightY)) {
    rightActive = true;
    lastRightMs = now;
    if (rightBtns > 0) {
      for (int i = 0; i < 8; i++) {
        if (rightBtns & (1 << i)) {
          currentRightIcon = right_btn_images[i];
          lastRightBtnMs = now;
          break; // Берем первую нажатую кнопку
        }
      }
    }
  }

  // Для левого джойкона
  if (checkUART(&Serial2, leftBtns, leftX, leftY)) {
    leftActive = true;
    lastLeftMs = now;
    if (leftBtns > 0) {
      for (int i = 0; i < 8; i++) {
        if (leftBtns & (1 << i)) {
          currentLeftIcon = left_btn_images[i];
          lastLeftBtnMs = now;
          break;
        }
      }
    }
  }

  // Очистка по таймауту
  if (now - lastLeftBtnMs > BTN_DISPLAY_MS) currentLeftIcon = nullptr;
  if (now - lastRightBtnMs > BTN_DISPLAY_MS) currentRightIcon = nullptr;

  if (now - lastRightMs > JOYCON_TIMEOUT_MS) rightActive = false;
  if (now - lastLeftMs  > JOYCON_TIMEOUT_MS) leftActive  = false;

  readRpiSerial();

  // --- HID ---
  static uint32_t lastHidMs = 0;
  if (now - lastHidMs >= 10) {
    lastHidMs = now;

    if (usb_hid.ready()) {
      uint16_t buttons = 0;
      if (rightBtns & (1 << 0)) buttons |= (1 << 0);
      if (rightBtns & (1 << 1)) buttons |= (1 << 1);
      if (rightBtns & (1 << 2)) buttons |= (1 << 2);
      if (rightBtns & (1 << 3)) buttons |= (1 << 3);
      if (rightBtns & (1 << 4)) buttons |= (1 << 7);
      if (rightBtns & (1 << 5)) buttons |= (1 << 5);
      if (rightBtns & (1 << 6)) buttons |= (1 << 9);
      if (rightBtns & (1 << 7)) buttons |= (1 << 11);
      if (leftBtns  & (1 << 0)) buttons |= (1 << 4);
      if (leftBtns  & (1 << 1)) buttons |= (1 << 6);
      if (leftBtns  & (1 << 2)) buttons |= (1 << 8);
      if (leftBtns  & (1 << 3)) buttons |= (1 << 10);
      if (leftBtns  & (1 << 4)) buttons |= (1 << 12);
      if (leftBtns  & (1 << 5)) buttons |= (1 << 13);
      if (leftBtns  & (1 << 6)) buttons |= (1 << 14);
      if (leftBtns  & (1 << 7)) buttons |= (1 << 15);

      int8_t report[6];
      // 1-2 байты: Кнопки (16 штук)
      report[0] = (uint8_t)(buttons & 0xFF);
      report[1] = (uint8_t)((buttons >> 8) & 0xFF);
      
      // 3-4 байты: Левый стик (X, Y)
      // Linux ожидает значения от -127 до 127
      report[2] = (int8_t)constrain(128 - (int16_t)leftX,  -127, 127);
      report[3] = (int8_t)constrain((int16_t)leftY  - 128, -127, 127);
      
      // 5-6 байты: Правый стик (X, Y)
      report[4] = (int8_t)constrain(128 - (int16_t)rightY, -127, 127);
      report[5] = (int8_t)constrain((int16_t)rightX - 128, -127, 127);

      // Отправляем без ID (0), так как в дескрипторе его нет
      usb_hid.sendReport(0, report, sizeof(report));
    }
  }
  
  // --- Батарея раз в 5 секунд ---
  if (now - lastBatMs >= BAT_INTERVAL_MS) {
    lastBatMs  = now;
    cachedBat  = getBatteryPercent();
  }

  // --- Дисплей ---
  if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
    lastDisplayMs = now;
    uint32_t elapsed = now - animStartMs;
    if (!animDone) {
      if (elapsed >= ANIM_DURATION_MS) animDone = true;
      else drawAnimation(elapsed);
    }
    if (animDone) drawMainScreen();
  }
  // delay убран полностью
}

// ================================================================
void drawAnimation(uint32_t elapsed) {
  display.clearDisplay();

  if (elapsed < CAT_PHASE_MS) {
    // ---- Фаза 1: кот идёт (0–5с) ----
    float t = (float)elapsed / CAT_PHASE_MS;
    int catX = (int)(-40 + t * (128 + 40));
    int catY = 5 + (elapsed % 400 < 200 ? 2 : 0);
    display.drawBitmap(catX, catY, epd_bitmap_Cat, 36, 22, SSD1306_WHITE);

  } else if (elapsed < CAT_PHASE_MS + DOCK_PHASE_MS) {
    // ---- Фаза 2: стыковка (5–8с) ----
    float t = (float)(elapsed - CAT_PHASE_MS) / DOCK_PHASE_MS;
    if (t > 1.0f) t = 1.0f;
    float ease = 1.0f - pow(1.0f - t, 3);

    int currentLX = (int)(-20 + ease * (28 + 20));
    int currentRX = (int)(128  - ease * (128 - 84));

    display.drawBitmap(44,        0, epd_bitmap_Hub, 40, 32, SSD1306_WHITE);
    display.drawBitmap(currentLX, 0, epd_bitmap_L,  16, 32, SSD1306_WHITE);
    display.drawBitmap(currentRX, 0, epd_bitmap_R,  16, 32, SSD1306_WHITE);

  } else {
    // ---- Фаза 3: застывание (8–16с) ----
    // Все три части на финальных позициях
    display.drawBitmap(44, 0, epd_bitmap_Hub, 40, 32, SSD1306_WHITE);
    display.drawBitmap(28, 0, epd_bitmap_L,  16, 32, SSD1306_WHITE);
    display.drawBitmap(84, 0, epd_bitmap_R,  16, 32, SSD1306_WHITE);
  }

  display.display();
}

// ================================================================
void drawMainScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  int bat = cachedBat;
  bool rpiOk = (millis() - lastRpiMs < RPI_TIMEOUT_MS);

  // Хаб по центру
  display.drawBitmap(44, 0, epd_bitmap_Hub_Clear, 40, 32, SSD1306_WHITE);

  // Левый джойкон
  if (leftActive) {
    display.drawBitmap(28, 0, epd_bitmap_L, 16, 32, SSD1306_WHITE);
  }

  // Правый джойкон
  if (rightActive) {
    display.drawBitmap(84, 0, epd_bitmap_R, 16, 32, SSD1306_WHITE);
  }
  // Рисуем иконку слева (20x32)
  if (currentLeftIcon != nullptr) {
    display.drawBitmap(4, 0, currentLeftIcon, 20, 32, SSD1306_WHITE);
  }

  // Иконка последней кнопки СПРАВА
  if (currentRightIcon != nullptr) {
    display.drawBitmap(104, 0, currentRightIcon, 20, 32, SSD1306_WHITE);
  }
  // Температура внутри хаба
  display.setTextSize(1);
  display.setCursor(53, 6);
  if (rpiOk && rpiTemp > 0) {
    display.print((int)rpiTemp);
    display.print("C");
  } else {
    display.print("--C");
  }

  // Батарея внутри хаба
  display.drawRect(51, 18, 20, 7, SSD1306_WHITE);   // корпус
  display.fillRect(71, 21,  2, 3, SSD1306_WHITE);   // плюсик
  int fillW = (int)(bat / 100.0f * 18);
  if (fillW > 0)
    display.fillRect(52, 19, fillW, 5, SSD1306_WHITE);

  display.display();
}

// ================================================================
int getBatteryPercent() {
  static bool warmed = false;
  if (!warmed) {
    for (int i = 0; i < 5; i++) analogRead(BAT_PIN);
    warmed = true;
  }

  // Просто усредняем без delay
  long sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(BAT_PIN);
  int raw = sum / 16;

  float voltage = (raw / 4095.0f) * 3.3f * 12.86f;
  int percent = (int)((voltage - BAT_EMPTY) / (BAT_FULL - BAT_EMPTY) * 100.0f);
  percent = constrain(percent, 0, 100);

  if (lastPercent == -1)          lastPercent = percent;
  else if (percent > lastPercent) lastPercent++;
  else if (percent < lastPercent) lastPercent--;

  return lastPercent;
}

// ================================================================
void readRpiSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (serialBuf.startsWith("T:")) {
        rpiTemp   = serialBuf.substring(2).toFloat();
        lastRpiMs = millis();
      }
      serialBuf = "";
    } else if (c != '\r') {
      if (serialBuf.length() < 16) serialBuf += c;
    }
  }
}

// ================================================================
bool checkUART(arduino::HardwareSerial *uart, uint8_t &btns, uint8_t &x, uint8_t &y) {
  bool got = false;
  while (uart->available() >= 5) {
    if (uart->peek() != 0xFF) { uart->read(); continue; }
    uint8_t b[5];
    uart->readBytes(b, 5);
    if (b[0] == 0xFF && b[4] == 0xAA) {
      btns = b[1]; x = b[2]; y = b[3];
      got = true;
    }
  }
  return got;
}
String getButtonName(uint8_t btns, bool isRight) {
  if (btns == 0) return ""; 
  // Если кнопок несколько, вернем первую найденную
  if (isRight) {
    if (btns & (1 << 0)) return "A";
    if (btns & (1 << 1)) return "B";
    if (btns & (1 << 2)) return "X";
    if (btns & (1 << 3)) return "Y";
    if (btns & (1 << 4)) return "R1";
    if (btns & (1 << 5)) return "R2";
  } else {
    if (btns & (1 << 0)) return "UP";
    if (btns & (1 << 1)) return "DWN";
    if (btns & (1 << 2)) return "LFT";
    if (btns & (1 << 3)) return "RGT";
    if (btns & (1 << 4)) return "L1";
    if (btns & (1 << 5)) return "L2";
  }
  return "?";
}
