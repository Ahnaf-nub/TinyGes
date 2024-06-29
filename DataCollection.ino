#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

#define FREQUENCY_HZ        50
#define INTERVAL_MS         (1000 / (FREQUENCY_HZ + 1))
#define ACC_RANGE           1 // 0: -/+2G; 1: +/-4G

// convert factor g to m/s2 ==> [-32768, +32767] ==> [-2g, +2g]
#define CONVERT_G_TO_MS2    (9.81/(16384.0/(1.+ACC_RANGE))) 

static unsigned long last_interval_ms = 0;

MPU6050 imu;
int16_t ax, ay, az;

void setup() {
  
    Serial.begin(115200);

    
    // initialize device
    Serial.println("Initializing I2C devices...");
    Wire.begin();
    imu.initialize();
    delay(10);
    
//    // verify connection
//    if (imu.testConnection()) {
//      Serial.println("IMU connected");
//    }
//    else {
//      Serial.println("IMU Error");
//    }
    delay(300);
    
    //Set MCU 6050 OffSet Calibration 
    imu.setXAccelOffset(-2788);
    imu.setYAccelOffset(-1952);
    imu.setZAccelOffset(2172);
    imu.setXGyroOffset(144);
    imu.setYGyroOffset(-22);
    imu.setZGyroOffset(12);
    
    /* Set full-scale accelerometer range.
     * 0 = +/- 2g
     * 1 = +/- 4g
     * 2 = +/- 8g
     * 3 = +/- 16g
     */
    imu.setFullScaleAccelRange(ACC_RANGE);
}

void loop() {

      if (millis() > last_interval_ms + INTERVAL_MS) {
        last_interval_ms = millis();
        
        // read raw accel/gyro measurements from device
        imu.getAcceleration(&ax, &ay, &az);

        // converting to m/s2
        float ax_m_s2 = ax * CONVERT_G_TO_MS2;
        float ay_m_s2 = ay * CONVERT_G_TO_MS2;
        float az_m_s2 = az * CONVERT_G_TO_MS2;

        Serial.print(ax_m_s2); 
        Serial.print("\t");
        Serial.print(ay_m_s2); 
        Serial.print("\t");
        Serial.println(az_m_s2); 
      }
}
