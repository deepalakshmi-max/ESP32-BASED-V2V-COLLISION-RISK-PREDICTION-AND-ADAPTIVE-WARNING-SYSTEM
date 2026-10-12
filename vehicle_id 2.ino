#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define VEHICLE_ID 2

// HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 18

// LEDs
#define GREEN_LED 23
#define YELLOW_LED 22
#define RED_LED 21

// Buzzer
#define BUZZER_PIN 19

// ---------------- DATA STRUCTURE ----------------

struct VehicleData {
  int vehicleID;
  float distance;
};

VehicleData receivedData;

// ---------------- STATUS ----------------

enum Status {
  SAFE,
  CAUTION,
  DANGER
};

Status currentStatus = SAFE;
Status detectedStatus = SAFE;

int sameStatusCount = 0;

// ---------------- BUZZER TIMING ----------------

unsigned long previousBuzzerMillis = 0;
bool buzzerState = false;

// ---------------- LED CONTROL ----------------

void setLEDs(bool green, bool yellow, bool red) {
  digitalWrite(GREEN_LED, green);
  digitalWrite(YELLOW_LED, yellow);
  digitalWrite(RED_LED, red);
}

void updateLED(Status status) {

  if (status == SAFE) {
    setLEDs(true, false, false);
  }

  else if (status == CAUTION) {
    setLEDs(false, true, false);
  }

  else {
    setLEDs(false, false, true);
  }
}

// ---------------- NON-BLOCKING BUZZER ----------------

void updateBuzzer(Status status) {

  unsigned long currentMillis = millis();

  // SAFE → Buzzer OFF
  if (status == SAFE) {

    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;
    previousBuzzerMillis = currentMillis;

    return;
  }

  // CAUTION → Slow beep
  if (status == CAUTION) {

    if (currentMillis - previousBuzzerMillis >= 800) {

      previousBuzzerMillis = currentMillis;

      buzzerState = !buzzerState;

      digitalWrite(BUZZER_PIN, buzzerState);
    }

    return;
  }

  // DANGER → Fast beep
  if (status == DANGER) {

    if (currentMillis - previousBuzzerMillis >= 200) {

      previousBuzzerMillis = currentMillis;

      buzzerState = !buzzerState;

      digitalWrite(BUZZER_PIN, buzzerState);
    }

    return;
  }
}

// ---------------- DISTANCE ----------------

float getDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return 999;
  }

  float distance = duration * 0.0343 / 2.0;

  return distance;
}

// ---------------- ESP-NOW RECEIVE ----------------

void OnDataRecv(const esp_now_recv_info_t *info,
                const uint8_t *incomingData,
                int len) {

  if (len == sizeof(VehicleData)) {

    memcpy(&receivedData,
           incomingData,
           sizeof(receivedData));

    // Ignore own data
    if (receivedData.vehicleID != VEHICLE_ID) {

      Serial.print("Received Vehicle ");
      Serial.print(receivedData.vehicleID);

      Serial.print(" Distance: ");
      Serial.print(receivedData.distance);

      Serial.println(" cm");
    }
  }
}

// ---------------- SETUP ----------------

void setup() {

  Serial.begin(115200);

  delay(1000);

  // HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // LEDs
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initial Green
  setLEDs(true, false, false);

  // WiFi Station
  WiFi.mode(WIFI_STA);

  // ESP-NOW channel 1
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  // Start ESP-NOW
  if (esp_now_init() != ESP_OK) {

    Serial.println("ESP-NOW initialization failed!");

    return;
  }

  // Register receive callback
  esp_now_register_recv_cb(OnDataRecv);

  // Broadcast address
  uint8_t broadcastAddress[] = {
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF
  };

  esp_now_peer_info_t peerInfo = {};

  memcpy(peerInfo.peer_addr,
         broadcastAddress,
         6);

  peerInfo.channel = 1;
  peerInfo.encrypt = false;

  // Add peer
  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(&peerInfo);
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("       VEHICLE 2 READY");
  Serial.println("================================");
}

// ---------------- LOOP ----------------

void loop() {

  float distance = getDistance();

  // ---------------- DETECT STATUS ----------------

  Status newStatus;

  if (distance <= 15) {

    newStatus = DANGER;
  }

  else if (distance <= 30) {

    newStatus = CAUTION;
  }

  else {

    newStatus = SAFE;
  }

  // ---------------- STABILITY CHECK ----------------

  if (newStatus == detectedStatus) {

    sameStatusCount++;
  }

  else {

    detectedStatus = newStatus;
    sameStatusCount = 1;
  }

  // Change LED only after 3 same readings
  if (sameStatusCount >= 3 &&
      currentStatus != detectedStatus) {

    currentStatus = detectedStatus;

    updateLED(currentStatus);
  }

  // ---------------- BUZZER ----------------

  updateBuzzer(currentStatus);

  // ---------------- STATUS TEXT ----------------

  String statusText;

  if (currentStatus == SAFE) {

    statusText = "SAFE";
  }

  else if (currentStatus == CAUTION) {

    statusText = "CAUTION";
  }

  else {

    statusText = "DANGER";
  }

  // ---------------- SEND ESP-NOW DATA ----------------

  VehicleData data;

  data.vehicleID = VEHICLE_ID;
  data.distance = distance;

  uint8_t broadcastAddress[] = {
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF
  };

  esp_now_send(
    broadcastAddress,
    (uint8_t *)&data,
    sizeof(data)
  );

  // ---------------- SERIAL MONITOR ----------------

  Serial.print("Vehicle 2 | Distance: ");
  Serial.print(distance);

  Serial.print(" cm | Status: ");
  Serial.println(statusText);

  delay(300);
}
