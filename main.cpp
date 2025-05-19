#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <HardwareSerial.h>
#include "DHTesp.h"
#include <BH1750.h>
#include <MQUnifiedsensor.h>
#include <my_mqtt.h>

#define Board ("ESP-32")         
#define Voltage_Resolution (3.3) 
#define ADC_Bit_Resolution (12)  
#define RatioMQ2CleanAir (9.83)  
#define RatioMQ135CleanAir 3.6   

#define LED_PIN 12            // LED引脚为12

MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ_2, "MQ-2");     // MQ2 烟雾
MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ_135, "MQ-7"); // MQ135 甲醛

BH1750 lightMeter(0x23);           // BH1750地址
HardwareSerial MySerial1(1);       // RX1 (GPIO18), TX1 (GPIO17)
HardwareSerial MySerial2(2);       // RX2 (GPIO16), TX2 (GPIO15)
TFT_eSPI tft = TFT_eSPI(128, 160); // Invoke library, pins defined in User_Setup.h
DHTesp dht;
TempAndHumidity newValues; // 温湿度

volatile uint8_t ucTemp;        // 用于存储接收到的字节
volatile uint8_t u1_number = 0; // 用于记录接收到的字节数量
volatile uint8_t DATAH = 0;     // 存储数据字节1
volatile uint8_t DATAL = 0;     // 存储数据字节2
volatile uint8_t CHECKSUM = 0;  // 存储校验字节

uint8_t count = 0;
bool getflag = 0;
uint8_t rdata[10];
uint16_t USART2_RX_STA = 0; // 接收状态标记
uint8_t USART2_RX_BUF[20];  // 接收缓冲,最大USART_REC_LEN个字节.

float lux;              // 存储光照强度值
uint16_t PM2_5_val = 0; // 存储PM2.5浓度值
float CH2Odata;
float MQ_2_value = 0;
float MQ_135_value = 0;
float decibel = 0;

// 定义缓冲区
char lcdBuf[250]; // 用于存储格式化后的字符串
unsigned long lastMsg = 0;
// 定义变量保存上一次的值
float lastTemp = -1;
float lastHumidity = -1;
float lastLux = -1;
float lastMQ2 = -1;
float lastMQ135 = -1;
float lastCH2O = -1;
float lastDecibel = -1;
int lastPM2_5 = -1;

bool water_status = false;
bool fan_status = false;


float humi_mix = 30.0;
float temp_max = 30.0;
float light_max = 200.0;
float light_mix = 40.0;
float MQ_2_max = 50;
float MQ_135_max = 50;

void CO2GetData(uint16_t *data);
void CH2OGetData(float *data);
void serial1Event();
void serial2Event();
void MQ_init(void);
float getDecibelValue();
void fan_control(bool status);
void water_control(bool status);
int read_key1();
int read_key2();
void setup(void)
{
  Serial.begin(115200);
  mqtt_init();// mqtt初始化
  pinMode(water_motor, OUTPUT);
  pinMode(fan_motor, OUTPUT);
  fan_control(false);
  water_control(false);

  // 初始化新增的LED引脚为输出模式
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

                             
  MySerial1.begin(9600, SERIAL_8N1, 18, 17); // RXD,TXD
  MySerial1.onReceive(serial1Event);         // 设置串口接收中断回调函数
  MySerial2.begin(9600, SERIAL_8N1, 16, 15); // RXD,TXD
  MySerial2.onReceive(serial2Event);         // 设置串口接收中断回调函数
  MQ_init();                                 // MQ2、mq135初始化
  Wire.begin(3, 2);
  tft.init();
  tft.setRotation(0);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  tft.fillScreen(TFT_WHITE);
  dht.setup(DHT, DHTesp::DHT11);                      // 温湿度初始化
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE); // 光照模块初始化

  pinMode(BEEP, OUTPUT);
  pinMode(KEY1, INPUT_PULLUP);
  pinMode(KEY2, INPUT_PULLUP);
  pinMode(BOMA1, INPUT_PULLUP);
  pinMode(BOMA2, INPUT_PULLUP);

  // 显示静态文本（变量名称）
  tft.drawString("Temp: ", 10, 2);
  tft.drawString("Humidit: ", 10, 20);
  tft.drawString("Light: ", 10, 40);
  tft.drawString("MQ-2: ", 10, 60);
  tft.drawString("MQ-135: ", 10, 80);
  tft.drawString("CH2O: ", 10, 100);
  tft.drawString("Decibel: ", 10, 120);
  tft.drawString("PM2.5: ", 10, 140);
}

