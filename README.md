# Brauhelferlein

Automatisierung des Maische Kochens mit minimalem Aufwand für 10€.

## Funktionen

Temperatur PID Regelung auf zehntel Grad genau

- Rührwerk Drehzahlregelung
- Rührwerk Interwall Steuerung
- Graphen für Temperatur Verlauf
- Rast Zeit

### Screenshot

![Screenshot](screenshot.png)

## Anleitung

### Benötigt wird

- ein ESP8266
- ein Relais
- ein DS18b20 Messfühler
- zur Steuerung des Maische Rührwerks (Scheibenwischermotor) einen Motor Treiber zB. VNH2SP30 
- optional ein Display (LCD I2C 1602).

### Software

- Arduino 1.6.x
- add [ESP8266 Arduino](https://github.com/esp8266/Arduino)
- Install libraries
  - <OneWire.h>
  - <DallasTemperature.h>
  - NewLiquidCrystal <LiquidCrystal_I2C.h> https://bitbucket.org/fmalpartida/new-liquidcrystal/wiki/Home
  - <PID_v1.h>
  - <ArduinoJson.h>

Enjoy your beer! :)