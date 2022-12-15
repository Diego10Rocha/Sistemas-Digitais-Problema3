//#include <TimedAction.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <stdlib.h>
#include <string.h>

//Definicoes
//#ifndef STASSID
//#define STASSID "Nina"
//#define STAPSK  "paralelogramo01"
//#endif

#ifndef STASSID
#define STASSID "INTELBRAS"
#define STAPSK  "Pbl-Sistemas-Digitais"
#define TRUE 1
#endif

//Constantes e variaveis
const char* ssid = STASSID;
const char* password = STAPSK;

const char* mqtt_server = "10.0.0.101";
const char *mqtt_username = "aluno";
const char *mqtt_password = "@luno*123";

byte ledState = HIGH;
String address = "D0";
char activeSensors[] = {'1','1','0','0','0','0','0','0'};
unsigned int sendInterval = 2000;
// Comandos de resposta

#define ANALOG_INPUT_VALUE "0x01"
#define DIGITAL_INPUT_VALUE "0x02"
#define LED_ON "0x06"
#define LED_OFF "0x07"
#define NODE_OK "0x200"

// Comandos de requisição
#define GET_ANALOG_VALUE "0x04"
#define GET_DIGITAL_VALUE "0x05"
#define SET_ON_NODEMCU_LED "0x06"
#define SET_OFF_NODEMCU_LED "0x07"
#define GET_NODE_CONNECTION_STATUS "0x08"
#define GET_NODE_STATUS "0x03"
#define TIME_INTERVAL "0x09"

// Definições dos tópicos
#define REQUEST "SBCESP"
#define RESPONSE "ESPSBC"


// Endereço pino
#define ADDR_D0 "D0"
#define ADDR_D1 "D1"
#define ADDR_D2 "D2"
#define ADDR_D3 "D3"
#define ADDR_D4 "D4"
#define ADDR_D5 "D5"
#define ADDR_D6 "D6"
#define ADDR_D7 "D7"

WiFiClient espClient;
PubSubClient MQTT(espClient);

//void setTimeInterval(int sec);
char getDigitalValue(String addr);
void sendDigitalValues();
void sendAnalogValue();
void getLedState();
void setLedState();

void connectWifi_OTA(){
  Serial.begin(9600);
  delay(10);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();}
  ArduinoOTA.setHostname("ESP-10.0.0.108");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect_MQTT(){
  Serial.print("Attempting MQTT connection...");
  String clientId = "esp8266";
  if (MQTT.connect(clientId.c_str(),mqtt_username,mqtt_password)) {
    Serial.println("connected");
    MQTT.subscribe(REQUEST);
  } else {
    Serial.print("failed, rc=");
    Serial.print(MQTT.state());
    Serial.println(" try again in 5 seconds");
    delay(2000);
  }
}



//espera um playload separado por -, a primeira parte é a msg a segunda o endereço
void callback(char* topic, byte* payload, unsigned int length) {
  String msg, aux = "";
  String adress = "";  
  int split_flag = 0;
  //obtem a string do payload recebido
  for (int i = 0; i < length; i++) {
    char c = (char)payload[i];
    if (c != '-'){
      aux += c;
    }
    else if (c=='-'){
      split_flag = 1; 
      msg = aux;  
    }

    if (split_flag == TRUE && c != '-'){
      adress += c;     
    }    
    
  }  
  //strcmp compara se o topico é igual a requisição 
  if (strcmp(topic, REQUEST) == 0) {

    //Liga o LED
    if (msg == SET_ON_NODEMCU_LED) {
      setLedState(0);
      MQTT.publish(RESPONSE,SET_ON_NODEMCU_LED);
      //Desliga o LED
    } else if (msg == SET_OFF_NODEMCU_LED) {
      setLedState(1);
      MQTT.publish(RESPONSE,SET_OFF_NODEMCU_LED);
      //STATUS DO NODE      
    } else if (msg == GET_NODE_STATUS) {
      MQTT.publish(RESPONSE,NODE_OK);
      
      // VALOR DIGITAL     
    } else if (msg == GET_DIGITAL_VALUE) {
    //  sendDigitalValues();
        DigitalValue(adress);
      //VALOR ANALOGICO      
    } else if (msg == GET_ANALOG_VALUE){
      sendAnalogValue();
    } else if (msg == TIME_INTERVAL) {
	    //setTimeInterval(adress.toInt());
    } else{
      MQTT.publish(RESPONSE,NODE_OK);                
    }

 
}
}
//enviar valor do sensor digital
void DigitalValue(String addr) {
  int value;
  char valueCh;
  char buff[4];
  if (addr == "1") {
    value = digitalRead(D0);
  } else if (addr == "2") {
    value = digitalRead(D1);
  } else if (addr == "3") {
    value = digitalRead(D2);
  } else if (addr == "4") {
    value = digitalRead(D3);
  } else if (addr == "5") {
    value = digitalRead(D4);
  } else if (addr == "6") {
    value = digitalRead(D5);
  } else if (addr == "7") {
    value = digitalRead(D6);
  } else if (addr == "8") {
    value = digitalRead(D7);
  }
  
  if(value == 1){
    valueCh = '1';
  }else if(value == 0){
    valueCh = '0';
  }
  snprintf(buff,4,"%d",DIGITAL_INPUT_VALUE,'-',value);
  MQTT.publish(RESPONSE,buff);
  //return valueCh;
}

//Envia valor do sensor analogico 
void sendAnalogValue() {
    int value = analogRead(A0);
    char buf[10];
    snprintf(buf,10,"%ld",ANALOG_INPUT_VALUE,'-',value);
    MQTT.publish(RESPONSE, buf);
}

//Liga e desliga o led
void setLedState(byte state){
    digitalWrite(LED_BUILTIN,state);
    if(state == 1){
      ledState = 0;  
    }else{
      ledState = 1;
    }
}

void sendAll(){
  
}


/*TimedAction sendAction = TimedAction(sendInterval, sendAll);

void setTimeInterval(int sec) {
  sendInterval = sec * 1000;
  sendAction.setInterval(sendInterval);
  MQTT.publish(RESPONSE, "0xFA");
}*/

void setup() {
  pinMode(LED_BUILTIN,OUTPUT);
  //setTimeInterval(50);
  connectWifi_OTA();
  MQTT.setServer(mqtt_server, 1883);
  MQTT.setCallback(callback);
  
}

void loop() {
  if(!MQTT.connected()){
    reconnect_MQTT();
  }
  ArduinoOTA.handle();
  MQTT.loop();
  //sendAction.check();
}
