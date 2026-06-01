#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <SD.h>
#include <SPI.h>
#include <RTClib.h>
#include <avr/sleep.h>
#include <Wire.h>

#define ONE_WIRE_BUS 3
#define TEMPERATURE_PRECISION 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
DeviceAddress one;



void setup() {
    Serial.begin(9600);
    const int testPin = 3;
    pinMode(testPin, INPUT);

    tempSensors.begin();
    tempSensors.setResolution(TEMPERATURE_PRECISION);
    if (!tempSensors.getAddress(one, 0)) {
    Serial.println("Sensor not found");
        }


}
void loop() {
    // Teste die Funktionalität der Sensoren
    tempSensors.requestTemperatures();
    float tempC = tempSensors.getTempC(one);
    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.println(" °C");

    delay(2000); // Warte 2 Sekunden bevor die nächste Messung durchgeführt wird
}