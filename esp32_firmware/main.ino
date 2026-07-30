#include <HardwareSerial.h>
#include <TinyGPS++.h>

// ---------------------------------------------------------
// PINS & CONSTANTS
// ---------------------------------------------------------
const int MQ3_PIN = 34;        // Analog ADC1 pin for alcohol sensor
const int MOTOR_PWM_PIN = 18;  // PWM pin for L298N ENA
const int MOTOR_IN1 = 19;      // L298N IN1
const int MOTOR_IN2 = 21;      // L298N IN2
const int BUZZER_PIN = 23;     // Active buzzer

const int ALCOHOL_THRESHOLD = 400;

// PWM Configuration for ESP32
const int PWM_FREQ = 5000;
const int PWM_CHANNEL = 0;
const int PWM_RESOLUTION = 8; // 0-255

// GPS Configuration
HardwareSerial GPS_Serial(1); // Using UART1 for GPS
TinyGPSPlus gps;

// State Variables
int currentSpeed = 255;
bool piTriggeredDrowsy = false;

// ---------------------------------------------------------
// SETUP
// ---------------------------------------------------------
void setup() {
  // Main USB Serial for communicating with Raspberry Pi
  Serial.begin(115200);

  // GPS Serial (RX=16, TX=17)
  GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);

  // Pin Modes
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);

  // Setup ESP32 Hardware PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_PWM_PIN, PWM_CHANNEL);
  
  // Start engine at full speed
  ledcWrite(PWM_CHANNEL, currentSpeed);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("ESP32_READY");
}

// ---------------------------------------------------------
// DECELERATION LOGIC
// ---------------------------------------------------------
void decelerateMotor() {
  if (currentSpeed > 0) {
    currentSpeed -= 5; 
    if (currentSpeed < 0) currentSpeed = 0;
    ledcWrite(PWM_CHANNEL, currentSpeed);
    
    // Buzz to alert
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ---------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------
void loop() {
  // 1. Parse GPS Data (Non-blocking)
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  // 2. Read Alcohol Level
  int alcoholLevel = analogRead(MQ3_PIN);
  
  // 3. Listen to commands from Raspberry Pi (Master)
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "CMD,DROWSY") {
      piTriggeredDrowsy = true;
    } else if (command == "CMD,AWAKE") {
      piTriggeredDrowsy = false;
    }
  }

  // 4. Actuation Logic (Safety Overrides)
  if (alcoholLevel > ALCOHOL_THRESHOLD || piTriggeredDrowsy) {
    decelerateMotor();
  } else {
    // Optional: slowly speed back up if safe, or require manual reset
    // currentSpeed = 255; 
    // ledcWrite(PWM_CHANNEL, currentSpeed);
  }

  // 5. Transmit Telemetry back to Raspberry Pi (Every 1 second)
  static unsigned long lastTransmit = 0;
  if (millis() - lastTransmit > 1000) {
    lastTransmit = millis();
    
    float lat = 0.0;
    float lng = 0.0;
    if (gps.location.isValid()) {
      lat = gps.location.lat();
      lng = gps.location.lng();
    }
    
    // Send string format: SENS,alcohol,lat,lng
    Serial.print("SENS,");
    Serial.print(alcoholLevel);
    Serial.print(",");
    Serial.print(lat, 6);
    Serial.print(",");
    Serial.println(lng, 6);
  }
}
