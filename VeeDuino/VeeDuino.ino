#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <max6675.h>

// Hardware Constants
#define THERMO_CS   5       // Thermocouple chip select
#define THERMO_SO   4       // Thermocouple MISO (SPI out)
#define THERMO_SCK  6       // Thermocouple SCK (SPI clock)
#define MAP_PIN     A3      // MAP sensor analog input
#define SCREEN_ADDR 0x3C    // I2C address for display
#define DISPLAY_REFRESH 250 // Update interval in milliseconds

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

void setup() {
  Serial.begin(9600);
  calibrateAtmosphericPressure();    // Calibrate atmospheric pressure
  initializeDisplay();
}

void calibrateAtmosphericPressure() {
  // Average multiple readings to get baseline pressure
  float sum = 0;
  for(int i = 0; i < 100; i++) {
    int raw = analogRead(MAP_PIN);
    float voltage = raw * VOLTS_PER_COUNT;
    sum += (MAP_SLOPE * voltage) + MAP_OFFSET; // Pressure in kPa
    delay(10);
  }
  
  // Convert average kPa to PSI and store
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

void loop() {
  if(millis() - lastUpdate >= DISPLAY_REFRESH) {
    updateDisplay(
      readMAP(),
      readEGT()
    );
    lastUpdate = millis();
  }
}

float readMAP() {
  int raw = analogRead(MAP_PIN);
  float voltage = raw * VOLTS_PER_COUNT;
  float absolutePSI = ((MAP_SLOPE * voltage) + MAP_OFFSET) * KPA_TO_PSI;
  return absolutePSI - atmosphericPressure;  // Return relative pressure
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

void updateDisplay(float boost, float temp) {
  display.clearDisplay();
  
  // Display Boost Pressure with Sign
  display.setCursor(0, 10);
  if(boost >= 0) {
    display.print(F("+"));  // Explicitly show positive sign
  }
  display.print(boost, 1);  // Show 1 decimal place
  display.print(F(" PSI"));

  // Display EGT with error handling
  display.setCursor(0, 30);
  if(!isnan(temp)) {
    display.print("EGT: ");
    display.print(temp, 0);
    display.print(F("C"));
  } else {
    display.print(F("Sensor Err"));
  }
  
  display.display();
}