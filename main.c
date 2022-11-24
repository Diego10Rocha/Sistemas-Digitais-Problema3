#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wiringPi.h>
#include <lcd.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>

//USE WIRINGPI PIN NUMBERS
#define LCD_RS  13               //Register select pin
#define LCD_E   18               //Enable Pin
#define LCD_D4  21               //Data pin D4
#define LCD_D5  24               //Data pin D5
#define LCD_D6  26               //Data pin D6
#define LCD_D7  27               //Data pin D7

// Botões 
#define BUTTON_1 19
#define BUTTON_2 23
#define BUTTON_3 25

#define USERNAME "aluno"
#define PASSWORD "@luno*123"

//IP do broker do laboratório
#define MQTT_ADDRESS   "tcp://10.0.0.101"

#define CLIENTID       "2022136"

/* Substitua aqui os topicos de publish e subscribe por topicos exclusivos de sua aplicacao */
#define MQTT_PUBLISH_TOPIC     "/sensor/"
#define MQTT_SUBSCRIBE_TOPIC   "/sensor/"

MQTTClient client;
int lcd;

void escreverEmUmaLinha(char linha1[]);
void escreverEmDuasLinhas(char linha1[], char linha2[]);

void publish(MQTTClient client, char* topic, char* payload);
int on_message(void *context, char *topicName, int topicLen, MQTTClient_message *message);

int main(){
	wiringPiSetup();
	lcd = lcdInit (2, 16, 4, LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7,0,0,0,0);
	pinMode(BUTTON_1,INPUT);
	pinMode(BUTTON_2,INPUT);
	pinMode(BUTTON_3,INPUT);
	
	escreverEmDuasLinhas("     MI - SD    ", "Protocolo MQTT");
	
    	//Abertura do arquivo da UART
	int mqtt_connect = -1;
	
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	
	printf("Iniciando MQTT\n");
	
	/* Inicializacao do MQTT (conexao & subscribe) */
   	MQTTClient_create(&client, MQTT_ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
   	MQTTClient_setCallbacks(client, NULL, NULL, on_message, NULL);

	mqtt_connect = MQTTClient_connect(client, &conn_opts);
	
	if (mqtt_connect == -1){
		printf("\n\rFalha na conexao ao broker MQTT. Erro: %d\n", mqtt_connect);
       		exit(-1);
	}
	
	MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC, 0);
	while(1) {
	
		if(digitalRead(BUTTON_1) == 0) {
			printf("enviando mensagem\n");
			publish(client, MQTT_PUBLISH_TOPIC, "Teste MQTT");
		}
		
	}
	
    	exit(-1);
	return 0;	
}

void escreverEmUmaLinha(char linha1[]) {
	lcdHome(lcd);
	lcdPuts(lcd, linha1);
	lcdPosition(lcd,0,1);
	lcdPuts(lcd, "                ");
}
void escreverEmDuasLinhas(char linha1[], char linha2[]) {
	lcdHome(lcd);
	lcdPuts(lcd, linha1);
	lcdPosition(lcd,0,1);
	lcdPuts(lcd, linha2);
}

/* Funcao: publicacao de mensagens MQTT
 * Parametros: cleinte MQTT, topico MQTT and payload
 * Retorno: nenhum
*/
void publish(MQTTClient client, char* topic, char* payload) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
 
    pubmsg.payload = payload;
    pubmsg.payloadlen = strlen(pubmsg.payload);
    pubmsg.qos = 1;
    pubmsg.retained = 0;
    MQTTClient_deliveryToken token;
    MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    MQTTClient_waitForCompletion(client, token, 1000L);
}

/* Funcao: callback de mensagens MQTT recebidas e echo para o broker
 * Parametros: contexto, ponteiro para nome do topico da mensagem recebida, tamanho do nome do topico e mensagem recebida
 * Retorno : 1: sucesso (fixo / nao ha checagem de erro neste exemplo)
*/
int on_message(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char* payload = message->payload;
 
    /* Mostra a mensagem recebida */
    printf("Mensagem recebida! \n\rTopico: %s Mensagem: %s\n", topicName, payload);
    escreverEmDuasLinhas("Mensagem:", payload);
 
    /* Faz echo da mensagem recebida */
    publish(client, MQTT_PUBLISH_TOPIC, payload);
 
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}
