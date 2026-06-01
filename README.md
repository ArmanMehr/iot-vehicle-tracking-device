# IoT Vehicle Tracker

An SMS-based vehicle tracking system built on Arduino UNO. The device receives an SMS with the keyword **"Location"**, acquires GPS coordinates via the NEO-6M module, and replies with latitude, longitude, current speed, and a Google Maps link — all over the cellular network using a SIM800L GSM module. The system runs on two rechargeable 18500 Li-Ion batteries, making it fully portable and self-contained.

---

## Table of Contents

- [How It Works](#how-it-works)
- [Hardware Components](#hardware-components)
- [Circuit Design](#circuit-design)
- [Pin Connections](#pin-connections)
- [Software](#software)
- [Installation and Upload](#installation-and-upload)
- [Usage](#usage)
- [LED Status Indicators](#led-status-indicators)
- [Project Structure](#project-structure)
- [Dependencies](#dependencies)
- [Known Limitations](#known-limitations)
- [License](#license)

---

## How It Works

<p align="center">
  <img src=assets/high-level-block-diagram.png>
<p align="center">

1. On power-up, the Arduino initializes the SIM800L and waits for a cellular network connection.
2. It then validates the NEO-6M GPS fix before entering the idle loop.
3. The device continuously polls the SIM800L for incoming SMS messages.
4. When an SMS containing the word `Location` (case-insensitive) is received, the sender's number is extracted automatically from the message header.
5. The Arduino switches to the GPS serial port and waits for a valid location and speed fix.
6. It builds a Google Maps URL from the coordinates, switches back to the GSM port, and sends the reply SMS.
7. The device clears the SMS inbox and returns to the idle polling loop.

---

## Hardware Components

| Component              | Model / Spec                      | Notes                                             |
| ---------------------- | --------------------------------- | ------------------------------------------------- |
| Microcontroller        | Arduino UNO (ATmega328P)          | Main processing unit                              |
| GPS Module             | u-blox NEO-6M                     | UART, 3.7–5 V supply                              |
| GSM Module             | SIM800L                           | UART, 3.4–4.4 V, up to 2 A peak current           |
| Batteries              | 2x Li-Ion 18500, 3.7 V / 1200 mAh | Series connection → 7.4 V                         |
| Battery Charger Module | TP4056 / 18650 Charger            | Micro-USB charging, B+/B- and OUT+/OUT- terminals |
| DC-DC Buck Converter   | LM2596 Step-Down, 3 A             | Steps 7.4 V down to ~4.1 V for SIM800L            |
| Capacitor              | 1000 µF / 16 V electrolytic       | Stabilizes SIM800L supply during current peaks    |
| Resistors              | 10 kΩ and 20 kΩ (1/4 W)           | Voltage divider: Arduino TX → SIM800L RX          |
| Resistors              | 2x 220 Ω (1/4 W)                  | Current limiting for status LEDs                  |
| LEDs                   | 1x Green, 1x Red                  | GPS and GSM status indicators                     |
| Breadboard             | Full-size                         | Prototyping platform                              |
| Jumper Wires           | Male-to-male                      | Interconnects                                     |

---

## Circuit Design
<p align="center">
  <img src=assets/circuit-diagram.png>
<p align="center">

### Power Architecture

The two 18500 cells are connected in series to produce 7.4 V. This feeds:

- The Arduino UNO via the `VIN` pin (accepts 7–12 V).
- The LM2596 buck converter, adjusted to output ~4.1 V for the SIM800L.

The batteries connect first to the TP4056 charger module (B+/B- terminals), which provides protection and allows recharging via Micro-USB. The OUT+/OUT- terminals of the charger supply the rest of the circuit.

The 1000 µF capacitor is placed in parallel across the LM2596 output and the SIM800L VCC/GND pins to absorb the 2 A instantaneous current spikes the module draws during transmission bursts.

The NEO-6M is powered directly from the Arduino's regulated 5 V output pin, since it draws well under 50 mA.

### Logic-Level Voltage Divider

The Arduino UNO outputs 5 V logic on its digital pins, but the SIM800L RX line expects a maximum of 3.3 V. A resistive voltage divider is placed between Arduino digital pin 11 (TX) and SIM800L RXD:

<p align="center">
  <img src=assets/level-shifter.png>
<p align="center">

This divides the 5 V output to approximately 3.33 V at the SIM800L input:

```
V_RX = (20 / (10 + 20)) × 5 V ≈ 3.33 V
```

The SIM800L TXD line outputs 3.3 V logic, which the Arduino accepts directly with no level shifting needed.

### Status LEDs

Each LED is connected in series with a 220 Ω current-limiting resistor between an Arduino digital output pin and GND.

---

## Pin Connections

### NEO-6M GPS Module → Arduino UNO

|NEO-6M Pin|Arduino Pin|Note|
|---|---|---|
|VCC|5V|Arduino regulated 5 V output|
|GND|GND|Common ground|
|TX|D9 (Software RX)|GPS data to Arduino|
|RX|D8 (Software TX)|Commands from Arduino to GPS|

### SIM800L GSM Module → Arduino UNO

|SIM800L Pin|Arduino Pin / Source|Note|
|---|---|---|
|VCC|LM2596 OUT+|~4.1 V from buck converter|
|GND|GND|Common ground (shared with Arduino)|
|TXD|D10 (Software RX)|GSM data to Arduino, direct connection|
|RXD|D11 via voltage divider|5 V stepped down to ~3.33 V|

### Status LEDs → Arduino UNO

|LED|Arduino Pin|Series Resistor|
|---|---|---|
|Green (GPS status)|D2|220 Ω|
|Red (GSM status)|D3|220 Ω|

---

## Software

The firmware is written in Arduino C++ and is contained entirely in `project.ino`.

### Libraries Used

- **TinyGPS++** — Parses NMEA sentences from the NEO-6M and exposes `latitude()`, `longitude()`, `speed.kmph()`, and validity checks.
- **SoftwareSerial** — Creates two software UART ports on digital pins, since the UNO has only one hardware UART (which is occupied by the USB/PC connection).

### Firmware Logic Overview

```
setup()
  ├── Initialize GPIO (LED pins as OUTPUT)
  ├── Start all three serial ports at 9600 baud
  ├── GSM initialization
  │   ├── AT                    — verify module responds
  │   ├── ATZ                   — reset to factory defaults
  │   ├── AT+CREG?              — wait for network registration
  │   ├── AT+CSMP=17,167,0,0    — SMS bearer settings
  │   ├── AT+CMGF=1             — set TEXT mode
  │   ├── AT+CNMI=2,2,0,0,0     — route incoming SMS to serial
  │   └── AT+CMGD=4             — clear SMS inbox
  └── GPS initialization
      └── Block until gps.location.isValid()

loop()
  ├── Poll SIM800L for incoming SMS via ReadGSM()
  │   └── Extract sender number from +98 prefix if present
  ├── If SMS body contains "Location" or "location":
  │   ├── Switch serial listener to GPS module
  │   ├── Block until valid GPS fix (lat/lng + speed)
  │   ├── Build Google Maps URL string
  │   ├── Switch serial listener to GSM module
  │   ├── Send SMS reply: longitude, latitude, speed, link
  │   └── Clear SMS inbox (AT+CMGD=4)
  └── Repeat
```

### AT Commands Reference

|Command|Purpose|
|---|---|
|`AT`|Verify module communication|
|`ATZ`|Reset module to default configuration|
|`AT+CREG?`|Check GSM network registration status|
|`AT+CSQ`|Query received signal quality (RSSI)|
|`AT+CMGF=1`|Set SMS to text mode|
|`AT+CNMI=2,2,0,0,0`|Route new SMS directly to serial output|
|`AT+CSMP=17,167,0,0`|Configure SMS parameters|
|`AT+CMGS="<number>"`|Send SMS to a phone number|
|`AT+CMGD=4`|Delete all stored messages|

---

## Installation and Upload

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) 1.8+ or Arduino IDE 2.x
- **TinyGPS++** library — install via Arduino IDE Library Manager (`Sketch > Include Library > Manage Libraries`, search for `TinyGPS++`)
- **SoftwareSerial** — included with the Arduino IDE by default

### Steps

1. Clone the repository:
    
    ```bash
    git clone https://github.com/ArmanMehr/iot-vehicle-tracking-device.git
    cd iot-vehicle-tracking-device
    ```
    
2. Open `project.ino` in the Arduino IDE.
    
3. The device automatically replies to the number that sends the trigger SMS — the sender's number is extracted from the incoming message header, so no phone number needs to be hardcoded.
    
4. Connect the Arduino UNO via USB.
    
5. Select `Tools > Board > Arduino UNO` and the correct COM port.
    
6. Click **Upload**.
    
7. Open the Serial Monitor at **9600 baud** to observe initialization output.
    

---

## Usage

1. Power on the device. Both LEDs illuminate for 5 seconds during startup.
2. The red LED stays on while the GSM module connects to the cellular network.
3. Once ready, the Serial Monitor shows `Waiting for SMS...` and the red LED blinks.
4. From any phone, send an SMS containing the word `Location` to the SIM card number inserted in the SIM800L module.
5. The green LED lights up while a GPS fix is being acquired.
6. Within seconds you will receive an SMS reply in this format:

```
Longitude: 51.388974
Latitude: 35.689197
Speed: 42.30 km/h

Link: https://maps.google.com/?q=35.689197,51.388974
```

7. Tap the Google Maps link to see the vehicle's position on the map.

---

## LED Status Indicators

|Green LED (D2)|Red LED (D3)|System State|
|---|---|---|
|ON|ON|Startup / initialization (5 s)|
|OFF|ON|GSM initializing or sending SMS reply|
|ON|OFF|Acquiring GPS fix|
|OFF|Blinking|Idle — waiting for incoming SMS|

---

## Dependencies

|Library|Version|Source|
|---|---|---|
|TinyGPS++|>= 1.0.3|Arduino Library Manager / [GitHub](https://github.com/mikalhart/TinyGPSPlus)|
|SoftwareSerial|Built-in|Arduino IDE|

---

## Known Limitations

- The Arduino UNO has a single hardware UART, so both the GPS and GSM modules use software serial. Only one software serial port can receive data at a time; the firmware switches listeners explicitly between modules.
- Response latency depends on GPS satellite signal quality and GSM network conditions. In areas with poor sky visibility, the GPS fix may take longer.
- The SIM800L operates on 2G bands (GSM 850/900/1800/1900 MHz). It will not work in regions where 2G networks have been fully decommissioned.
- The `AT+CSMP=17,167,0,0` setting is configured for the Irancell operator. If you are using a different carrier, you may need to adjust or remove this command.

---

## License

This project is released under the [MIT License](https://opensource.org/license/mit).
