const unsigned int clockPin = 6;
const unsigned int latchPin = 5;
const unsigned int dataPin = 4;
const unsigned int PWMPin = 3;

void setup() {
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(PWMPin, OUTPUT);
  analogWrite(PWMPin, 255);

  Serial.begin(9600);
  Serial.println("Shift register debug");
}

void loop() {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, 0b11111101);
  //shiftOut(dataPin, clockPin, MSBFIRST, _b2);
  //shiftOut(dataPin, clockPin, MSBFIRST, _b3);
  //shiftOut(dataPin, clockPin, MSBFIRST, _b4);
  digitalWrite(latchPin, HIGH);
}

