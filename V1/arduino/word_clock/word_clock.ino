#include <SPI.h>
#include <Adafruit_NeoPixel.h>

#define SECONDS_READ  0x00
#define SECONDS_WRITE 0x80
#define MINUTES_READ  0x01
#define MINUTES_WRITE 0x81
#define HOURS_READ    0x02
#define HOURS_WRITE   0x82
#define DAY_READ      0x03
#define DAY_WRITE     0x83
#define DATE_READ     0x04
#define DATE_WRITE    0x84
#define MONTH_READ    0x05
#define MONTH_WRITE   0x85
#define YEAR_READ     0x06
#define YEAR_WRITE    0x86

#define CONTROL_READ  0x0E
#define CONTROL_WRITE 0x8E
#define STATUS_READ   0x0F
#define STATUS_WRITE  0x8F
#define OSC_AGE_READ  0x10

#define HOURS_12_OR_24_BIT 6
#define HOURS_AM_OR_PM_BIT 5

const unsigned int LEDStripPin = 10;
const unsigned int RTCSlaveSelectPin = 8; // nb: active low
const unsigned int lightSensorInputPin = A0;
const unsigned int ClockFaceClockPin = 6;
const unsigned int ClockFaceLatchPin = 5;
const unsigned int ClockFaceDataPin = 4;
const unsigned int ClockFaceLEDsPWMPin = 3;
const unsigned int hoursButtonPin = 2;
const unsigned int minutesButtonPin = 7;

const byte SabBirthdayDay = 28;
const byte SabBirthdayMonth = 3;
const byte NicoBirthdayDay = 19;
const byte NicoBirthdayMonth = 7;
const byte AnniversaryDay = 14;
const byte AnniversaryMonth = 9;

// helper class to pull SC pin low (enable) and high (disable) on enter / leave of containing scope
class DS3234RTC_SPITransaction
{
  public:
    DS3234RTC_SPITransaction()
    {
      digitalWrite(RTCSlaveSelectPin, LOW);
    };
    ~DS3234RTC_SPITransaction()
    {
      digitalWrite(RTCSlaveSelectPin, HIGH);
    };
};

void SPI_setup() {
  SPI.begin();
  // DS3234 specs : 4Mhz (i.e. 1/4 of ATMega328's 16Mhz clock), mode 1 or 3 supported (CPOL 0 or 1, CPHA 1), most data transfers MSB first (except temperature ?)
  // NB: it looks like DS3234 operates at 2Mhz on battery backup (during a power failure) so we might want to set to /8... but SPI comms also seem to be disabled during battery backup, as per the specs sheet
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  SPI.setDataMode(SPI_MODE1);
  SPI.setBitOrder(MSBFIRST);
}

void setTime(byte h, byte m, byte s) {
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(SECONDS_WRITE); // send address; return is meaningless
    byte secondsBCD = (s / 10 << 4) | s % 10;
    SPI.transfer(secondsBCD);
  }
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(MINUTES_WRITE); // send address; return is meaningless
    byte minutesBCD = (m / 10 << 4) | m % 10;
    SPI.transfer(minutesBCD);
  }
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(HOURS_WRITE); // send address; return is meaningless
    byte hoursBCD = (h / 10 << 4) | h % 10; // bit 6 will default to low, i.e. 24 hours mode
    SPI.transfer(hoursBCD);
  }
}

void setDate(byte d, byte m, byte y) {
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(DATE_WRITE); // send address; return is meaningless
    byte dateBCD = (d / 10 << 4) | d % 10;
    SPI.transfer(dateBCD);
  }
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(MONTH_WRITE); // send address; return is meaningless
    byte monthBCD = (m / 10 << 4) | m % 10;
    SPI.transfer(monthBCD);
  }
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(YEAR_WRITE); // send address; return is meaningless
    byte yearBCD = (y / 10 << 4) | y % 10;
    SPI.transfer(yearBCD);
  }
}

