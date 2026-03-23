// Константы для правой стороны (8 кнопок)
const int btnPins[8] = {
  2,
  3, 
  4,  
  5,  
  6, 
  7, 
  8, 
  9   
};

float smoothX = 512;
float smoothY = 512;
const float filterAlpha = 0.2;

int centerX = 512;
int centerY = 512;

void setup() {
  // Скорость UART должна совпадать с KuroBridge (RP2040)
  Serial.begin(115200);

  // Инициализируем только 8 кнопок
  for (int i = 0; i < 8; i++) {
    pinMode(btnPins[i], INPUT_PULLUP);
  }

  // Калибровка центра
  long sumX = 0;
  long sumY = 0;
  for(int i = 0; i < 30; i++){
    sumX += analogRead(A0);
    sumY += analogRead(A1);
    delay(5);
  }
  centerX = sumX / 30;
  centerY = sumY / 30;

  smoothX = centerX;
  smoothY = centerY;
}

void loop() {
  // 1. Читаем стик
  int rawX = analogRead(A0);
  int rawY = analogRead(A1);

  // 2. Сглаживаем
  smoothX = (rawX * filterAlpha) + (smoothX * (1.0 - filterAlpha));
  smoothY = (rawY * filterAlpha) + (smoothY * (1.0 - filterAlpha));

  // 3. Собираем маску кнопок
  uint8_t mask = 0;

  if (digitalRead(2) == LOW) mask |= (1 << 0); 
  if (digitalRead(3) == LOW) mask |= (1 << 1); 
  if (digitalRead(4) == LOW) mask |= (1 << 2); 
  if (digitalRead(5) == LOW) mask |= (1 << 3); 
  if (digitalRead(6) == LOW) mask |= (1 << 4); 
  if (digitalRead(7) == LOW) mask |= (1 << 5); 
  if (digitalRead(8) == LOW) mask |= (1 << 6); 
  if (digitalRead(9) == LOW) mask |= (1 << 7); 

  // 4. Мапим значения для передачи (0-255)
  // Вместо map(..., 0, 255):
  uint8_t joyX = (uint8_t)map((int)smoothX, 0, 1023, 1, 254);
  uint8_t joyY = (uint8_t)map((int)smoothY, 0, 1023, 1, 254);

  // 5. ОТПРАВКА ПАКЕТА
  // KuroJoycon.ino — новый пакет (5 байт)
  Serial.write(0xFF);  // Байт 0: СТАРТ-маркер (стик никогда не даст 0xFF если ограничить до 254)
  Serial.write(mask);  // Байт 1: Кнопки
  Serial.write(joyX);  // Байт 2: Ось X  
  Serial.write(joyY);  // Байт 3: Ось Y
  Serial.write(0xAA);  // Байт 4: СТОП-маркер

  // Задержка 10мс дает частоту опроса 100Гц - идеально для игр
  delay(10);
}
