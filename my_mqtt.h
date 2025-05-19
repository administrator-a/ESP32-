#ifndef my_mqtt_h
#define my_mqtt_h

#define topic_pub "bai/Enviro_monit/pub"//MQTTX连接主题
#define topic_sub "bai/Enviro_monit/sub" //APP连接,订阅的主题

//传感器引脚连接
#define BEEP 6
#define KEY1 19
#define KEY2 20
#define BOMA1 7
#define BOMA2 8
#define MQ_2 4
#define MQ_135 5
#define DHT 21
#define SY01 1
#define water_motor 36
#define fan_motor 35



void mqtt_reconnect();
void mqtt_publish();
void mqtt_init();
void Get_Clock_Value();
void initNTP();
void updateLocalTime();
#endif