void GetTime(byte& oHours, byte& oMinutes, byte& oSeconds)
{
  byte BCDseconds;
  byte BCDminutes;
  byte BCDhours;
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(SECONDS_READ); // send address; return is meaningless
    BCDseconds = SPI.transfer(0x0); // read seconds; sent value is meaningless
    BCDminutes = SPI.transfer(0x0); // read minutes; sent value is meaningless
    BCDhours = SPI.transfer(0x0); // read minutes; sent value is meaningless
  }
  oSeconds = (((BCDseconds & 0b01110000) >> 4) * 10) + (BCDseconds & 0b00001111);
  oMinutes = (((BCDminutes & 0b01110000) >> 4) * 10) + (BCDminutes & 0b00001111);
  if (bitRead(BCDhours, HOURS_12_OR_24_BIT))
  { // high : 12 hour mode
    if (bitRead(BCDhours, HOURS_AM_OR_PM_BIT))
    { // high : PM
      oHours = (((BCDhours & 0b00010000) >> 4) * 10) + (BCDhours & 0b00001111) + 12;
    }
    else
    { // low : AM
      oHours = (((BCDhours & 0b00010000) >> 4) * 10) + (BCDhours & 0b00001111);
    }
  }
  else
  { // low 24 hour mode
    //Serial.println("24 hour mode");
    oHours = (((BCDhours & 0b00110000) >> 4) * 10) + (BCDhours & 0b00001111);
  }
}

void GetDate(byte& oDate, byte& oMonth, byte& oYear) {
  byte BCDdate;
  byte BCDmonth;
  byte BCDyear;
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(DATE_READ); // initiate multi byte read; will read out addresses starting from this one in sequence
    BCDdate = SPI.transfer(0x0);
    BCDmonth = SPI.transfer(0x0);
    BCDyear = SPI.transfer(0x0);
  }
  oDate = (((BCDdate & 0b00110000) >> 4) * 10) + (BCDdate & 0b00001111);
  oMonth = (((BCDmonth & 0b00010000) >> 4) * 10) + (BCDmonth & 0b00001111);
  // technically, we should probably read the century overflow bit (MSB of BCDmonth) but I doubt this will still run in 2100 :p
  oYear = (((BCDyear & 0b11110000) >> 4) * 10) + (BCDyear & 0b00001111);
}

// Input a value 0 to 255 to get a color value. Cycles from R to G to B and back to R.
uint32_t Wheel(byte WheelPos) {
  if (WheelPos < 85) {
    return Adafruit_NeoPixel::Color(255 - WheelPos * 3, WheelPos * 3, 0);
  }
  else if (WheelPos < 170) {
    WheelPos -= 85;
    return Adafruit_NeoPixel::Color(0, 255 - WheelPos * 3, WheelPos * 3);
  }
  else {
    WheelPos -= 170;
    return Adafruit_NeoPixel::Color(WheelPos * 3, 0, 255 - WheelPos * 3);
  }
}

byte LED_STRIP_WORD(Adafruit_NeoPixel &iStrip, unsigned short iStartPos, unsigned short iLength, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  for (unsigned short i = iStartPos; i < iStartPos + iLength; ++i) {
    if (iColorLength)
      iStrip.setPixelColor(i, Wheel((i - iStartPos) * (255 / iColorLength) + iColorOrStep));
    else
      iStrip.setPixelColor(i, iColorOrStep);
  }
  return iLength * 255 / iColorLength + iColorOrStep;
}

byte HAPPY(Adafruit_NeoPixel& iStrip, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  return LED_STRIP_WORD(iStrip, 3, 5, iColorOrStep, iColorLength);
}

byte A(Adafruit_NeoPixel& iStrip, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  return LED_STRIP_WORD(iStrip, 0, 1, iColorOrStep, iColorLength);
}

byte BY(Adafruit_NeoPixel& iStrip, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  return LED_STRIP_WORD(iStrip, 1, 2, iColorOrStep, iColorLength);
}

byte BIRTHDAY(Adafruit_NeoPixel& iStrip, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  return LED_STRIP_WORD(iStrip, 8, 8, iColorOrStep, iColorLength);
}