void loop()
{

  unsigned long now = millis();
  if (now - lastMsg > 1000)
  {
    lastMsg = now;
    mqtt_reconnect();
    mqtt_publish();
  }
  newValues = dht.getTempAndHumidity(); // 获取温湿度

  lux = lightMeter.readLightLevel(); // 获取光照强度

 

  MQ2.update();
  MQ_2_value = MQ2.readSensor();
  // printf("MQ_2_value: %0.2f ppm\r\n", MQ_2_value);

  MQ135.update();
  MQ_135_value = MQ135.readSensor();
  // printf("MQ_135_value: %0.2f ppm\r\n", MQ_135_value);

  CH2OGetData(&CH2Odata);
  // printf("PM2.5 Data: %d \r\n", PM2_5_val);

  decibel = getDecibelValue();
 
  if (newValues.temperature != lastTemp)
  {
    tft.fillRect(50, 2, 60, 14, TFT_WHITE); // 清除旧数据区域
    sprintf(lcdBuf, "%d.%d C", (int)newValues.temperature, (int)(newValues.temperature * 10) % 10);
    tft.drawString(lcdBuf, 50, 2);
    lastTemp = newValues.temperature;
  }

  // 显示湿度
  if (newValues.humidity != lastHumidity)
  {
    tft.fillRect(70, 20, 60, 14, TFT_WHITE);
    sprintf(lcdBuf, "%d.%d %%", (int)newValues.humidity, (int)(newValues.humidity * 10) % 10);
    tft.drawString(lcdBuf, 70, 20);
    lastHumidity = newValues.humidity;
  }


  // 显示光照强度
  if (lux != lastLux)
  {
    tft.fillRect(60, 40, 60, 14, TFT_WHITE);
    sprintf(lcdBuf, "%0.1f lx", lux);
    tft.drawString(lcdBuf, 60, 40);
    lastLux = lux;
  }



  // 显示MQ-2传感器值
  if (MQ_2_value != lastMQ2)
  {
    tft.fillRect(60, 60, 60, 14, TFT_WHITE);
    sprintf(lcdBuf, "%0.2f ppm", MQ_2_value);
    tft.drawString(lcdBuf, 60, 60);
    lastMQ2 = MQ_2_value;
  }

  // 显示MQ-135传感器值
  if (MQ_135_value != lastMQ135)
  {
    tft.fillRect(60, 80, 60, 14, TFT_WHITE);
    sprintf(lcdBuf, "%0.2f ppm", MQ_135_value);
    tft.drawString(lcdBuf, 60, 80);
    lastMQ135 = MQ_135_value;
  }

  // 显示甲醛数据
  if (CH2Odata != lastCH2O)
  {
    tft.fillRect(60, 100, 80, 14, TFT_WHITE);
    sprintf(lcdBuf, "%0.3f mg/m3", CH2Odata);
    tft.drawString(lcdBuf, 60, 100);
    lastCH2O = CH2Odata;
  }

  // 显示声音分贝值
  if (decibel != lastDecibel)
  {
    tft.fillRect(60, 120, 60, 14, TFT_WHITE);
    sprintf(lcdBuf, "%0.2f dB", decibel);
    tft.drawString(lcdBuf, 60, 120);
    lastDecibel = decibel;
  }

  // 显示PM2.5数据
  if (PM2_5_val != lastPM2_5)
  {
    tft.fillRect(60, 140, 60, 14, TFT_WHITE);
    sprintf(lcdBuf, "%d ug/m3", PM2_5_val);
    tft.drawString(lcdBuf, 60, 140);
    lastPM2_5 = PM2_5_val;
  }

  if (digitalRead(BOMA1) == LOW) // 自动模式
  {
    if (newValues.temperature > temp_max || MQ_2_value > MQ_2_max || MQ_135_value > MQ_135_max) // 温度过高、气体超过阈值
    {
      digitalWrite(BEEP, HIGH); // 响
      delay(200);
      digitalWrite(BEEP, LOW);
      delay(200);
      fan_control(true);
    }
    else
    {
      fan_control(false);
    }
    if (newValues.humidity < humi_mix) // 湿度过低
    {
      digitalWrite(BEEP, HIGH); // 响
      delay(200);
      digitalWrite(BEEP, LOW);
      delay(200);
      water_control(true);
    }
    else
    {
      water_control(false);
    }
  }
  else // 手动模式
  {
    int key1_number = read_key1();
    int key2_number = read_key2();
    if (key1_number == 1)
    {
      printf("key1 pressed\n");
      water_control(true);
    }
    else if (key1_number == 0)
    {
      printf("key1 pressed\n");
      water_control(false);
    }
    if (key2_number == 1)
    {
      printf("key2 pressed\n");
      fan_control(true);
    }
    else if (key2_number == 0)
    {
      printf("key2 pressed\n");
      fan_control(false);
    }
  }

  delay(200);
}


