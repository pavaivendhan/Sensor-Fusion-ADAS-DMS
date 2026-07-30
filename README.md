<div align="center">

# Sensor Fusion ADAS & DMS for Driver Safety 🚙🧠

[![ESP32](https://img.shields.io/badge/Firmware-ESP32-black?style=for-the-badge&logo=espressif)](https://www.espressif.com/)
[![Python](https://img.shields.io/badge/Edge_AI-Python-3776AB?style=for-the-badge&logo=python&logoColor=white)](https://python.org/)
[![OpenCV](https://img.shields.io/badge/Vision-OpenCV-5C3EE8?style=for-the-badge&logo=opencv&logoColor=white)](https://opencv.org/)
[![MQTT](https://img.shields.io/badge/Telemetry-MQTT-660066?style=for-the-badge)](https://mqtt.org/)

An enterprise-grade, distributed embedded safety system designed to prevent vehicular accidents using Sensor Fusion, Computer Vision, and Hard Real-Time hardware controls.

</div>

---

## 📌 Abstract & Overview
Traditional driver safety prototypes rely on simple, easily spoofed hardware (like single-point IR sensors). This project mirrors the architecture of commercial **Advanced Driver Assistance Systems (ADAS)** and **Driver Monitoring Systems (DMS)** by distributing the computational load across a **Master-Slave Topology**:

1. **The Edge Brain (Raspberry Pi):** Runs a full Linux OS to execute heavy Computer Vision algorithms (Dlib & OpenCV). It mathematically computes the Eye Aspect Ratio (EAR), Mouth Aspect Ratio (MAR), and Head Tilt angle to detect true physiological drowsiness.
2. **The Edge Controller (ESP32 via FreeRTOS):** Runs strictly compiled C++ for Hard Real-Time operations using a True Real-Time Operating System. It polls the MQ-3 alcohol sensor, parses NEO-M8N GPS data, and controls the L298N Motor Driver.
3. **The Interconnect:** The two boards communicate via USB Serial (UART), simulating an automotive CAN Bus network. Fused data is then published asynchronously via **MQTT** to a fleet management cloud dashboard.

---

## ✨ Key Features & Innovations

- **Computer Vision Drowsiness Detection:** Replaces basic IR sensors with mathematically robust EAR/MAR calculation, making it immune to false positives from natural head movements.
- **Sensor Fusion:** Merges complex visual data with analog (ethanol concentration) and serial (geolocation) inputs in real-time.
- **Automotive Safe Deceleration:** Simulates modern ECU throttle reduction via Hardware PWM, safely decelerating the vehicle rather than executing a dangerous binary engine cutoff.
- **High-Frequency Telemetry:** Utilizes MQTT (Publish-Subscribe) for non-blocking, real-time data streaming over unreliable mobile networks.
- **Multithreaded Processing:** Ensures that heavy video frame processing never blocks serial sensor reading or cloud operations.

---

## 📁 System Architecture

```text
Advanced-Driver-Safety/
│
├── esp32_firmware/           # ESP32 C++ Code (Hard Real-Time ECU)
│   └── main.ino              # Motor control, GPS parser, MQ-3 ADC
│
├── pi_edge/                  # Raspberry Pi Python Code (Edge AI)
│   ├── main_hub.py           # Master script & Serial communicator
│   ├── vision_tracker.py     # OpenCV/Dlib Facial Landmark logic
│   └── mqtt_publisher.py     # Cloud connectivity & JSON packaging
│
└── docs/                     # Diagrams and Schematics
```

---

## 🚀 Setup & Deployment

### Part 1: Compiling the Edge Controller (ESP32)
1. Open the `esp32_firmware` folder in **VS Code with the PlatformIO extension**.
2. PlatformIO will automatically read `platformio.ini` and download the `TinyGPSPlus` library.
3. Click the **Build & Upload** button to compile the C++ firmware and flash it to your ESP32 module.

### Part 2: Starting the Edge Brain (Raspberry Pi)
1. Connect the flashed ESP32 to the Raspberry Pi via a Micro-USB cable.
2. Install the required Python dependencies:
   ```bash
   pip install opencv-python dlib scipy paho-mqtt pyserial numpy
   ```
3. Download the Dlib landmark predictor model into the `models/` directory:
   ```bash
   mkdir -p pi_edge/models
   wget -P pi_edge/models/ http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2
   bzip2 -d pi_edge/models/shape_predictor_68_face_landmarks.dat.bz2
   ```
4. Verify the `SERIAL_PORT` variable in `pi_edge/main_hub.py` (e.g., `/dev/ttyUSB0` or `COM3`).
5. Execute the master hub:
   ```bash
   python pi_edge/main_hub.py
   ```

---

## 🎓 Academic / Engineering Contribution
This project serves as a comprehensive demonstration of integrating behavioral computer vision research with low-level embedded hardware constraints, creating a unified, robust, and scalable Internet of Vehicles (IoV) platform.
