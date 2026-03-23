#include <Adafruit_TinyUSB.h>

// ---------- HID Report Descriptor (16 кнопок + 2 стика) ----------
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

Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 1, false);

// ---------- Состояние джойконов ----------
uint8_t leftBtns = 0,  leftX  = 128, leftY  = 128;
uint8_t rightBtns = 0, rightX = 128, rightY = 128;

bool     rightActive = false, leftActive = false;
uint32_t lastRightMs = 0,     lastLeftMs = 0;
const uint32_t JOYCON_TIMEOUT_MS = 500;

// ---------- Прототип ----------
bool checkUART(arduino::HardwareSerial *uart, uint8_t &btns, uint8_t &x, uint8_t &y);

// ================================================================
void setup() {
  Serial1.setRX(1);
  Serial1.begin(115200);

  Serial2.setRX(5);
  Serial2.begin(115200);

  usb_hid.begin();
  TinyUSBDevice.setManufacturerDescriptor("KuroCube");
  TinyUSBDevice.setProductDescriptor("KuroBit Gamepad");
  TinyUSBDevice.setSerialDescriptor("KB-001");

  delay(1000);
}

// ================================================================
void loop() {
  uint32_t now = millis();

  if (checkUART(&Serial1, rightBtns, rightX, rightY)) {
    rightActive = true;
    lastRightMs = now;
  }
  if (checkUART(&Serial2, leftBtns, leftX, leftY)) {
    leftActive = true;
    lastLeftMs = now;
  }

  if (now - lastRightMs > JOYCON_TIMEOUT_MS) rightActive = false;
  if (now - lastLeftMs  > JOYCON_TIMEOUT_MS) leftActive  = false;

  // --- HID отчёт ---
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

    if (leftBtns & (1 << 0)) buttons |= (1 << 4);
    if (leftBtns & (1 << 1)) buttons |= (1 << 6);
    if (leftBtns & (1 << 2)) buttons |= (1 << 8);
    if (leftBtns & (1 << 3)) buttons |= (1 << 10);
    if (leftBtns & (1 << 4)) buttons |= (1 << 12);
    if (leftBtns & (1 << 5)) buttons |= (1 << 13);
    if (leftBtns & (1 << 6)) buttons |= (1 << 14);
    if (leftBtns & (1 << 7)) buttons |= (1 << 15);

    int8_t report[6];
    report[0] = buttons & 0xFF;
    report[1] = (buttons >> 8) & 0xFF;
    report[2] = (int8_t)constrain(128 - (int16_t)leftX,  -127, 127);
    report[3] = (int8_t)constrain((int16_t)leftY  - 128, -127, 127);
    report[4] = (int8_t)constrain(128 - (int16_t)rightY, -127, 127);
    report[5] = (int8_t)constrain((int16_t)rightX - 128, -127, 127);

    usb_hid.sendReport(0, report, sizeof(report));
  }

  delay(5);
}

// ================================================================
bool checkUART(arduino::HardwareSerial *uart, uint8_t &btns, uint8_t &x, uint8_t &y) {
  bool got = false;

  while (uart->available() >= 4) {
    uint8_t b[4];
    uart->readBytes(b, 4);

    if (b[3] == 0xAA) {
      btns = b[0];
      x    = b[1];
      y    = b[2];
      got  = true;
    } else {
      // Плохой пакет — ищем 0xAA для ресинхронизации
      for (int i = 0; i < 3; i++) {
        if (b[i] == 0xAA) break;
      }
    }
  }
  return got;
}
