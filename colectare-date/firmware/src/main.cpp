#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
// Viteza motor
const int TARGET_PWM = 255;
// Pinii i2c mpu6050
const int PIN_I2C_SDA = 21;
const int PIN_I2C_SCL= 22;
// Pinul ENA pe motor
const int PIN_MOTOR_PWM = 14;
// Configurare PWM
const int PWM_Channel = 0;
const int PWM_Frequency = 5000;
const int PWM_Resolution = 8; 
// Obiect MPU6050
Adafruit_MPU6050 mpu;

void setup() {
   Serial.begin(115200);

   Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
   if (!mpu.begin()){
      Serial.println("Eroare la initializarea MPU6050!");
      while (1) { delay(10);};
   }
   mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
   mpu.setGyroRange(MPU6050_RANGE_500_DEG);
   mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

   ledcSetup(PWM_Channel, PWM_Frequency, PWM_Resolution);
   ledcAttachPin(PIN_MOTOR_PWM, PWM_Channel);

   ledcWrite(PWM_Channel, TARGET_PWM);

   delay(3000);

}

void loop(){
   ledcWrite(PWM_Channel, TARGET_PWM);
   sensors_event_t a, g, temp;
   mpu.getEvent(&a, &g, &temp);

   Serial.print(a.acceleration.x);
   Serial.print(",");
   Serial.print(a.acceleration.y);
   Serial.print(",");
   Serial.print(a.acceleration.z);
   Serial.print(",");

   Serial.print(g.gyro.x);
   Serial.print(",");
   Serial.print(g.gyro.y);
   Serial.print(",");
   Serial.print(g.gyro.z);
   Serial.print(",");

   Serial.println(temp.temperature);

   delay(10);
}


