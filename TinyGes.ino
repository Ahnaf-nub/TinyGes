#include <TinyGes_inferencing.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include <NTPClient.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

#define FIREBASE_HOST "tinyges-default-rtdb.asia-southeast1.firebasedatabase.app"
#define WIFI_SSID "Mahir"         // Change the name of your WIFI
#define WIFI_PASSWORD "Ahnaf767"  // Change the password of your WIFI
#define FIREBASE_AUTH "AIzaSyCnp1jYGlWrOJ6312Pun7i3ExNe6BB1HNA"

FirebaseData firebaseData;
FirebaseJson json;

/* Constant defines -------------------------------------------------------- */
MPU6050 imu;
int16_t ax, ay, az;
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
#define ACC_RANGE 1  // 0: -/+2G; 1: +/-4G
#define CONVERT_G_TO_MS2 (9.81 / (16384 / (1. + ACC_RANGE)))
#define MAX_ACCEPTED_RANGE (2 * 9.81) + (2 * 9.81) * ACC_RANGE
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);  // Use NTP server and time offset
bool checker1 = false;
bool checker2 = false;
String formattedDate;

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false;  // Set this to true to see e.g. features generated from the raw signal

/**
 * @brief      Arduino setup function
 */
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  timeClient.begin();
  timeClient.setTimeOffset(21600);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  // initialize device
  Serial.println("Initializing I2C devices...");
  Wire.begin();
  imu.initialize();

  // Set MPU6050 Offset Calibration
  imu.setXAccelOffset(-2788);
  imu.setYAccelOffset(807);
  imu.setZAccelOffset(1210);
  imu.setXGyroOffset(66);
  imu.setYGyroOffset(-12);
  imu.setZGyroOffset(16);
}

/**
 * @brief      Arduino main loop
 */
void loop() {
  // Put your main code here, to run repeatedly:
  imu.getAcceleration(&ax, &ay, &az);

  signal_t signal;
  int err = get_signal_from_mpu6050(&imu, &signal);
  if (err != 0) {
    ei_printf("Failed to get signal (%d)\n", err);
    return;
  }

  ei_impulse_result_t result = { 0 };

  // Run the classifier
  err = run_classifier(&signal, &result, debug_nn);
  if (err != EI_IMPULSE_OK) {
    ei_printf("ERR: Failed to run classifier (%d)\n", err);
    return;
  }

  // Ensure time is updated
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  int hour = timeClient.getHours();
  String am_pm = (hour >= 12) ? "PM" : "AM";
  formattedDate = timeClient.getFormattedTime() + " " + am_pm + " " + daysOfTheWeek[timeClient.getDay()];

  // Check-in and Check-out logic
  if (result.classification[0].value >= 0.95) {
    if (!checker1) {
      checker1 = true;
      Serial.print("Checked in at: ");
      Serial.print(formattedDate);
      Serial.println(" by " + String(result.classification[0].label));
    } else {
      checker1 = false;
      Serial.print("Checked out at: ");
      Serial.print(formattedDate);
      Serial.println(" by " + String(result.classification[0].label));
    }
  }

  if (result.classification[2].value >= 0.95) {
    if (!checker2) {
      checker2 = true;
      Serial.print("Checked in at: ");
      Serial.print(formattedDate);
      Serial.println(" by " + String(result.classification[2].label));
    } else {
      checker2 = false;
      Serial.print("Checked out at: ");
      Serial.print(formattedDate);
      Serial.println(" by " + String(result.classification[2].label));
    }
  }

  Firebase.setString(firebaseData, "/TinyGes/Status for Ahnaf", checker1 ? "Checked In" : "Checked Out");
  Firebase.setString(firebaseData, "/TinyGes/Status for x", checker2 ? "Checked In" : "Checked Out");
  Firebase.setString(firebaseData, "/TinyGes/Time_and_Date", formattedDate);

  delay(10000);  // Wait for 10 seconds before repeating
}
