#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp32-hal-ledc.h" // 包含用于PWM控制的库

const char* ssid = "KGB";
const char* password = "crqcrqcrq";
const char* host = "192.168.123.206"; // Unity host的IP地址
const int tcpPort = 12345;
const int udpPort = 12346;

WiFiServer tcpServer(tcpPort);
WiFiUDP udp;

const int fanPins[] = {32, 33, 25, 26, 27, 14}; // 控制风扇的GPIO引脚
const int freq = 25000;  // PWM信号的频率，25kHz
const int resolution = 8;  // PWM分辨率，8位表示范围为0-255
const int numberOfFans = 6; // 风扇数量

unsigned long lastUdpTime = 0; // 记录上次收到UDP消息的时间
const long timeout = 2000; // 超时时间（毫秒）

void controlFans(float angle, float windMultiplier) {
  // 初始化所有风扇的PWM值
  int pwmValues[6] = {0, 0, 0, 0, 0, 0};
  
  // 根据角度范围调整每个风扇的PWM值
  if (angle >= -90 && angle < -45) {
    pwmValues[0] = map(angle, -90, -45, 178, 102);  // fan 0
    pwmValues[1] = map(angle, -90, -45, 24, 102);  // fan 1
    pwmValues[4] = map(angle, -90, -45, 217, 255);  // fan 4 & 5
    pwmValues[5] = pwmValues[4];
  } else if (angle >= -45 && angle < -15) {
    pwmValues[0] = 102;  // fan 0
    pwmValues[1] = map(angle, -45, -15, 102, 204);  // fan 1
    pwmValues[2] = map(angle, -45, -15, 0, 178);  // fan 2 & 3
    pwmValues[3] = pwmValues[2];
    pwmValues[4] = map(angle, -45, -15, 255, 217);  // fan 4 & 5
    pwmValues[5] = pwmValues[4];
  } else if (angle >= -15 && angle < 0) {
    pwmValues[0] = map(angle, -15, 0, 102, 255);  // fan 0
    pwmValues[1] = map(angle, -15, 0, 204, 255);  // fan 1
    pwmValues[2] = map(angle, -15, 0, 178, 255);  // fan 2 & 3
    pwmValues[3] = pwmValues[2];
    pwmValues[4] = map(angle, -15, 0, 217, 255);  // fan 4 & 5
    pwmValues[5] = pwmValues[4];
  } else if (angle >= 0 && angle < 15) {
    pwmValues[0] = map(angle, 0, 15, 255, 204);  // fan 0
    pwmValues[1] = map(angle, 0, 15, 255, 102);  // fan 1
    pwmValues[2] = map(angle, 0, 15, 255, 217);  // fan 2 & 3
    pwmValues[3] = pwmValues[2];
    pwmValues[4] = map(angle, 0, 15, 255, 178);  // fan 4 & 5
    pwmValues[5] = pwmValues[4];
  } else if (angle >= 15 && angle < 45) {
    pwmValues[0] = map(angle, 15, 45, 204, 102);  // fan 0
    pwmValues[1] = 102;  // fan 1
    pwmValues[2] = map(angle, 15, 45, 217, 255);  // fan 2 & 3
    pwmValues[3] = pwmValues[2];
    pwmValues[4] = map(angle, 15, 45, 178, 0);  // fan 4 & 5
    pwmValues[5] = pwmValues[4];
  } else if (angle >= 45 && angle <= 90) {
    pwmValues[0] = map(angle, 45, 90, 102, 24);  // fan 0
    pwmValues[1] = map(angle, 45, 90, 102, 178);  // fan 1
    pwmValues[2] = map(angle, 45, 90, 255, 217);  // fan 2 & 3
    pwmValues[3] = pwmValues[2];
    // pwmValues[4] = 0;  // fan 4 & 5
    // pwmValues[5] = 0;
  }

  // 应用PWM值到每个风扇
  for (int i = 0; i < numberOfFans; i++) {
  ledcWrite(i, pwmValues[i] * windMultiplier);
  }
}


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  // Print the IP address of the ESP32
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

  tcpServer.begin(); // 启动TCP服务器监听连接
  udp.begin(udpPort); // 启动UDP

  // 为每个风扇设置PWM通道并初始化
  for (int i = 0; i < numberOfFans; i++) {
    ledcSetup(i, freq, resolution);
    ledcAttachPin(fanPins[i], i);
    ledcWrite(i, 0);                // Set initial fan speed to 0%
  }
}

void loop() {
  // 检查是否有TCP连接用于初始确认
  WiFiClient client = tcpServer.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        String msg = client.readStringUntil('\n');
        Serial.print("Received TCP message: ");
        Serial.println(msg);
        if (msg.indexOf("Hello ESP32") >= 0) {
          client.println("ESP32 Ready"); // 向Unity发送确认消息
          break; // 退出循环并开始监听UDP消息以控制风扇
        }
      }
    }
    client.stop();
  }

  // 监听UDP消息以控制风扇
  int packetSize = udp.parsePacket();
  if (packetSize) {
    lastUdpTime = millis(); // 更新上次收到消息的时间
    char incomingPacket[255];
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';
    }
    // float angle = atof(incomingPacket); // 将接收到的角度字符串转换为浮点数
    // controlFans(angle);

    // 分割字符串，提取角度和倍率
    char* token = strtok(incomingPacket, ",");
    float angle = atof(token); // 第一个部分是角度
    token = strtok(NULL, ",");
    float windMultiplier = atof(token); // 第二个部分是倍率

    controlFans(angle, windMultiplier);
  }

  // 如果超过了超时时间还没有收到UDP消息，则停止所有风扇
  if (millis() - lastUdpTime > timeout) {
    for (int i = 0; i < numberOfFans; i++) {
      ledcWrite(i, 0);  // 关闭风扇
    }
  }
}
