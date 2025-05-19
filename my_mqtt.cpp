#include <WiFi.h>
#include <PubSubClient.h>
#include <my_mqtt.h>
#include "DHTesp.h"
#include "ArduinoJson.h"



const char *ssid = "遥遥领先";
const char *password = "12345678900";
const char *mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);


extern void fan_control(bool status);
extern void water_control(bool status);
extern TempAndHumidity newValues; // 温湿度
extern float lux;          // 存储光照强度值
extern uint16_t PM2_5_val; // 存储PM2.5浓度值
extern float CH2Odata;
extern float MQ_2_value;
extern float MQ_135_value;
extern float decibel;
extern bool water_status;
extern bool fan_status;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi()
{

    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    // 将 payload 转换为字符串
    char message[length + 1];
    for (unsigned int i = 0; i < length; i++)
    {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';

    Serial.println(message);

    // 解析 JSON 消息
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    // 检查是否存在 "target" 字段
    if (doc.containsKey("target"))
    {
        // 获取 "target" 字段的值
        const char *target = doc["target"];

        // 检查是否为 "mod"
        if (strcmp(target, "water") == 0)
        {
            // 获取 "value" 字段的值
            int value = doc["value"];
            if (value == 1)
            {
                water_control(true);
            }
            else
            {
                water_control(false);
            }
        }
        else if (strcmp(target, "fan") == 0)
        {
            // 获取 "value" 字段的值
            int value = doc["value"];
            if (value == 1)
            {
                fan_control(true);
            }
            else
            {
                fan_control(false);
            }

        }

        mqtt_publish();
    }
    else
    {
        Serial.println("No 'target' field found");
    }
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str()))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            // client.publish(topic_pub, "hello world"); // 发布
            // ... and resubscribe
            client.subscribe(topic_sub); // 订阅
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void mqtt_init()
{
    pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin as an output
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

// void loop() {
//  if (!client.connected()) {
//     reconnect();
//   }
//   client.loop();
//   unsigned long now = millis();
//   if (now - lastMsg > 2000) {
//     lastMsg = now;
//     ++value;
//     snprintf (msg, MSG_BUFFER_SIZE, "hello world #%ld", value);
//     Serial.print("Publish message: ");
//     Serial.println(msg);
//     client.publish("outTopic", msg);
//   }
// }

void mqtt_publish()
{

    sprintf(msg, "{\"MQ2\":%0.2f,\"MQ135\":%0.2f,\"decibel\":%0.2f,\"PM25\":%d,\"temp\":%0.2f,\"humi\":%0.2f,\"light\":%0.2f,\"CH2O\":%0.2f,\"water\":%d,\"fan\":%d}", MQ_2_value,
        MQ_135_value, decibel, PM2_5_val, newValues.temperature, newValues.humidity, lux,CH2Odata,water_status,fan_status);

    client.publish(topic_pub, msg);
}
void mqtt_reconnect()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();
}
const char *ntpServer = "ntp1.aliyun.com"; //"pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
struct tm timeinfo;

void printLocalTime()
{

    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    // Serial.println(&timeinfo, "%F %T %A"); // 格式化输出
}

void Get_Clock_Value()
{

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
}
// 初始化 NTP 并获取时间
void initNTP()
{
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time from NTP server");
    }
    else
    {
        Serial.println("Time obtained from NTP server");
    }
}
// 更新本地 RTC 时间
void updateLocalTime()
{
    time_t now;
    time(&now);                   // 获取当前时间戳
    localtime_r(&now, &timeinfo); // 转换为本地时间
}