byte DAY(Adafruit_NeoPixel& iStrip, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  return LED_STRIP_WORD(iStrip, 8, 3, iColorOrStep, iColorLength);
}

byte NICO (Adafruit_NeoPixel& iStrip, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  return LED_STRIP_WORD(iStrip, 17, 4, iColorOrStep, iColorLength);
}

byte SABRINA (Adafruit_NeoPixel& iStrip, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  return LED_STRIP_WORD(iStrip, 21, 7, iColorOrStep, iColorLength);
}

byte AMPERSAND (Adafruit_NeoPixel& iStrip, uint32_t iColorOrStep, unsigned short iColorLength = 0) {
  return LED_STRIP_WORD(iStrip, 16, 1, iColorOrStep, iColorLength);
}

class ClockFace {
  public:
    ClockFace(const unsigned int iLatch, const unsigned int iData, const unsigned int iClock, const unsigned int iPWM) : _latchPin(iLatch), _dataPin(iData), _clockPin(iClock), _PWMPin(iPWM), _b1(0), _b2(0), _b3(0), _b4(0) {};

    void begin() {
      pinMode(_clockPin, OUTPUT);
      pinMode(_latchPin, OUTPUT);
      pinMode(_dataPin, OUTPUT);
      pinMode(_PWMPin, OUTPUT);
      setBrightness(50);
    };

    ClockFace& IT()      {
      _b1 |= 0b00000001;
      return *this;
    };
    ClockFace& IS()      {
      _b1 |= 0b00000010;
      return *this;
    };
    ClockFace& TEN_M()   {
      _b1 |= 0b00000100;
      return *this;
    };
    ClockFace& HALF()    {
      _b1 |= 0b00001000;
      return *this;
    };
    ClockFace& QUARTER() {
      _b1 |= 0b00010000;
      return *this;
    };
    ClockFace& TWENTY()  {
      _b1 |= 0b00100000;
      return *this;
    };
    ClockFace& FIVE_M()  {
      _b1 |= 0b01000000;
      return *this;
    };

    ClockFace& MINUTES() {
      _b2 |= 0b00000001;
      return *this;
    };
    ClockFace& PAST()    {
      _b2 |= 0b00000010;
      return *this;
    };
    ClockFace& TO()      {
      _b2 |= 0b00000100;
      return *this;
    };
    ClockFace& THREE()   {
      _b2 |= 0b00001000;
      return *this;
    };
    ClockFace& ELEVEN()  {
      _b2 |= 0b00010000;
      return *this;
    };
    ClockFace& FOUR()    {
      _b2 |= 0b00100000;
      return *this;
    };
    ClockFace& ONE()     {
      _b2 |= 0b01000000;
      return *this;
    };

    ClockFace& TWO()   {
      _b3 |= 0b00000001;
      return *this;
    };
    ClockFace& EIGHT() {
      _b3 |= 0b00000010;
      return *this;
    };
    ClockFace& NINE()  {
      _b3 |= 0b00000100;
      return *this;
    };
    ClockFace& SEVEN() {
      _b3 |= 0b00001000;
      return *this;
    };
    ClockFace& FIVE()  {
      _b3 |= 0b00010000;
      return *this;
    };
    ClockFace& SIX()   {
      _b3 |= 0b00100000;
      return *this;
    };
    ClockFace& TEN()   {
      _b3 |= 0b01000000;
      return *this;
    };

    ClockFace& TWELVE() {
      _b4 |= 0b00000100;
      return *this;
    };
    ClockFace& O()      {
      _b4 |= 0b00000010;
      return *this;
    };
    ClockFace& CLOCK()  {
      _b4 |= 0b00000001;
      return *this;
    };
    ClockFace& MIN1()   {
      _b4 |= 0b00001000;
      return *this;
    };
    ClockFace& MIN2()   {
      _b4 |= 0b00010000;
      return *this;
    };
    ClockFace& MIN3()   {
      _b4 |= 0b00100000;
      return *this;
    };
    ClockFace& MIN4()   {
      _b4 |= 0b01000000;
      return *this;
    };

