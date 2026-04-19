#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <SD.h>
#include <SPI.h>
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
DeviceAddress one;


const int chipSelect = 10;
unsigned long startTime;
bool write = true;
bool del = false;

long lastWriteTime = 0;
const unsigned long writeInterval = 1000 * 5 * 1;


void setup()
{
  Serial.begin(9600);

  pinMode(2, INPUT);
  pinMode(6, OUTPUT);
  tempSensors.begin();
  tempSensors.setResolution(TEMPERATURE_PRECISION);
  if (!tempSensors.getAddress(one, 0)) {
    Serial.println("Unable to find address for Device 0");
  }

  startTime = millis();
   


  if (!SD.begin(chipSelect)) {
    Serial.println("SD Karte wurde nicht erkannt!");
    return;
  }
  Serial.println("SD Karte erkannt.");


  if (del && SD.remove("datalog.txt")) {
    Serial.println("datalog.txt gelöscht");
  }



  File readFile = SD.open("datalog.txt", FILE_READ);

  if (readFile) {
    Serial.println("Datei Inhalt: ");
    while (readFile.available()) {
  
      
      Serial.write(readFile.read());
    }
    readFile.close();
  } else {
    Serial.println("Error opening datalog.txt for reading");
  }
}

int getAvarage(int pin) {
  const int numReadings = 10;
  int readings[numReadings];
  int total = 0;

  for (int i = 0; i < numReadings; i++) {
    readings[i] = analogRead(pin);
    total += readings[i];
    delay(100); // Delay zwischen Reading
  }

  int average = total / numReadings;
  return average;
}

float getAvarageTemp() {
  const int numReadings = 10;
  float readings[numReadings];
  float total = 0;

  for (int i = 0; i < numReadings; i++) {
    tempSensors.requestTemperatures();
    readings[i] = tempSensors.getTempC(one);
    total += readings[i];
    delay(100); // Delay zwischen Readings
  }

  float average = total / numReadings;
  return average;
}


void writeData() {

    int moistureValue = getAvarage(A0);
    float tempValue = getAvarageTemp();
    int lightValue = getAvarage(A1);


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
      dataFile.println(millis() - startTime);
      dataFile.close();

    } 
}


void loop()
{

    if (millis() - lastWriteTime >= writeInterval) {
        writeData();
        lastWriteTime = millis();
    }
    digitalWrite(6, HIGH);
   
}



