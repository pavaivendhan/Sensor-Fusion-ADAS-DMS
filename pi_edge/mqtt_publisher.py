import json
import time

try:
    import paho.mqtt.client as mqtt
    MQTT_AVAILABLE = True
except ImportError:
    MQTT_AVAILABLE = False
    print("[WARNING] paho-mqtt missing. Running cloud module in simulation mode.")

# Broker config (Using a public broker for demonstration)
BROKER_ADDRESS = "broker.hivemq.com"
TOPIC = "fleet/Hybrid-V1/telemetry"

if MQTT_AVAILABLE:
    client = mqtt.Client(client_id="Hybrid_Vehicle_01", callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    try:
        client.connect(BROKER_ADDRESS)
        client.loop_start()
        print(f"[INFO] Connected to MQTT Broker: {BROKER_ADDRESS}")
    except Exception as e:
        print(f"[ERROR] Could not connect to MQTT: {e}")

def publish_telemetry(ear, alcohol, lat, lng, is_drowsy):
    """
    Publishes the fused telemetry payload to the MQTT broker.
    """
    status = "CRITICAL" if (is_drowsy or alcohol > 400) else "SAFE"
    
    payload = {
        "vehicle_id": "Hybrid-V1",
        "timestamp": time.time(),
        "telemetry": {
            "ear_score": round(ear, 3) if ear else 0.0,
            "alcohol_ppm": alcohol,
            "gps": {
                "lat": lat,
                "lng": lng
            }
        },
        "status": status
    }
    
    payload_json = json.dumps(payload)
    
    if MQTT_AVAILABLE:
        client.publish(TOPIC, payload_json)
