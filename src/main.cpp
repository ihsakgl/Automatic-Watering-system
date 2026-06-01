#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <SD.h>
#include <SPI.h>
#include <RTClib.h>
#include <avr/sleep.h>
#include <Wire.h>

const int maxADC = 1023;

 int moisturePins[] = {A0, A1};
 int lightPins[] = {A2};
const int RTC_SDA_pin = A4;
const int RTC_SCL_pin = A5;

const int RTC_interruptPin = 2; // Pin für RTC Interrupt
const int temperatureSensorPin = 3; // Pin zum Ein-/Ausschalten der Temperatursensoren und SD-Karte
const int valveControlPin = 6; // Pin zum Steuern des Ventils
const int sdControlPin = 7; // Pin zum Steuern der SD-Karte (optional, falls separate Steuerung gewünscht)
const int CS = 10;
const int MOSI_pin = 11;
const int MISO_pin = 12;
const int SCK_pin = 13;

const int sleepIndicatorPin = 4;


#define ONE_WIRE_BUS temperatureSensorPin
#define TEMPERATURE_PRECISION 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
DeviceAddress one;





unsigned long startTime;
bool write = true;
bool read = false;
bool del = false;
bool setTime = false;

long lastWriteTime = 0;
const unsigned long writeInterval = 5;


RTC_DS1307 rtc;

volatile int seconds = 0;
volatile bool wakeUp = false;

void wakeISR() {
  seconds++;

  if (seconds >= 15) {
    seconds = 0;
    wakeUp = true;

  }
}

void goToSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_bod_disable();
  sleep_cpu();
  sleep_disable();
}

void setup()
{
  Serial.begin(9600);


  


  pinMode(temperatureSensorPin, INPUT); // OneWire Bus für Temperatursensoren
  pinMode(sdControlPin, OUTPUT);  // MOSFET SWITCH FOR SENSORS AND SD CARD POWER
  digitalWrite(sdControlPin, HIGH); // Power on sensors and SD card for initialization
  pinMode(valveControlPin, OUTPUT); // MOSFET Valve control pin
  digitalWrite(valveControlPin, LOW); // Ensure valve is closed at startup

  tempSensors.begin();
  tempSensors.setResolution(TEMPERATURE_PRECISION);
  if (!tempSensors.getAddress(one, 0)) {
    Serial.println("Unable to find address for Device 0");
  }

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1);
  }

  Wire.beginTransmission(0x68);
  Wire.write(0x07); // control register
  Wire.write(0x10); // SQW = 1Hz
  Wire.endTransmission();
  delay(100);

  if (setTime) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  startTime = rtc.now().unixtime();
   


  if (!SD.begin(CS)) {
    Serial.println("SD Karte wurde nicht erkannt!");
    return;
  }
  Serial.println("SD Karte erkannt.");


  if (del && SD.remove("datalog.txt")) {
    Serial.println("datalog.txt gelöscht");
  }



  File readFile = SD.open("datalog.txt", FILE_READ);

  if (read && readFile) {
    Serial.println("Datei Inhalt: ");
    while (readFile.available()) {
  
      
      Serial.write(readFile.read());
    }
    readFile.close();
    SD.end();
  } else if (read){
    Serial.println("Error opening datalog.txt for reading");
  }

  digitalWrite(sdControlPin, LOW);
  SPI.end();
  pinMode(CS, INPUT);
  pinMode(MOSI_pin, INPUT);
  pinMode(MISO_pin, INPUT);
  pinMode(SCK_pin, INPUT);


  pinMode(RTC_interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTC_interruptPin), wakeISR, FALLING);

  pinMode(sleepIndicatorPin, OUTPUT);

  Serial.println("Setup complete. Entering micro-nap loop...");
  Serial.flush();

}

int getAverage(int pins[], int numSensors) {
  const int readingsPerSensor = 10;
  long total = 0; // Use 'long' to prevent overflow if the total gets large

  for (int i = 0; i < numSensors; i++) {
    for (int j = 0; j < readingsPerSensor; j++) {
      total += analogRead(pins[i]);
      delay(50); // Reduced to 20ms; 100ms is usually overkill and slows your loop down
    }
  }

  // Calculate the final average: Total readings / (number of sensors * readings per sensor)
  return total / (numSensors * readingsPerSensor);
}

