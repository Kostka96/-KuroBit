#include <Adafruit_TinyUSB.h>

const int centerX = 512;
const int centerY = 512;
const int deadzone = 20; // Чуть увеличил для надежности

uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_GAMEPAD()
};
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);

hid_gamepad_report_t gp;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200); // Пины GP0 (TX), GP1 (RX)
  
  usb_hid.begin();
  while( !TinyUSBDevice.mounted() ) delay(1);
}

void loop() {
  if (Serial1.available()) {
    // Читаем строку
    String data = Serial1.readStringUntil(';');
    
    int firstComma = data.indexOf(',');
    int secondComma = data.lastIndexOf(',');

    if (firstComma != -1 && secondComma != -1) {
      int rawX = data.substring(0, firstComma).toInt();
      int rawY = data.substring(firstComma + 1, secondComma).toInt();
      int btn = data.substring(secondComma + 1).toInt();

      int8_t joyX, joyY; // Объявляем переменные здесь

      // Обработка оси X
      if (abs(rawX - centerX) < deadzone) joyX = 0;
      else if (rawX < centerX) joyX = map(rawX, 0, centerX, 127, 0);
      else joyX = map(rawX, centerX, 1023, 0, -127);

      // Обработка оси Y
      if (abs(rawY - centerY) < deadzone) joyY = 0;
      else if (rawY < centerY) joyY = map(rawY, 0, centerY, -127, 0);
      else joyY = map(rawY, centerY, 1023, 0, 127);

      // Обновляем отчет
      gp.x = joyX;
      gp.y = joyY;
      gp.buttons = (btn == 1) ? 0x01 : 0x00;

      usb_hid.sendReport(0, &gp, sizeof(gp));
    }
  }
}