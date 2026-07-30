import serial
import time
import threading
import json
from vision_tracker import start_monitoring, get_vision_data
from mqtt_publisher import publish_telemetry

# Configuration
SERIAL_PORT = "/dev/ttyUSB0"  # Path to ESP32 when plugged via USB
BAUD_RATE = 115200

try:
    esp_serial = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(2) # Wait for ESP32 to reboot
    HARDWARE_CONNECTED = True
    print(f"[INFO] Connected to ESP32 on {SERIAL_PORT}")
except Exception as e:
    HARDWARE_CONNECTED = False
    print(f"[WARNING] Could not connect to ESP32: {e}. Running in Serial Simulation Mode.")

def parse_esp_data(data_str):
    """Parses SENS,alcohol,lat,lng"""
    parts = data_str.split(",")
    if len(parts) == 4 and parts[0] == "SENS":
        return int(parts[1]), float(parts[2]), float(parts[3])
    return None, None, None

def communication_thread():
    print("[INFO] Serial communication thread started.")
    while True:
        # 1. Get Vision State from CV thread
        ear, is_drowsy = get_vision_data()
        
        # 2. Send override command to ESP32
        if HARDWARE_CONNECTED:
            cmd = "CMD,DROWSY\n" if is_drowsy else "CMD,AWAKE\n"
            esp_serial.write(cmd.encode('utf-8'))
            
            # 3. Read incoming telemetry from ESP32
            if esp_serial.in_waiting > 0:
                line = esp_serial.readline().decode('utf-8').strip()
                alcohol, lat, lng = parse_esp_data(line)
                
                if alcohol is not None:
                    # 4. Push combined data to Cloud
                    publish_telemetry(ear, alcohol, lat, lng, is_drowsy)
                    print(f"Telemetry -> EAR: {ear:.2f} | Alcohol: {alcohol} | GPS: {lat},{lng} | Drowsy: {is_drowsy}")
        else:
            # Simulation Mode Logging
            print(f"[SIM] Telemetry -> EAR: {ear:.2f} | Drowsy: {is_drowsy} | Sending to ESP32: {'DROWSY' if is_drowsy else 'AWAKE'}")
            publish_telemetry(ear, 150, 34.0, -118.0, is_drowsy)
            time.sleep(1)

def main():
    print("=========================================================")
    print("  Master Node (Raspberry Pi) - Hybrid Safety System...")
    print("=========================================================\n")

    # Start Serial comms in a background thread
    comm_thread = threading.Thread(target=communication_thread, daemon=True)
    comm_thread.start()

    # Start Computer Vision on main thread
    try:
        start_monitoring()
    except KeyboardInterrupt:
        print("[INFO] Shutting down Master Node...")

if __name__ == "__main__":
    main()
