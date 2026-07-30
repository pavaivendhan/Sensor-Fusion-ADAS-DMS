#include <Arduino.h>
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

// GPS Configuration (NEO-M8N uses same NMEA protocol)
HardwareSerial GPS_Serial(1); // Using UART1 for GPS
TinyGPSPlus gps;

// ---------------------------------------------------------
// FreeRTOS SHARED STATE & MUTEXES
// ---------------------------------------------------------
SemaphoreHandle_t stateMutex;
int currentSpeed = 255;
int sharedAlcoholLevel = 0;
bool piTriggeredDrowsy = false;
float sharedLat = 0.0;
float sharedLng = 0.0;

// ---------------------------------------------------------
// FREERTOS TASKS
// ---------------------------------------------------------

// Task 1: Sensor & Motor Control (Core 1)
void TaskSensorsMotor(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    // 1. Read Analog Sensor
    int localAlcohol = analogRead(MQ3_PIN);
    
    // Lock Mutex to safely update shared state and read drowsy flag
    bool localDrowsy = false;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
      sharedAlcoholLevel = localAlcohol;
      localDrowsy = piTriggeredDrowsy;
      xSemaphoreGive(stateMutex);
    }

    // 2. Actuation Logic (Hardware Overrides)
    if (localAlcohol > ALCOHOL_THRESHOLD || localDrowsy) {
      if (currentSpeed > 0) {
        currentSpeed -= 5; 
        if (currentSpeed < 0) currentSpeed = 0;
        ledcWrite(PWM_CHANNEL, currentSpeed);
        
        // Buzz to alert
        digitalWrite(BUZZER_PIN, HIGH);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        digitalWrite(BUZZER_PIN, LOW);
      }
    }
    
    // Run this critical loop every 50ms
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// Task 2: Master-Slave UART Communication (Core 0)
void TaskComm(void *pvParameters) {
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1000 / portTICK_PERIOD_MS; // 1Hz telemetry

  for (;;) {
    // 1. Listen to Raspberry Pi
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      
      if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
        if (command == "CMD,DROWSY") {
          piTriggeredDrowsy = true;
        } else if (command == "CMD,AWAKE") {
          piTriggeredDrowsy = false;
        }
        xSemaphoreGive(stateMutex);
      }
    }

    // 2. Transmit Telemetry (Once per second)
    int localAlcohol = 0;
    float localLat = 0.0;
    float localLng = 0.0;
    
    if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
      localAlcohol = sharedAlcoholLevel;
      localLat = sharedLat;
      localLng = sharedLng;
      xSemaphoreGive(stateMutex);
    }

    Serial.print("SENS,");
    Serial.print(localAlcohol);
    Serial.print(",");
    Serial.print(localLat, 6);
    Serial.print(",");
    Serial.println(localLng, 6);

    // Wait for the next 1000ms cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// Task 3: NEO-M8N GPS Parsing (Core 1)
void TaskGPS(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    while (GPS_Serial.available() > 0) {
      gps.encode(GPS_Serial.read());
    }

    if (gps.location.isValid() && gps.location.isUpdated()) {
      if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
        sharedLat = gps.location.lat();
        sharedLng = gps.location.lng();
        xSemaphoreGive(stateMutex);
      }
    }
    // Yield to allow other tasks to run
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ---------------------------------------------------------
// SETUP
// ---------------------------------------------------------
void setup() {
  Serial.begin(115200);
  GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_PWM_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, currentSpeed);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize Mutex
  stateMutex = xSemaphoreCreateMutex();
  if (stateMutex == NULL) {
    Serial.println("Mutex creation failed!");
    while(1);
  }

  // Create FreeRTOS Tasks
  // Syntax: TaskFunction, Name, StackSize, Params, Priority, Handle, Core
  
  xTaskCreatePinnedToCore(
    TaskSensorsMotor, 
    "TaskSensors", 
    2048, 
    NULL, 
    3, // Highest priority
    NULL, 
    1  // Pin to Core 1 (App Core)
  );

  xTaskCreatePinnedToCore(
    TaskComm, 
    "TaskComm", 
    4096, 
    NULL, 
    2, // Medium priority
    NULL, 
    0  // Pin to Core 0 (Pro/Network Core)
  );

  xTaskCreatePinnedToCore(
    TaskGPS, 
    "TaskGPS", 
    2048, 
    NULL, 
    1, // Lowest priority
    NULL, 
    1  // Pin to Core 1
  );

  Serial.println("ESP32_FREERTOS_READY");
}

void loop() {
  // Empty! FreeRTOS scheduler takes over automatically.
  vTaskDelete(NULL);
}