    ClockFace& setBrightness(byte iBrightness) {
      analogWrite(_PWMPin, iBrightness);
    };

    ClockFace& show() {
      digitalWrite(_latchPin, LOW);
      shiftOut(_dataPin, _clockPin, MSBFIRST, _b1);
      shiftOut(_dataPin, _clockPin, MSBFIRST, _b2);
      shiftOut(_dataPin, _clockPin, MSBFIRST, _b3);
      shiftOut(_dataPin, _clockPin, MSBFIRST, _b4);
      digitalWrite(_latchPin, HIGH);
      return *this;
    };

    ClockFace& clear() {
      _b1 = 0; _b2 = 0; _b3 = 0; _b4 = 0; return *this;
    }

  private:
    byte _b1; // IT IS TEN HALF QUARTER TWENTY FIVE
    byte _b2; // MINUTES PAST TO THREE ELEVEN FOUR ONE
    byte _b4; // TWO EIGHT NINE SEVEN FIVE SIX TEN
    byte _b3; // TWELVE O CLOCK MIN1 MIN2 MIN3 MIN4

    const unsigned int _clockPin;
    const unsigned int _latchPin;
    const unsigned int _dataPin;
    const unsigned int _PWMPin;
};


// A + HAPPY + BY + BIRTHDAY + NICO + & + BIRTHDAY
Adafruit_NeoPixel strip(28, LEDStripPin, NEO_GRB + NEO_KHZ800);
ClockFace clockFace(ClockFaceLatchPin, ClockFaceDataPin, ClockFaceClockPin, ClockFaceLEDsPWMPin);

void startupSequence(uint32_t iColor, unsigned short iDelay) {
  strip.clear();
  strip.show();
  delay(100);

  A(strip, iColor);
  strip.show();
  delay(iDelay);
  strip.clear();
  strip.show();

  clockFace.setBrightness(50);
  clockFace.CLOCK().show();
  delay(iDelay);
  clockFace.clear().show();

  BY(strip, iColor);
  strip.show();
  delay(iDelay);
  strip.clear();
  strip.show();

  SABRINA(strip, iColor);
  strip.show();
  delay(iDelay);
  strip.clear();
  strip.show();

  AMPERSAND(strip, iColor);
  strip.show();
  delay(iDelay);
  strip.clear();
  strip.show();

  NICO(strip, iColor);
  strip.show();
  delay(1000);
  strip.clear();
  strip.show();
}

void IncrementHours() {
  byte BCDhours;
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(HOURS_READ); // send address; return is meaningless
    BCDhours = SPI.transfer(0x0); // read hours; sent value is meaningless
  }
  unsigned int currentHours = (((BCDhours & 0b00110000) >> 4) * 10) + (BCDhours & 0b00001111);
  currentHours = (currentHours == 23) ? 0 : currentHours + 1;
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(HOURS_WRITE); // send address; return is meaningless
    byte hoursBCD = (currentHours / 10 << 4) | currentHours % 10; // bit 6 will default to low, i.e. 24 hours mode
    SPI.transfer(hoursBCD);
  }

  if (currentHours < 12) {
    A(strip, 0xFF0000);
    strip.setBrightness(255);
    strip.show();
    delay(500);
    strip.clear();
    strip.show();
  }
}

