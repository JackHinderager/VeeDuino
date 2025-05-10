#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <max6675.h>

// Hardware Constants
#define THERMO_CS   5
#define THERMO_SO   4
#define THERMO_SCK  6
#define MAP_PIN     A3
#define FREQ_PIN    2
#define SCREEN_ADDR 0x3C
#define DISPLAY_REFRESH 100 // ms
#define DISPLAY_RESTART_INTERVAL 60000 // Restart every 60,000 ms (1 min)

// Sensor Constants
#define VOLTS_PER_COUNT 0.00488758553
#define MAP_SLOPE       61.5907
#define MAP_OFFSET      1.1044
#define KPA_TO_PSI      0.145038
#define SEA_LEVEL_PSI   14.696

// Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DEFAULT_CONTRAST 2
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MAX6675 thermocouple(THERMO_SCK, THERMO_CS, THERMO_SO);

// Timing Control
unsigned long lastUpdate = 0;
unsigned long lastDisplayRestart = 0;

// Frequency measurement globals
volatile unsigned long lastRiseTime = 0;
volatile unsigned long period = 0;
volatile bool newPeriodAvailable = false;
float frequencyValue = 0.0;

// EMA globals
float emaMAP = 0, emaEGT = 0, emaRPM = 0;
const float alpha = 0.2;
bool firstEMA = true;

void measurePeriod() {
  unsigned long now = micros();
  if (lastRiseTime > 0) {
    period = now - lastRiseTime;
    newPeriodAvailable = true;
  }
  lastRiseTime = now;
}

void setup() {
  Serial.begin(9600);
  initializeDisplay();

  pinMode(FREQ_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(FREQ_PIN), measurePeriod, RISING);
  lastDisplayRestart = millis();
}

void loop() {
  // Frequency update
  if (newPeriodAvailable) {
    noInterrupts(); unsigned long p = period; newPeriodAvailable = false; interrupts();
    if (p > 0 && ((1e6 / p) * 4.2017 / 2) < 8000) frequencyValue = 1e6 / p;
  }

  // Periodic display restart
  if (millis() - lastDisplayRestart >= DISPLAY_RESTART_INTERVAL) {
    initializeDisplay();
    lastDisplayRestart = millis();
  }

  // Read & smooth at refresh
  if (millis() - lastUpdate >= DISPLAY_REFRESH) {
    float rawMAP = readMAP();
    float rawEGT = readEGT();
    float rawRPM = frequencyValue * 4.2017 / 2;

    if (firstEMA) {
      emaMAP = rawMAP; emaEGT = rawEGT; emaRPM = rawRPM;
      firstEMA = false;
    } else {
      emaMAP = alpha * rawMAP + (1 - alpha) * emaMAP;
      emaEGT = alpha * rawEGT + (1 - alpha) * emaEGT;
      // emaRPM = alpha * rawRPM + (1 - alpha) * emaRPM; // Systemic error (frequency issue) causes low readings at higher rpms
      emaRPM = rawRPM; // Hack for now
    }

    updateDisplay(emaMAP, emaEGT, emaRPM);
    lastUpdate = millis();
  }
}

float readMAP() {
  int raw = analogRead(MAP_PIN);
  float volts = raw * VOLTS_PER_COUNT;
  float psi = ((MAP_SLOPE * volts) + MAP_OFFSET) * KPA_TO_PSI;
  return psi - SEA_LEVEL_PSI;
}

float readEGT() {
  static unsigned long lastGoodTime = 0;
  static float lastTemp = 0;
  float t = thermocouple.readCelsius();
  if (!isnan(t)) { lastTemp = t; lastGoodTime = millis(); }
  else if (millis() - lastGoodTime > 5000) lastTemp = NAN;
  return lastTemp;
}

void updateDisplay(float boost, float temp, float rpm) {
  display.clearDisplay();
  display.setCursor(0, 8);
  if (boost >= 0) display.print(F("+")); display.print(boost,1); display.print(F(" PSI"));
  display.setCursor(0,24);
  if (!isnan(temp)) { display.print(F("EGT: ")); display.print(temp,0); display.print(F(" C")); }
  else display.print(F("Sensor Err"));
  display.setCursor(0,40);
  display.print(rpm,0); display.print(F(" RPM"));
  display.display();
}

void initializeDisplay() {
  if (!display.begin(SCREEN_ADDR, true)) {
    Serial.println(F("Display init failed!")); while(true);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setRotation(0);
  display.setContrast(DEFAULT_CONTRAST);
}
