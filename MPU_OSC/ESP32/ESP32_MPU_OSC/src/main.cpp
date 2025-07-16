#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <Wire.h>
#include <MPU6050.h> // Electronic Cats library
#include <WebServer.h>

#define OUTPUT_TEAPOT
#define LED_BUILTIN 2
#define BUTTON_PIN 18

// WiFi credentials
WiFiUDP Udp1, Udp2; // Multiple UDP instances
const char* ssid = "CUCA_BELUDO";
const char* password = "cuca_areka";

// OSC server addresses and ports
String oscServerIp = "192.168.0.10";
const int oscServerPort1 = 8000;
const int oscServerPort2 = 8001;

OSCMessage gyr("/gyr");
OSCMessage acc("/acc");
OSCMessage optMsg("/opt");
int buttonCounter = 1;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 50;

// Create an Electronic Cats MPU6050 object
MPU6050 mpu;

WebServer server(80);

void handleRoot() {
  String html = "<html><body>"
                "<h2>OSC Server IP Configuration</h2>"
                "<form action='/setip' method='POST'>"
                "OSC Server IP: <input type='text' name='ip' value='" + oscServerIp + "'>"
                "<input type='submit' value='Update'>"
                "</form>"
                "</body></html>";
  server.send(200, "text/html", html);
}

void handleSetIp() {
  if (server.hasArg("ip")) {
    oscServerIp = server.arg("ip");
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void sendOptOSC(int value) {
  optMsg.empty();
  optMsg.add((int32_t)value);
  Udp1.beginPacket(oscServerIp.c_str(), oscServerPort1);
  optMsg.send(Udp1);
  Udp1.endPacket();
  optMsg.empty();
  Udp2.beginPacket(oscServerIp.c_str(), oscServerPort2);
  optMsg.send(Udp2);
  Udp2.endPacket();
  optMsg.empty();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  Wire.begin();

  mpu.initialize();
  while (!mpu.testConnection()) {
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
    delay(250);
    digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
    delay(300);
    Serial.println("MPU6050 connection failed");
  }
  Serial.println("MPU6050 connected!");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
  // Start web server
  server.on("/", handleRoot);
  server.on("/setip", HTTP_POST, handleSetIp);
  server.begin();
  Serial.println("Web server started on port 80");
}

void sendOSCMessages(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz) {
  // Publish accelerometer data
  acc.add((float)ax);
  acc.add((float)ay);
  acc.add((float)az);

  // Publish gyroscope data
  gyr.add((float)gx);
  gyr.add((float)gy);
  gyr.add((float)gz);

  // Send to first port
  Udp1.beginPacket(oscServerIp.c_str(), oscServerPort1);
  acc.send(Udp1);
  Udp1.endPacket();
  acc.empty();

  Udp1.beginPacket(oscServerIp.c_str(), oscServerPort1);
  gyr.send(Udp1);
  Udp1.endPacket();
  gyr.empty();

  // Send to second port
  Udp2.beginPacket(oscServerIp.c_str(), oscServerPort2);
  acc.send(Udp2);
  Udp2.endPacket();
  acc.empty();

  Udp2.beginPacket(oscServerIp.c_str(), oscServerPort2);
  gyr.send(Udp2);
  Udp2.endPacket();
  gyr.empty();
}

void loop() {
  server.handleClient();
  // Button logic
  static bool lastButtonState = HIGH;
  bool buttonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && buttonState == LOW) {
    unsigned long now = millis();
    if (now - lastButtonPress > debounceDelay) {
      buttonCounter++;
      if (buttonCounter > 5) buttonCounter = 1;
      sendOptOSC(buttonCounter);
      lastButtonPress = now;
    }
  }
  lastButtonState = buttonState;
  // Get accelerometer and gyroscope data
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  mpu.getAcceleration(&ax, &ay, &az);
  mpu.getRotation(&gx, &gy, &gz);

  // Send OSC messages
  sendOSCMessages(ax, ay, az, gx, gy, gz);

  // Delay before next reading
  delay(150);
}