void serial1Event()
{
  while (MySerial1.available())
  {
    uint8_t receivedByte = MySerial1.read(); // 读取一个字节

    if (receivedByte == 0x2C)
    {            // 如果接收到分隔符（0x2C）
      count = 0; // 重置计数器
    }

    rdata[count] = receivedByte; // 存储接收到的字节
    count++;

    if (count >= 10)
    { // 如果接收到10个字节

      count = 0; // 重置计数器
    }
  }
}


void CO2GetData(uint16_t *data)
{
  *data = (uint16_t)(rdata[6] * 256 + rdata[7]);
}
void CH2OGetData(float *data)
{
  *data = (float)(rdata[4] * 256 + rdata[5]) * 0.001; // 数据转换成mg/m3
}

void serial2Event()
{
  while (MySerial2.available())
  {
    ucTemp = MySerial2.read(); // 读取接收到的数据

    if (u1_number == 0 && ucTemp == 0xA5)
    { // 如果是第一个字节且为特征字节
      u1_number++;
    }
    else if (u1_number > 0 && u1_number < 4)
    { // 如果已经接收到特征字节，继续接收数据
      if (u1_number == 1)
      {
        DATAH = ucTemp; // 存储数据字节1
        u1_number++;
      }
      else if (u1_number == 2)
      {
        DATAL = ucTemp; // 存储数据字节2
        u1_number++;
      }
      else if (u1_number == 3)
      {
        CHECKSUM = ucTemp; // 存储校验字节

        uint8_t sum = 0xA5 + DATAH + DATAL; // 计算校验和
        sum = sum ^ 0x80;                   // 异或，得到低7位数据

        if (sum != CHECKSUM)
        {                // 如果校验失败
          u1_number = 0; // 重置计数器
        }
        else
        {
          // 校验成功，计算浓度值
          PM2_5_val = (DATAH << 7) | (DATAL & 0x7F);
        }
        u1_number = 0; // 重置计数器，准备接收下一帧数据
      }
    }
    else
    {
      u1_number = 0; // 如果接收到的数据不是特征字节，重置计数器
    }
  }
}

void MQ_init(void)
{
  MQ2.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ2.setA(574.25);
  MQ2.setB(-2.222); // Configure the equation to to calculate LPG concentration
  MQ2.init();
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++)
  {
    MQ2.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
    Serial.print(".");
  }
  MQ2.setR0(calcR0 / 10);
  Serial.println("MQ2  done!.");

  MQ135.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ135.setA(102.2);
  MQ135.setB(-2.473); // Configure the equation to to calculate NH4 concentration
  MQ135.init();
  calcR0 = 0;
  for (int i = 1; i <= 10; i++)
  {
    MQ135.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0 / 10);
  Serial.println(" MQ135 done!.");
}

// 获取分贝值的函数
float getDecibelValue()
{
  int admax = 0;
  int ad;
  float voltage;
  float voice;

  // 采集20次ADC值，找出最大值
  for (int tt = 0; tt < 20; tt++)
  {
    ad = analogRead(SY01); // 读取声音传感器的ADC值
    if (ad > admax)
      admax = ad;           // 找出最大值
    delayMicroseconds(100); // 延迟100微秒
  }

  // 计算电压值
  voltage = admax * (5.0 / 4095.0);

  // 计算声音分贝值
  voice = 0.028 * voltage * voltage * voltage - 1.25 * voltage * voltage + 17.8 * voltage + 15.25;

  return voice; // 返回分贝值
}
void water_control(bool status)
{
  if (status == true) // 打开
  {
    digitalWrite(water_motor, HIGH);
    water_status = true;
  }
  if (status == false) // 关
  {

    digitalWrite(water_motor, LOW);
    water_status = false;
  }
}
void fan_control(bool status)
{
  if (status == true) // 打开
  {
    digitalWrite(fan_motor, HIGH);
    fan_status = true;
  }
  if (status == false) // 关
  {

    digitalWrite(fan_motor, LOW);
    fan_status = false;
  }
}
int read_key1()
{
  static int lastkey1 = 0;
  if (digitalRead(KEY1) == LOW)
  {
    delay(5);
    if (digitalRead(KEY1) == LOW)
    {
      lastkey1 = !lastkey1; 
      return lastkey1;
    }
  }

  return -1;
}
int read_key2()
{
  static int lastkey2 = 0;
  if (digitalRead(KEY2) == LOW)
  {
    delay(5);
    if (digitalRead(KEY2) == LOW)
    {
      lastkey2 = !lastkey2; 
      return lastkey2;
    }
  }
  return -1;
}

int read_lux(){

  lux = lightMeter.readLightLevel(); // 获取光照强度
    // 控制LED灯：如果光照强度小于10，则打开LED灯
    if (lux < 20) {
      digitalWrite(LED_PIN, HIGH); // 打开LED灯
    } else {
      digitalWrite(LED_PIN, LOW); // 关闭LED灯
    }

}