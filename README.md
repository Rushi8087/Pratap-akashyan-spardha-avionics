# Pratap Akashyan Spardha Avionics System 🚀

Avionics flight computer and telemetry system for **Akashyan Spardha 2026 — Solid Fuel Rocketry Challenge**.

- **Team**: Pratap
- **Team Code**: `AYS26-D5FTA`

---

## 📌 Features

- **Barometric Altimeter**: BMP280 pressure and altitude measurement with automatic ground baseline calibration and noise/step-rejection filtering.
- **Apogee & Drop Detection**: Multi-reading confirmation logic to reliably detect apogee and trigger parachute deployment.
- **Ejection Mechanism**: Servo-controlled release system powered separately via GPIO27.
- **LoRa Telemetry**: Long-range telemetry broadcast at 434.5 MHz per competition specification:
  - Altitude packets (Type `0x01`) sent at 2 Hz (every 500 ms).
  - Apogee confirmation packets (Type `0x02`).

---

## 🛠️ Hardware & Pin Configuration

| Component | Function | ESP32 GPIO Pin |
| :--- | :--- | :--- |
| **BMP280** | I2C SDA | GPIO 21 |
| **BMP280** | I2C SCL | GPIO 22 |
| **LoRa (RA-02)** | SPI MOSI | GPIO 23 |
| **LoRa (RA-02)** | SPI MISO | GPIO 19 |
| **LoRa (RA-02)** | SPI SCK | GPIO 18 |
| **LoRa (RA-02)** | NSS / CS | GPIO 5 |
| **LoRa (RA-02)** | RESET | GPIO 14 |
| **LoRa (RA-02)** | DIO0 | GPIO 26 |
| **Ejection Servo** | Signal (Separate Battery) | GPIO 27 |

---

## 📦 Required Libraries

Install via the Arduino IDE Library Manager:
- `Adafruit BMP280 Library` (and `Adafruit Unified Sensor`)
- `ESP32Servo`
- `LoRa` by Sandeep Mistry

---

## ⚙️ Configuration & Flashing

1. Open `avionics_system.ino` in Arduino IDE or PlatformIO.
2. Select your ESP32 board target.
3. Update `ROCKET_ID` if a specific numeric ID has been issued during technical inspection.
4. Verify pin configurations and compile/flash to the flight computer.