void IncrementMinutes() {
  byte BCDminutes;
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(MINUTES_READ); // send address; return is meaningless
    BCDminutes = SPI.transfer(0x0); // read minutes; sent value is meaningless
  }
  unsigned int currentMinutes = (((BCDminutes & 0b01110000) >> 4) * 10) + (BCDminutes & 0b00001111);
  currentMinutes = (currentMinutes == 59) ? 0 : currentMinutes + 1;

  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(MINUTES_WRITE); // send address; return is meaningless
    byte minutesBCD = (currentMinutes / 10 << 4) | currentMinutes % 10;
    SPI.transfer(minutesBCD);
  }
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(SECONDS_WRITE); // send address; return is meaningless
    byte secondsBCD = 0;
    SPI.transfer(secondsBCD);
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(LEDStripPin, OUTPUT);
  pinMode(RTCSlaveSelectPin, OUTPUT);
  pinMode(lightSensorInputPin, INPUT);
  pinMode(hoursButtonPin, INPUT_PULLUP);
  pinMode(minutesButtonPin, INPUT_PULLUP);

  clockFace.begin();
  clockFace.clear().show();

  strip.begin();
  strip.clear();
  strip.setBrightness(50);
  strip.show();

  SPI_setup();

  // turn the RTC chip off (SS pin is active low) ("off" from an SPI standpoint; the clock itself still runs) at the start. Each SPI transfer will start by pulling this low to enable
  digitalWrite(RTCSlaveSelectPin, HIGH);

  Serial.begin(9600);
  Serial.println("Word Clock V1.0");

  // set up RTC options
  byte options = 0;
  byte stat = 0;
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(CONTROL_READ); // send address; return is meaningless
    options = SPI.transfer(0x0);
    stat = SPI.transfer(0x0);
  }
  Serial.print("Current control register: "); Serial.println(options, BIN);
  Serial.print("Current status register: "); Serial.println(stat, BIN);

  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(STATUS_WRITE); // send address; return is meaningless
    // mask the existing status register with the following settings; in other words, force everything to 0 but keep BUSY (BIT2) in the unlikely event that it was set
    // BIT7:Oscillator Stop Flag 0 BIT6:Battery Backed 32kHz output 0 BIT5:CRATE1 0 BIT4:CRATE0 0 BIT3:Enable 32kHz Output 0 BIT2:BUSY 1 BIT1:Alarm 2 Flag 0 BIT0: Alarm 1 Flag 0;
    byte status_set = 0b00000100;
    SPI.transfer(stat & status_set);
  }
  
  //setDate(27, 3, 2015);

  startupSequence(0xFFFFFF, 500);
}

unsigned long lastButtonsUpdate = 0;
unsigned int ButtonsUpdateDelay = 5;
unsigned long lastLEDStripUpdate = 0;
unsigned int LEDStripUpdateDelay = 10;
const unsigned int LEDStripUpdateDelayMax = 100;
unsigned long lastDateCheckUpdate = 60000; // update at start
const unsigned int DateCheckUpdateDelay = 60000; // check date every minute
unsigned long lastClockUpdate = 0;
const unsigned int ClockUpdateDelay = 1000;
unsigned long lastAmbientLightUpdate = 0;
const unsigned int AmbientLightUpdateDelay = 25;
unsigned long lastBacklightPowerUpdate = 0;
const unsigned int BacklightPowerUpdateDelay = 1000;
byte ambientLightArray[5];
byte ambientLightArrayIdx = 0;
signed short currentBacklightPower = 100;
byte LEDCycleCurrentStep = 0;
bool birthdaySab = false;
bool birthdayNico = false;
bool anniversary = false;

void DoDateCheck() {
  byte d = 0; byte mo = 0; byte y = 0;
  GetDate(d, mo, y);
  if (d == SabBirthdayDay && mo == SabBirthdayMonth)
    birthdaySab = true;
  else
    birthdaySab = false;
  if (d == NicoBirthdayDay && mo == NicoBirthdayMonth)
    birthdayNico = true;
  else
    birthdayNico = false;
  if (d == AnniversaryDay && mo == AnniversaryMonth)
    anniversary = true;
  else
    anniversary = false;

  DebugPrintDate(d, mo, y);
}

void DoDebugChecks() {
  byte stat = 0;
  {
    DS3234RTC_SPITransaction spiTransaction;
    SPI.transfer(STATUS_READ); // send address; return is meaningless
    stat = SPI.transfer(0x0);
  }
  if (bitRead(stat, 7)) {
    Serial.println("WARNING: clock reports oscillator stop; resetting flag");
    {
      DS3234RTC_SPITransaction spiTransaction;
      SPI.transfer(STATUS_WRITE); // send address; return is meaningless
      SPI.transfer(stat & 0b01111111);
    }
  }
}

