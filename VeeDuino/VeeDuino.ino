#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <max6675.h>

// Hardware Constants
#define THERMO_CS   5       // Thermocouple chip select
#define THERMO_SO   4       // Thermocouple MISO (SPI out)
#define THERMO_SCK  6       // Thermocouple SCK (SPI clock)
#define MAP_PIN     A3      // MAP sensor analog input
#define FREQ_PIN    2       // Digital pin for frequency measurement
#define SCREEN_ADDR 0x3C    // I2C address for display
#define DISPLAY_REFRESH 100 // Update interval in milliseconds

// Sensor Constants
#define VOLTS_PER_COUNT 0.00488758553  // 5V/1023
#define MAP_SLOPE 61.5907              // kPa/V
#define MAP_OFFSET 1.1044              // kPa
#define KPA_TO_PSI 0.145038

// Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DEFAULT_CONTRAST 2  // Brightness level (0-255)
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MAX6675 thermocouple(THERMO_SCK, THERMO_CS, THERMO_SO);

// Timing Control
unsigned long lastUpdate = 0;
float atmosphericPressure = 0.0;      // Stores baseline pressure

// Frequency measurement globals
volatile unsigned long lastRiseTime = 0;
volatile unsigned long period = 0;
volatile bool newPeriodAvailable = false;
float frequencyValue = 0.0; // Frequency in Hz

// Interrupt Service Routine for frequency measurement
void measurePeriod() {
  unsigned long currentTime = micros();
  if (lastRiseTime > 0) {
    period = currentTime - lastRiseTime;
    newPeriodAvailable = true;
  }
  lastRiseTime = currentTime;
}

void setup() {
  Serial.begin(9600);
  
  // Calibrate atmospheric pressure from MAP sensor
  calibrateAtmosphericPressure();
  
  // Initialize the display
  initializeDisplay();

  // Set up the frequency measurement pin
  pinMode(FREQ_PIN, INPUT);
  // Attach an interrupt to FREQ_PIN on the rising edge
  attachInterrupt(digitalPinToInterrupt(FREQ_PIN), measurePeriod, RISING);
}

void loop() {
  // Update frequency value if a new measurement is available
  if (newPeriodAvailable) {
    noInterrupts();
    unsigned long measuredPeriod = period;
    newPeriodAvailable = false;
    interrupts();
    
    if ((measuredPeriod > 0) & (((1000000.0 / measuredPeriod) * 4.2017 / 2) < 8000)) {
      frequencyValue = 1000000.0 / measuredPeriod; // period is in microseconds
    }
  }
  
  if(millis() - lastUpdate >= DISPLAY_REFRESH) {
    updateDisplay(
      readMAP(),
      readEGT(),
      frequencyValue
    );
    lastUpdate = millis();
  }
}

float readMAP() {
  int raw = analogRead(MAP_PIN);
  float voltage = raw * VOLTS_PER_COUNT;
  float absolutePSI = ((MAP_SLOPE * voltage) + MAP_OFFSET) * KPA_TO_PSI;
  return absolutePSI - atmosphericPressure;  // Return relative pressure in PSI
}

float readEGT() {
  static unsigned long lastValidRead = 0;
  static float lastGoodTemp = 0;
  
  float temp = thermocouple.readCelsius();
  
  if(!isnan(temp)) {
    lastGoodTemp = temp;
    lastValidRead = millis();
  }
  else if(millis() - lastValidRead > 5000) {
    lastGoodTemp = NAN;
  }
  
  return lastGoodTemp;
}

void updateDisplay(float boost, float temp, float freq) {
  display.clearDisplay();
  
  // Display Boost Pressure with Sign at the top (y=10)
  display.setCursor(0, 8);
  if(boost >= 0) {
    display.print(F("+"));  // Show positive sign explicitly
  }
  display.print(boost, 1);  // One decimal place
  display.print(F(" PSI"));
  
  // Display EGT reading at y=30
  display.setCursor(0, 24);
  if(!isnan(temp)) {
    display.print("EGT: ");
    display.print(temp, 0);
    display.print(F(" C"));
  } else {
    display.print(F("Sensor Err"));
  }
  
  // Display Frequency reading at y=50
  display.setCursor(0, 40);
  display.print(freq * 4.2017 / 2, 0);
  display.print(" RPM");
  
  display.display();
}

void calibrateAtmosphericPressure() {
  // Average multiple readings to get a baseline pressure
  float sum = 0;
  for(int i = 0; i < 100; i++) {
    int raw = analogRead(MAP_PIN);
    float voltage = raw * VOLTS_PER_COUNT;
    sum += (MAP_SLOPE * voltage) + MAP_OFFSET; // Pressure in kPa
    delay(10);
  }
  
  // Convert average kPa to PSI and store as atmospheric pressure
  atmosphericPressure = (sum / 100) * KPA_TO_PSI;
  Serial.print("Calibrated atmospheric pressure: ");
  Serial.println(atmosphericPressure);
}

void initializeDisplay() {
  if(!display.begin(SCREEN_ADDR, true)) {
    Serial.println(F("Display init failed!"));
    while(true);
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setRotation(0);
  display.setContrast(DEFAULT_CONTRAST);  // Set display brightness
}