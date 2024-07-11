#include <TinyGes_inferencing.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include <NTPClient.h>
#include <WiFi.h>
#include <WebServer.h>

MPU6050 imu;
int16_t ax, ay, az;

const char* ssid = "Mahir";
const char* password = "Ahnaf767";

#define ACC_RANGE 1  // 0: -/+2G; 1: +/-4G
#define CONVERT_G_TO_MS2 (9.81 / (16384 / (1. + ACC_RANGE)))
#define MAX_ACCEPTED_RANGE (2 * 9.81) + (2 * 9.81) * ACC_RANGE
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

bool checker1 = false;
bool checker2 = false;
String formattedDate;
String checkInTime1 = "";
String checkOutTime1 = "";
String checkInTime2 = "";
String checkOutTime2 = "";
String user1 = "";
String user2 = "";
WebServer server(80);

String htmlPage = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Check-in/Check-out Status</title>
  <style>
    body { font-family: Arial, sans-serif; background-color: #581818; }
    .container { max-width: 600px; margin: 50px auto; padding: 20px; background: white; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }
    h1 { text-align: center; color: #333; }
    p { font-size: 1.2em; }
    .status { margin-top: 20px; padding: 10px; border: 1px solid #ccc; border-radius: 5px; }
    .checked-in { background: #d4edda; color: #155724; }
    .checked-out { background: #f8d7da; color: #721c24; }
  </style>
</head>
<body>
  <div class="container">
    <h1>TinyGes</h1>
    <div class="status">
      <p id="statusText">Loading...</p>
    </div>
  </div>
  <script>
    setInterval(() => {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          const statusText = document.getElementById('statusText');
          let statusHtml = '';
          if (data.checkInTime1) {
            statusHtml += `<p class="checked-in">${data.user1} checked in at: ${data.checkInTime1}</p>`;
          }
          if (data.checkOutTime1) {
            statusHtml += `<p class="checked-out">${data.user1} checked out at: ${data.checkOutTime1}</p>`;
          }
          if (data.checkInTime2) {
            statusHtml += `<p class="checked-in">${data.user2} checked in at: ${data.checkInTime2}</p>`;
          }
          if (data.checkOutTime2) {
            statusHtml += `<p class="checked-out">${data.user2} checked out at: ${data.checkOutTime2}</p>`;
          }
          statusText.innerHTML = statusHtml;
        });
    }, 1000);
  </script>
</body>
</html>
)=====";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleStatus() {
  String status = "{\"checkInTime1\": \"" + checkInTime1 + "\", \"checkOutTime1\": \"" + checkOutTime1 + "\", \"checkInTime2\": \"" + checkInTime2 + "\", \"checkOutTime2\": \"" + checkOutTime2 + "\", \"user1\": \"" + user1 + "\", \"user2\": \"" + user2 + "\"}";
  server.send(200, "application/json", status);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
    ;
  Serial.println("Connected to WiFi");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  timeClient.begin();
  timeClient.setTimeOffset(21600);
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.begin();

  Wire.begin();
  imu.initialize();

  imu.setXAccelOffset(-2788);
  imu.setYAccelOffset(807);
  imu.setZAccelOffset(1210);
  imu.setXGyroOffset(66);
  imu.setYGyroOffset(-12);
  imu.setZGyroOffset(16);
}

float ei_get_sign(float number) {
  return (number >= 0.0) ? 1.0 : -1.0;
}

void loop() {
  server.handleClient();

  float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3) {
    uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);

    imu.getAcceleration(&ax, &ay, &az);
    buffer[ix + 0] = ax;
    buffer[ix + 1] = ay;
    buffer[ix + 2] = az;

    buffer[ix + 2] *= CONVERT_G_TO_MS2;

    for (int i = 0; i < 3; i++) {
      if (fabs(buffer[ix + i]) > MAX_ACCEPTED_RANGE) {
        buffer[ix + i] = ei_get_sign(buffer[ix + i]) * MAX_ACCEPTED_RANGE;
      }
    }
    delayMicroseconds(next_tick - micros());
  }

  signal_t signal;
  int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != 0) {
    ei_printf("Failed to create signal from buffer (%d)\n", err);
    return;
  }

  ei_impulse_result_t result = { 0 };

  err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) {
    ei_printf("ERR: Failed to run classifier (%d)\n", err);
    return;
  }

  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  int hour = timeClient.getHours();
  String am_pm = (hour >= 12) ? "PM" : "AM";
  formattedDate = timeClient.getFormattedTime() + " " + am_pm + " " + daysOfTheWeek[timeClient.getDay()];

  if (result.classification[0].value >= 0.95) {
    if (!checker1) {
      checker1 = true;
      checkInTime1 = formattedDate;
      user1 = String(result.classification[0].label);
      //Serial.print(user1 + " checked in at: ");
      //Serial.println(formattedDate);
    } else {
      checker1 = false;
      checkOutTime1 = formattedDate;
      user1 = String(result.classification[0].label);
      //Serial.print(user1 + " checked out at: ");
      //Serial.println(formattedDate);
    }
  }

  if (result.classification[2].value >= 0.90) {
    if (!checker2) {
      checker2 = true;
      checkInTime2 = formattedDate;
      user2 = String(result.classification[2].label);
      //Serial.print(user2 + " checked in at: ");
      //Serial.println(formattedDate);
    } else {
      checker2 = false;
      checkOutTime2 = formattedDate;
      user2 = String(result.classification[2].label);
      //Serial.print(user2 + " checked out at: ");
      //Serial.println(formattedDate);
    }
  }
}
