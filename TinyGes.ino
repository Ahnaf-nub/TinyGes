#include <TinyGes_inferencing.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include <NTPClient.h>
#include <WiFi.h>
/* Constant defines -------------------------------------------------------- */
MPU6050 imu;
int16_t ax, ay, az;
const char* ssid = "Mahir";
const char* password = "Ahnaf767";
#define ACC_RANGE 1  // 0: -/+2G; 1: +/-4G
#define CONVERT_G_TO_MS2 (9.81 / (16384 / (1. + ACC_RANGE)))
#define MAX_ACCEPTED_RANGE (2 * 9.81) + (2 * 9.81) * ACC_RANGE
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
bool checker = false;
bool checker2 = false;
String formattedDate;
/*
 ** NOTE: If you run into TFLite arena allocation issue.
 **
 ** This may be due to may dynamic memory fragmentation.
 ** Try defining "-DEI_CLASSIFIER_ALLOCATION_STATIC" in boards.local.txt (create
 ** if it doesn't exist) and copy this file to
 ** `<ARDUINO_CORE_INSTALL_PATH>/arduino/hardware/<mbed_core>/<core_version>/`.
 **
 ** See
 ** (https://support.arduino.cc/hc/en-us/articles/360012076960-Where-are-the-installed-cores-located-)
 ** to find where Arduino installs cores on your machine.
 **
 ** If the problem persists then there's not enough memory for this model and application.
 */

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false;  // Set this to true to see e.g. features generated from the raw signal

/**
* @brief      Arduino setup function
*/
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  timeClient.begin();
  timeClient.setTimeOffset(21600);
  // comment out the below line to cancel the wait for USB connection (needed for native USB)
  while (!Serial);
  Serial.println("Edge Impulse Inferencing Demo");

  // initialize device
  Serial.println("Initializing I2C devices...");
  Wire.begin();
  imu.initialize();
  delay(10);

  //Set MCU 6050 OffSet Calibration
  imu.setXAccelOffset(-2788);
  imu.setYAccelOffset(-1952);
  imu.setZAccelOffset(2172);
  imu.setXGyroOffset(144);
  imu.setYGyroOffset(-22);
  imu.setZGyroOffset(12);

  imu.setFullScaleAccelRange(ACC_RANGE);

  if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3) {
    ei_printf("ERR: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be equal to 3 (the 3 sensor axes)\n");
    return;
  }
}

/**
 * @brief Return the sign of the number
 * 
 * @param number 
 * @return int 1 if positive (or 0) -1 if negative
 */
float ei_get_sign(float number) {
  return (number >= 0.0) ? 1.0 : -1.0;
}

/**
* @brief      Get data and run inferencing
*
* @param[in]  debug  Get debug info if true
*/
void loop() {
  
  ei_printf("Sampling...\n");

  // Allocate a buffer here for the values we'll read from the IMU
  float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3) {
    // Determine the next tick (and then sleep later)
    uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);

    imu.getAcceleration(&ax, &ay, &az);
    buffer[ix + 0] = ax;
    buffer[ix + 1] = ay;
    buffer[ix + 2] = az;

    //ei_printf("raw values:    %.2f, %.2f, %.2f\n", ax*CONVERT_G_TO_MS2, ay*CONVERT_G_TO_MS2, az*CONVERT_G_TO_MS2);

    buffer[ix + 0] *= CONVERT_G_TO_MS2;
    buffer[ix + 1] *= CONVERT_G_TO_MS2;
    buffer[ix + 2] *= CONVERT_G_TO_MS2;

    for (int i = 0; i < 3; i++) {
      if (fabs(buffer[ix + i]) > MAX_ACCEPTED_RANGE) {
        buffer[ix + i] = ei_get_sign(buffer[ix + i]) * MAX_ACCEPTED_RANGE;
      }
    }

    delayMicroseconds(next_tick - micros());
  }

  // Turn the raw buffer in a signal which we can the classify
  signal_t signal;
  int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != 0) {
    ei_printf("Failed to create signal from buffer (%d)\n", err);
    return;
  }

  // Run the classifier
  ei_impulse_result_t result = { 0 };

  err = run_classifier(&signal, &result, debug_nn);
  if (err != EI_IMPULSE_OK) {
    ei_printf("ERR: Failed to run classifier (%d)\n", err);
    return;
  }

  // print the predictions
  ei_printf("Predictions ");
  ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
  ei_printf(": \n");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
  }
  if (result.classification[0].value >= 0.7){
    checker = true;
    ei_printf("Checked in at: " + timeClient.getFormattedTime() + daysOfTheWeek[timeClient.getDay()] + "by Ahnaf")
  } 
  if (result.classification[1].value >= 0.7){
    checker2 = true;
    ei_printf("Checked in at: " + timeClient.getFormattedTime() + daysOfTheWeek[timeClient.getDay()] + "by Shaila Sharmin")
  }
}