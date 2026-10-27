
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
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
WebServer server(80);

// Phone connects directly to this ESP32 Wi-Fi
const char* apName = "ASTRA_AI_ALERT";
const char* apPassword = "Astra1234";

bool alarmActive = false;
bool alarmAcknowledged = false;
bool potholeAlert = false;

unsigned long alarmStart = 0;
unsigned long potholeStart = 0;
unsigned long uprightStart = 0;
unsigned long lastDisplay = 0;

const float TILT_LIMIT = 70.0;
const float IMPACT_LIMIT_G = 3.0;

void drawLogo(int x, int y, int r) {
  display.drawCircle(x, y, r, SSD1306_WHITE);
  display.drawCircle(x, y, r - 2, SSD1306_WHITE);
  display.drawLine(x-r/2, y+r/2, x, y-r/2, SSD1306_WHITE);
  display.drawLine(x, y-r/2, x+r/2, y+r/2, SSD1306_WHITE);
  display.drawLine(x-r/4, y+r/6, x+r/4, y+r/6, SSD1306_WHITE);
}

void showSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawLogo(64, 21, 15);
  display.setTextSize(2);
  display.setCursor(20, 40);
  display.println("ASTRA AI");
  display.setTextSize(1);
  display.setCursor(34, 57);
  display.print("SYSTEM START");
  display.display();
  delay(4000);
}

void handleHome() {
  String page =
    "<html><head><meta name='viewport' "
    "content='width=device-width, initial-scale=1'></head>"
    "<body style='font-family:Arial;text-align:center;"
    "padding:25px;background:#101820;color:white'>"
    "<h1>ASTRA AI</h1>"
    "<h3>Smart Ride Safety</h3>"
    "<p>Phone-to-ESP32 connection test</p>"
    "<a href='/pothole' style='display:inline-block;"
    "padding:18px;background:#e63946;color:white;"
    "text-decoration:none;border-radius:12px'>"
    "TEST POTHOLE ALERT</a>"
    "</body></html>";

  server.send(200, "text/html", page);
}

void handlePothole() {
  potholeAlert = true;
  potholeStart = millis();

  Serial.println("PHONE SENT POTHOLE ALERT");

  server.send(200, "text/plain",
              "ASTRA AI: POTHOLE ALERT SENT");
}

void drawDashboard(float leanAngle, float gForce) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawLogo(10, 9, 7);
  display.setTextSize(1);
  display.setCursor(23, 3);
  display.print("ASTRA AI");
  display.setCursor(91, 3);
  display.print("LIVE");
  display.fillCircle(120, 6, 2, SSD1306_WHITE);
  display.drawLine(0, 18, 127, 18, SSD1306_WHITE);

  display.setCursor(3, 23);
  display.print("LEAN ANGLE");
  display.setCursor(78, 23);
  display.print(leanAngle, 1);
  display.print(" deg");

  display.setCursor(3, 33);
  display.print("G-FORCE");
  display.setCursor(78, 33);
  display.print(gForce, 2);
  display.print(" g");

  display.drawRoundRect(1, 44, 126, 19, 3, SSD1306_WHITE);

  if (alarmActive) {
    display.setCursor(9, 50);
    display.print("FALL DETECTED");
  }
  else if (potholeAlert) {
    display.setCursor(9, 50);
    display.print("POTHOLE DETECTED");
  }
  else if (alarmAcknowledged) {
    display.setCursor(9, 50);
    display.print("ALERT COMPLETE");
  }
  else {
    display.setCursor(9, 50);
    display.print("SYSTEM SAFE");
    display.fillCircle(116, 53, 3, SSD1306_WHITE);
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  ledcAttach(BUZZER, 2000, 8);
  ledcWriteTone(BUZZER, 0);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED ERROR");
    while (true) delay(100);
  }

  showSplash();

  if (!mpu.begin(0x68)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.println("MPU6050 ERROR");
    display.display();
    while (true) delay(100);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Start a private Wi-Fi network
  WiFi.mode(WIFI_AP);
  bool apStarted = WiFi.softAP(apName, apPassword);

  if (!apStarted) {
    Serial.println("Wi-Fi AP FAILED");
  }

  server.on("/", HTTP_GET, handleHome);
  server.on("/pothole", HTTP_GET, handlePothole);
  server.begin();

  Serial.println("ASTRA AI READY");
  Serial.print("Wi-Fi name: ");
  Serial.println(apName);
  Serial.print("Wi-Fi password: ");
  Serial.println(apPassword);
  Serial.print("Open phone browser at: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();

  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  float totalG =
    sqrt(ax*ax + ay*ay + az*az) / 9.80665;

  float tilt =
    atan2(sqrt(ax*ax + ay*ay), fabs(az))
    * 180.0 / PI;

  // Existing fall detection
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
  }

  if (alarmAcknowledged && tilt < 30.0 && totalG < 1.4) {
    if (uprightStart == 0) uprightStart = millis();

    if (millis() - uprightStart >= 3000) {
      alarmAcknowledged = false;
      uprightStart = 0;
    }
  }
  else if (alarmAcknowledged) {
    uprightStart = 0;
  }

  // Phone-triggered pothole alert for 5 seconds
  if (potholeAlert &&
      millis() - potholeStart >= 5000) {
    potholeAlert = false;
  }

  // Sound while either alert is active
  if (alarmActive || potholeAlert) {
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
    drawDashboard(tilt, totalG);
  }

  delay(20);
}