void DebugPrintTime(unsigned int h, unsigned int m, unsigned int s) {
  String debugTime("");
  debugTime += h; debugTime += ":"; debugTime += m; debugTime += ":"; debugTime += s;
  Serial.println(debugTime);
}

void DebugPrintDate(unsigned int d, unsigned int mo, unsigned int y) {
  String debugDate("");
  debugDate += d; debugDate += "/"; debugDate += mo; debugDate += "/"; debugDate += y;
  Serial.println(debugDate);
}

static bool hoursButton = false;
static bool minutesButton = false;

void DebounceButtons() {
  static unsigned int stateHours = 0;
  static unsigned int stateMinutes = 0;
  unsigned int rawHoursButton = digitalRead(hoursButtonPin);
  unsigned int rawMinutesButton = digitalRead(minutesButtonPin);
  stateHours = (stateHours << 1) | rawHoursButton | 0xe000;
  stateMinutes = (stateMinutes << 1) | rawMinutesButton | 0xe000;
  if (stateHours == 0xf000)
    hoursButton = true;
  else
    hoursButton = false;
  if (stateMinutes == 0xf000)
    minutesButton = true;
  else
    minutesButton = false;
}

void loop() {
  unsigned long currentMillis = millis();

  // check buttons; this should probably be done in an ISR instead for accuracy but we're going to be pressing the buttons once a year so who cares
  if (currentMillis - lastButtonsUpdate > ButtonsUpdateDelay) {
    lastButtonsUpdate = currentMillis;
    DebounceButtons();

    if (hoursButton && minutesButton) {
      birthdaySab = true;
      lastDateCheckUpdate = currentMillis;
    }
    else {
      if (hoursButton) {
        IncrementHours();
      }

      if (minutesButton) {
        IncrementMinutes();
      }
    }
  }

  // check date against birthdays and anniversary
  if (currentMillis - lastDateCheckUpdate > DateCheckUpdateDelay) {
    lastDateCheckUpdate = currentMillis;
    DoDateCheck();
    DoDebugChecks();
  }

  // update LED strip for birthdays and anniversary
  if (birthdayNico || birthdaySab || anniversary) {
    if (currentMillis - lastLEDStripUpdate > LEDStripUpdateDelay) {
      lastLEDStripUpdate = currentMillis;
      strip.clear();
      if (LEDCycleCurrentStep == 255 && LEDStripUpdateDelay < LEDStripUpdateDelayMax)
        ++LEDStripUpdateDelay;
      if (birthdaySab)
        HAPPY(strip, BIRTHDAY(strip, SABRINA(strip, LEDCycleCurrentStep++, 20), 20), 20);
      if (birthdayNico)
        HAPPY(strip, BIRTHDAY(strip, NICO(strip, LEDCycleCurrentStep++, 17), 17), 17);
      if (anniversary)
        A(strip, HAPPY(strip, DAY(strip, LEDCycleCurrentStep++, 9), 9), 9);
      strip.show();
    }
  }
  else { // clear the WS2812 strip if no birthday / anniversary, otherwise it would stay on until the next birthday !
    strip.clear();
    strip.show();
  }

  // update the ambient light reading
  // we only want to do this every second or so but we also want to average readings over
  // a little bit of time and fade to our new target LED power smoothly
  if (currentMillis - lastBacklightPowerUpdate > BacklightPowerUpdateDelay) {
    if (currentMillis - lastAmbientLightUpdate > AmbientLightUpdateDelay) {
      lastAmbientLightUpdate = currentMillis;
      float lightReading = analogRead(lightSensorInputPin);
      //Serial.print("Raw light reading: "); Serial.println(lightReading);
      // scale to 0-255
      int backlightPower = ((lightReading / 10.0f) + 05.0f) * 1.5f;
      //int backlightPower = lightReading / 10.0f;
      if (ambientLightArrayIdx == 5) {
        ambientLightArrayIdx = 0;
        lastBacklightPowerUpdate = currentMillis; // only set this once we've read five values
      }
      ambientLightArray[ambientLightArrayIdx++] = backlightPower;
      unsigned short ambientLightArrayAverage = 0;
      for (byte i = 0; i < 5; ++i)
        ambientLightArrayAverage += ambientLightArray[i];
      ambientLightArrayAverage /= 5;
      //Serial.print("Current light reading: "); Serial.print(backlightPower); Serial.print(" - Average light reading: "); Serial.println(ambientLightArrayAverage);
      if (abs(currentBacklightPower - ambientLightArrayAverage) > (0.05f * currentBacklightPower)) {
        currentBacklightPower = ambientLightArrayAverage;
        clockFace.setBrightness(currentBacklightPower);
        strip.setBrightness(currentBacklightPower / 1.15f);
        //Serial.print("New backlight setting: "); Serial.println(currentBacklightPower);
      }
    }
  }

  // update the clock face
  if (currentMillis - lastClockUpdate > ClockUpdateDelay) {
    lastClockUpdate = currentMillis;

    byte h = 0; byte m = 0; byte s = 0;
    GetTime(h, m, s);
    DebugPrintTime(h, m , s);

    clockFace.clear();
    clockFace.IT().IS();
    switch (m % 5) { // NB: all fall through is by design; if 4 mins, 4,3,2 and 1 need to be lit, if 3 mins, 3,2 and 1, etc.
      case 4:
        clockFace.MIN4();
      case 3:
        clockFace.MIN3();
      case 2:
        clockFace.MIN2();
      case 1:
        clockFace.MIN1();
    }

    if (m < 5)
      clockFace.O().CLOCK();
    else if (m >= 5 && m < 10)
      clockFace.FIVE_M().MINUTES().PAST();
    else if (m >= 10 && m < 15)
      clockFace.TEN_M().MINUTES().PAST();
    else if (m >= 15 && m < 20)
      clockFace.QUARTER().PAST();
    else if (m >= 20 && m < 25)
      clockFace.TWENTY().MINUTES().PAST();
    else if (m >= 25 && m < 30)
      clockFace.TWENTY().FIVE_M().MINUTES().PAST();
    else if (m >= 30 && m < 35)
      clockFace.HALF().PAST();
    else if (m >= 35 && m < 40)
      clockFace.TWENTY().FIVE_M().MINUTES().TO();
    else if (m >= 40 && m < 45)
      clockFace.TWENTY().MINUTES().TO();
    else if (m >= 45 && m < 50)
      clockFace.QUARTER().TO();
    else if (m >= 50 && m < 55)
      clockFace.TEN_M().MINUTES().TO();
    else if (m >= 55)
      clockFace.FIVE_M().MINUTES().TO();

    switch (h) {
      case 0:
      case 12:
        m <= 34 ? clockFace.TWELVE() : clockFace.ONE();
        break;
      case 1:
      case 13:
        m <= 34 ? clockFace.ONE() : clockFace.TWO();
        break;
      case 2:
      case 14:
        m <= 34 ? clockFace.TWO() : clockFace.THREE();
        break;
      case 3:
      case 15:
        m <= 34 ? clockFace.THREE() : clockFace.FOUR();
        break;
      case 4:
      case 16:
        m <= 34 ? clockFace.FOUR() : clockFace.FIVE();
        break;
      case 5:
      case 17:
        m <= 34 ? clockFace.FIVE() : clockFace.SIX();
        break;
      case 6:
      case 18:
        m <= 34 ? clockFace.SIX() : clockFace.SEVEN();
        break;
      case 7:
      case 19:
        m <= 34 ? clockFace.SEVEN() : clockFace.EIGHT();
        break;
      case 8:
      case 20:
        m <= 34 ? clockFace.EIGHT() : clockFace.NINE();
        break;
      case 9:
      case 21:
        m <= 34 ? clockFace.NINE() : clockFace.TEN();
        break;
      case 10:
      case 22:
        m <= 34 ? clockFace.TEN() : clockFace.ELEVEN();
        break;
      case 11:
      case 23:
        m <= 34 ? clockFace.ELEVEN() : clockFace.TWELVE();
        break;
    }
    clockFace.show();
  }
}

