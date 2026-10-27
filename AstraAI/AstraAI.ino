
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define BUZZER 25
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

bool alarmActive = false;
bool alarmAcknowledged = false;

unsigned long alarmStart = 0;
unsigned long uprightStart = 0;
unsigned long lastDisplay = 0;

const float TILT_LIMIT = 70.0;
const float IMPACT_LIMIT_G = 3.0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  ledcAttach(BUZZER, 2000, 8);
  ledcWriteTone(BUZZER, 0);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED ERROR");
    while (true) {
      ledcWriteTone(BUZZER, 0);
      delay(100);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("ASTRA AI");
  display.setTextSize(1);
  display.println("Starting...");
  display.display();

  if (!mpu.begin(0x68)) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("MPU ERROR");
    display.display();

    while (true) {
      ledcWriteTone(BUZZER, 0);
      delay(100);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  delay(1000);
  Serial.println("ASTRA AI READY");
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  float totalG = sqrt(ax * ax + ay * ay + az * az)
                 / 9.80665;

  float tilt = atan2(
    sqrt(ax * ax + ay * ay),
    fabs(az)
  ) * 180.0 / PI;

  if (!alarmActive && !alarmAcknowledged &&
      (tilt > TILT_LIMIT || totalG > IMPACT_LIMIT_G)) {
    alarmActive = true;
    alarmStart = millis();
    Serial.println("POSSIBLE FALL!");
  }

  if (alarmActive && millis() - alarmStart >= 5000) {
    alarmActive = false;
    alarmAcknowledged = true;
    uprightStart = 0;
    ledcWriteTone(BUZZER, 0);
  }

  if (alarmAcknowledged && tilt < 30.0 && totalG < 1.4) {
    if (uprightStart == 0) {
      uprightStart = millis();
    }

    if (millis() - uprightStart >= 3000) {
      alarmAcknowledged = false;
      uprightStart = 0;
    }
  } else if (alarmAcknowledged) {
    uprightStart = 0;
  }

  if (alarmActive) {
    if ((millis() / 250) % 2 == 0) {
      ledcWriteTone(BUZZER, 1800);
    } else {
      ledcWriteTone(BUZZER, 0);
    }
  } else {
    ledcWriteTone(BUZZER, 0);
  }

  if (millis() - lastDisplay >= 150) {
    lastDisplay = millis();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("ASTRA AI");

    display.setTextSize(1);
    display.print("Tilt: ");
    display.print(tilt, 1);
    display.println(" deg");

    display.print("Motion: ");
    display.print(totalG, 2);
    display.println(" g");
    display.println();

    display.setTextSize(2);
    if (alarmActive) {
      display.println("FALL ALERT");
    } else if (alarmAcknowledged) {
      display.println("ALERT DONE");
    } else {
      display.println("SAFE");
    }

    display.display();
  }

  Serial.print("Tilt: ");
  Serial.print(tilt, 1);
  Serial.print(" deg | Motion: ");
  Serial.print(totalG, 2);
  Serial.println(" g");

  delay(20);
}