float getAverageTemp() {
  float total = 0;
  int validReadings = 0;

  // 1. Trigger a conversion for ALL sensors on the bus
  tempSensors.requestTemperatures();
  
  // 2. Wait for the 750ms conversion time
  delay(750); 

  // 3. Loop through all 3 sensors (Indices 0, 1, 2)
  for (int i = 0; i < 3; i++) {
    float tempC = tempSensors.getTempCByIndex(i);
    
    // 4. Validate the reading
    // If it returns -127.0, the sensor failed to read (often due to ground noise)
    if (tempC != -127.0) {
      total += tempC;
      validReadings++;
    } else {
      Serial.print("Sensor "); Serial.print(i); Serial.println(" failed to read.");
    }
  }

  // 5. Calculate Average
  if (validReadings > 0) {
    return total / validReadings;
  } else {
    return 0.0; // Return 0 if all sensors failed
  }
}

float getLux(int adcValue) {
    Serial.print("ADC = ");
    Serial.println(adcValue);

    if (adcValue <= 0) return 0.0f;
    if (adcValue >= maxADC) {
        Serial.println("ADC saturated");
        return -1.0f;
    }

    const float A = 246000.0f;
    const float a = 0.9f;
    const float Rfixed = 10000.0f;

    float base = A * adcValue /
                 (Rfixed * (maxADC - adcValue));

    Serial.print("Base = ");
    Serial.println(base, 6);

    float lux = pow(base, 1.0f / a);

    Serial.print("Lux = ");
    Serial.println(lux, 2);

    return lux;
}

void writeData() {

    int moistureValue = getAverage(moisturePins, 2);
    float tempValue = getAverageTemp();

    for (int i = 0; i < 5; i++) {
        analogRead(A2);
        delay(10);
    }
    float lightValue = getLux(getAverage(lightPins, 1));
    // Write to SD card
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
    if (dataFile && write) {
      dataFile.print("Moisture: ");
      dataFile.print(moistureValue);
      dataFile.print(", Temperature: ");
      dataFile.print(tempValue);
      dataFile.print(", Light: ");
      dataFile.print(lightValue);
      dataFile.print(", Time: ");
      dataFile.println(rtc.now().timestamp());
      dataFile.close();

      Serial.println("Data written to SD card: Moisture: " + String(moistureValue) + ", Temperature: " + String(tempValue) + ", Light: " + String(lightValue) + ", Time: " + rtc.now().timestamp());
     
    } 
}

void softResetSD() {
    // !!! NO MISO PULSE (short to GND) 
    pinMode(SCK_pin, OUTPUT);
    pinMode(MOSI_pin, OUTPUT);
    for (int i = 0; i < 80; i++) {
        digitalWrite(SCK_pin, HIGH);
        delayMicroseconds(10);
        digitalWrite(SCK_pin, LOW);
        delayMicroseconds(10);
    }
}


void loop()
{
    if (wakeUp) {
        wakeUp = false;
        Serial.println("Waking up...");
        digitalWrite(sleepIndicatorPin, HIGH); // Indicate wake-up with LED



        // 2. Power on sensors and SD card
        digitalWrite(sdControlPin, HIGH);  
        delay(3000); // Wait for components to stabilize voltage

        // 2. Set pins
        pinMode(CS, OUTPUT);
        digitalWrite(CS, HIGH); // CS MUST be HIGH for the reset to be valid
        pinMode(SCK_pin, OUTPUT);
        pinMode(MOSI_pin, OUTPUT);

        // 3. Perform the reset
        softResetSD();

        SPI.begin();  

        if (SD.begin(CS)) {
          tempSensors.begin();
          tempSensors.setResolution(TEMPERATURE_PRECISION);
          tempSensors.getAddress(one, 0);

          // Read and save data
          writeData();
        } else {
          Serial.println("SD Initialization Failed after power-up!");
        }

 


        // 5. Allow serial prints to finish
        delay(500); 
        SD.end(); // Properly close SD card to ensure data integrity

        // 6. Power off sensors and SD card
        digitalWrite(sdControlPin, LOW); 

        // 7. Prevent phantom power bleeding
        SPI.end(); // Completely disables the hardware SPI peripheral 
        
        // Turn SPI pins into high-impedance inputs
        pinMode(CS, INPUT);
        pinMode(MOSI_pin, INPUT);
        pinMode(MISO_pin, INPUT);
        pinMode(SCK_pin, INPUT);
        pinMode(temperatureSensorPin, INPUT);  

        digitalWrite(sleepIndicatorPin, LOW); // Turn off wake-up indicator

    }
    
    // Ensure the Serial buffer is completely empty before shutting down
    Serial.flush(); 


    
    // Put MCU back to sleep
    goToSleep();

  
   
} 