float smoothX = 512;
float smoothY = 512;
const float filterAlpha = 0.2; 

int centerX = 512;
int centerY = 512;

void setup() {
  Serial.begin(115200);
  pinMode(2, INPUT_PULLUP); // Обязательно для кнопки стика

  // Авто-калибровка центра при включении
  long sumX = 0, sumY = 0;
  for(int i = 0; i < 30; i++) {
    sumX += analogRead(A0);
    sumY += analogRead(A1);
    delay(10);
  }
  centerX = sumX / 30;
  centerY = sumY / 30;
  
  // Инициализируем фильтр начальными значениями центра
  smoothX = centerX;
  smoothY = centerY;
}

void loop() {
  int rawX = analogRead(A0);
  int rawY = analogRead(A1);

  // Сглаживание
  smoothX = (rawX * filterAlpha) + (smoothX * (1.0 - filterAlpha));
  smoothY = (rawY * filterAlpha) + (smoothY * (1.0 - filterAlpha));

  // Отправляем данные
  // Мы отправляем сырые сглаженные данные, 
  // а RP2040 уже решит, где там мертвая зона.
  Serial.print((int)smoothX);
  Serial.print(",");
  Serial.print((int)smoothY);
  Serial.print(",");
  Serial.print(digitalRead(2) == LOW ? 1 : 0);
  Serial.println(";");
  
  delay(10);
}