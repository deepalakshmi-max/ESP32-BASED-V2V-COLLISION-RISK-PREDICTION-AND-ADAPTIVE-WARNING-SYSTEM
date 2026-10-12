#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define VEHICLE_ID 1

// HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 18

// LEDs
#define GREEN_LED 23
#define YELLOW_LED 22
#define RED_LED 21

// Data structure
struct VehicleData {
  int vehicleID;
  float distance;
};

VehicleData receivedData;

float previousDistance = 0;
unsigned long previousTime = 0;


// ---------------- LED CONTROL ----------------

void setLEDs(bool green, bool yellow, bool red) {
  digitalWrite(GREEN_LED, green);
  digitalWrite(YELLOW_LED, yellow);
  digitalWrite(RED_LED, red);
}


// ---------------- DISTANCE MEASUREMENT ----------------

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

    Serial.print("Vehicle ");
    Serial.print(receivedData.vehicleID);

    Serial.print(" Distance: ");

    Serial.print(receivedData.distance);

    Serial.println(" cm");
  }
}


// ---------------- SETUP ----------------

void setup() {

  Serial.begin(115200);

  delay(1000);

  // Sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // LED pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Initially OFF
  setLEDs(false, false, false);


  // ---------------- WiFi ----------------

  WiFi.mode(WIFI_STA);

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);


  // ---------------- ESP-NOW ----------------

  if (esp_now_init() != ESP_OK) {

    Serial.println("ESP-NOW initialization failed!");

    return;
  }


  esp_now_register_recv_cb(OnDataRecv);


  // Broadcast address

  esp_now_peer_info_t peerInfo = {};

  uint8_t broadcastAddress[] = {
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF
  };


  memcpy(peerInfo.peer_addr,
         broadcastAddress,
         6);

  peerInfo.channel = 1;

  peerInfo.encrypt = false;


  if (!esp_now_is_peer_exist(broadcastAddress)) {

    esp_now_add_peer(&peerInfo);
  }


  Serial.println("Vehicle 1 Ready");
}


// ---------------- MAIN LOOP ----------------

void loop() {

  // Measure distance

  float distance = getDistance();


  // Calculate time

  unsigned long currentTime = millis();


  // Calculate closing speed

  float closingSpeed = 0;


  if (previousTime > 0) {

    float timeSeconds =
      (currentTime - previousTime) / 1000.0;


    if (timeSeconds > 0) {

      closingSpeed =
        (previousDistance - distance)
        / timeSeconds;
    }
  }


  previousDistance = distance;

  previousTime = currentTime;


  // ---------------- STATUS ----------------

  String status;


  // DANGER

  if (distance <= 30 || closingSpeed >= 50) {

    status = "DANGER";

    setLEDs(false, false, true);
  }


  // CAUTION

  else if (distance <= 60 || closingSpeed >= 20) {

    status = "CAUTION";

    setLEDs(false, true, false);
  }


  // SAFE

  else {

    status = "SAFE";

    setLEDs(true, false, false);
  }


  // ---------------- SEND DATA ----------------

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

  Serial.print("Vehicle 1 | Distance: ");

  Serial.print(distance);

  Serial.print(" cm | Closing Speed: ");

  Serial.print(closingSpeed);

  Serial.print(" cm/s | Status: ");

  Serial.println(status);


  delay(200);
}
