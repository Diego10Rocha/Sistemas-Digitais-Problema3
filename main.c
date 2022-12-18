#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wiringPi.h>
#include <lcd.h>
#include <stdlib.h>
#include <MQTTClient.h>
#include <pthread.h>

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

//Codigos de requisição
#define GET_NODEMCU_SITUACAO "0x03"
#define GET_NODEMCU_ANALOGICO_INPUT "0x04"
#define GET_NODEMCU_DIGITAL_INPUT "0x05"
#define SET_ON_LED_NODEMCU "0x06"
#define SET_OFF_LED_NODEMCU "0x07"

#define USERNAME "aluno"
#define PASSWORD "@luno*123"

//IP do broker do laboratório
#define MQTT_ADDRESS   "tcp://10.0.0.101"

#define CLIENTID       "2022136"

//tópico de envio de requisições da SBC para a NodeMCU: 
//ligar e desligar o led, obter situação da nodeMCU, obter a medição do sensor analogico
#define MQTT_PUBLISH_TOPIC_ESP_REQUEST    "SBCESP/REQUEST"

//tópico de request da medição de um sensor digital
#define MQTT_PUBLISH_TOPIC_ESP_REQUEST_SENSOR_DIGITAL    "SBCESP/REQUEST/DIGITAL"

//tópico para enviar o valor do novo intervalo de atualização das medições dos sensores
#define MQTT_PUBLISH_TOPIC_ESP_TIMER_INTERVAL "SBC/TIMERINTERVAL"

//tópico de resposta das requisições da SBC para a NodeMCU: 
//ligar e desligar o led, obter situação da nodeMCU
#define MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE  "ESPSBC/RESPONSE"

//Tópico para receber a medição do sensor analogico
#define MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_ANALOGICO  "ESPSBC/RESPONSE/ANALOGICO"

//Tópico para receber a medição do sensor digital selecionado pelo usuario
#define MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_DIGITAL  "ESPSBC/RESPONSE/DIGITAL"

//tópico para receber a medição dos sensores digitais dos 8 pinos da NodeMCU e atualizar no histórico
#define MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_HISTORY_SENSOR_DIGITAL  "ESPSBC/RESPONSE/HISTORY/DIGITAL"

typedef struct{
	char values[16];
	char time[10];
} History;

MQTTClient client;
int lcd;

// Controles de navegação dos menus
int currentMenuOption = 0;
int currentMenuSensorOption = 1;
int currentMenuOptionHistory = 1;
int currentMenuIntervalOption = 1;
int currentMenuStatusNode = 1;
int currentMenuAnalogSensorOption = 1;
int currentHistoryAnalogSensorOption = 0;
int currentHistoryDigitalSensorOption = 0;

// Flags de parada
int stopLoopMainMenu = 0;
int stopLoopConfigMenu = 0;
int stopLoopSensorsMenu = 0;
int stopLoopSetTimeInterval = 0;
int stopLoopSetTimeUnit = 0;
int stopLoopMenu = 0;
int stopLoopMenuHistory = 0;
int stopLoopMenuSituacaoNode = 0;
int stopLoopHistoryDigitalSensors = 0;
int stopLoopHistoryAnalogSensors = 0;
int stopLoopAnalogSensorsMenu = 0;

// Intervalo de Tempo
int timeInterval = 1;
char timeUnit = 's';
int timeUnitAux = 0;
long int timeSeconds = 0;

//variaveis para armazenar as ultimas medições dos sensores
char lastAnalogValue[5];
char lastDigitalValue[2];
char lastValueDigitalSensors[] = {'n','n','n','n','n','n','n','n'};
char timeLastValueDigitalSensors[10];
char timeLastValueAnalogSensors[10];
char* bufDigitalValues;

//variavel para identificar o sensor que foi solicitado a medição digital
char sensor[5];

//variavel para armazenar a ultima resposta da NodeMCU sobre o seu status
char SituacaoNode[15];

//vetores da struct History para armazenar as 10 ultimas medições
History historyListDigital[10];
History historyListAnalog[10];

//variaveis para indicar o numero de medições do histórico + 1
int nextHistoryDigital = 0;
int nextHistoryAnalog = 0;

// Led
int ledState = 0;

//Thread
pthread_t time_now;
//metodo para mostrar informações em uma linha do display LCD
void escreverEmUmaLinha(char linha1[]);
//metodo para mostrar informações nas duas linhas do display LCD
void escreverEmDuasLinhas(char linha1[], char linha2[]);
//metodo para fazer um publish da requisição de medição de um sensor digital
void publishGetDigitalValue();
//metodo para mostrar no display a ultima medição de um sensor digital
void showDigitalSensorValue();
//metodo para mostrar no display a ultima medição de um sensor analogico
void showAnalogicSensorValue();
//metodo para enviar uma requisição para saber o status da NodeMCU
void sendRequestSituacaoNodeMCU();
//Metodo para mostrar no display a situação da NodeMCU
void showSituacaoNodeMCU();

//metodo utilizado nas navegações dos menus, para avançar e para retroceder
void isPressed(int btt, int (*function)(int, int), int* controller, int minMax);
//metodo que caso um botão tenha sido pressionado, executa uma função
void enter(int btt,void (*function)(void));
//metodo para setar uma flag para sair do menu atual exibido no display
void close(int btt,int* stopFlag);

// Incrementa uma variável se não tiver atingido seu valor máximo
int increment(int valueController, int max);

// Decrementa uma variável se não tiver atingido seu valor minimo
int decrement(int valueController, int min);
//altera o estado do led de 1 para 0 ou de 0 para 1
void setLedState();
//menu para escolha do sensor digital que vai ser feita a requisição de sua medição
void sensorsMenu();
//menu de navegação da configuração do intervalo de tempo da atualização das medições
void configMenu();
//menu de navegação entre o histórico das medições dos sensores digitais
void historyDigitalSensors();
//menu de navegação entre o histórico das medições do sensor analogico
void historyAnalogSensors();
//menu de navegação para selecionar qual histórico sera apresentado -> digital/analogico
void historyMenu();
//metodo que atualiza o histórico do sensor analigico
void updateHistoryAnalog();
//metodo que atualiza o histórico dos sensores digitais
void updateHistoryDigital();
//metodo que trata os dados das medições dos sensores digitais para salvar no histórico
void setDigitalValueSensors();

// Ajustar o intervalo de tempo em que os sensores serão atualizados
void setTimeInterval();
// Ajustar a unidade de medida do intervalo de tempo
void setTimeUnit();
//faz o publish para NodeMCU com o novo intervalo de atualização das medições
void publishTimeInterval();
//faz o publish da requisição de medição do sensor analógico
void sendRequestAnalogicSensor();
//metodo do protocolo MQTT que faz o publish em um tópico
void publish(MQTTClient client, char* topic, char* payload);
//Metodo callback para recebimento de mensagens do MQTT
int on_message(void *context, char *topicName, int topicLen, MQTTClient_message *message);
//Metodo para pegar o horário do recebimento da medição dos sensores
void getTime(char buf[]);
//Metodo chamado quando a SBC perde a conexão com o broker do MQTT
void connlost(void *context, char *cause) {
    printf("\nConexão perdida\n");
    printf("     cause: %s\n", cause);
}

int main(){
	wiringPiSetup();
	lcd = lcdInit (2, 16, 4, LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7,0,0,0,0);
	pinMode(BUTTON_1,INPUT);
	pinMode(BUTTON_2,INPUT);
	pinMode(BUTTON_3,INPUT);
	
    //Abertura do arquivo da UART
	int mqtt_connect = -1;
	
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	conn_opts.keepAliveInterval = 1500;
	conn_opts.cleansession = 1;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	
	printf("Iniciando MQTT\n");
	
	/* Inicializacao do MQTT (conexao & subscribe) */
   	MQTTClient_create(&client, MQTT_ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
   	MQTTClient_setCallbacks(client, NULL, connlost, on_message, NULL);

	mqtt_connect = MQTTClient_connect(client, &conn_opts);
	
	if (mqtt_connect == -1){
		printf("\n\rFalha na conexao ao broker MQTT. Erro: %d\n", mqtt_connect);
       		exit(-1);
	}
	//definindo em quais tópicos a SBC estará inscrita
	MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE, 2);
    MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_DIGITAL, 2);
    MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_ANALOGICO, 2);
    MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_HISTORY_SENSOR_DIGITAL, 2);

    while(!stopLoopMainMenu){
        escreverEmDuasLinhas("     MI - SD    ", "Protocolo MQTT  ");
        close(BUTTON_3, &stopLoopMainMenu);
    }
    stopLoopMainMenu = 0;
	//menu principal
	while(!stopLoopMainMenu){
		switch(currentMenuOption){
			case 0:
				escreverEmDuasLinhas("    Situacao    ", "    NODEMCU     ");
				isPressed(BUTTON_2,increment, &currentMenuOption,6);
				isPressed(BUTTON_1,decrement, &currentMenuOption,0);
                enter(BUTTON_3, sendRequestSituacaoNodeMCU);
				break;
			case 1:
				escreverEmDuasLinhas("LEITURA:        ", "SENSOR DIGITAL  ");
				isPressed(BUTTON_2,increment,&currentMenuOption,6);
				isPressed(BUTTON_1,decrement,&currentMenuOption,0);
				enter(BUTTON_3, sensorsMenu);
				break;
			case 2:
				escreverEmDuasLinhas("LEITURA:        ", "SENSOR ANALOGICO");
				isPressed(BUTTON_2,increment, &currentMenuOption,6);
				isPressed(BUTTON_1,decrement, &currentMenuOption,0);
				enter(BUTTON_3, sendRequestAnalogicSensor);
				break;
			case 3:
				lcdHome(lcd);
				if(ledState == 0){
					lcdPrintf(lcd,"LED:    ON  %cOFF",0xFF);
				}else{
					lcdPrintf(lcd,"LED:    ON%c  OFF",0xFF);
				}
				lcdPosition(lcd,0,1);
				lcdPuts(lcd,"                ");
				isPressed(BUTTON_2,increment, &currentMenuOption,6);
				isPressed(BUTTON_1,decrement, &currentMenuOption,0);
				enter(BUTTON_3,setLedState);
				break;

			case 4:
				escreverEmUmaLinha("  CONFIGURACOES ");
				isPressed(BUTTON_2,increment, &currentMenuOption,6);
				isPressed(BUTTON_1,decrement, &currentMenuOption,0);
				enter(BUTTON_3,configMenu);
				break;
            case 5:
				escreverEmUmaLinha("    HISTORICO   ");
				isPressed(BUTTON_2,increment,&currentMenuOption,6);
				isPressed(BUTTON_1,decrement,&currentMenuOption,0);
				enter(BUTTON_3,historyMenu);
				break;
			case 6:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment, &currentMenuOption,6);
				isPressed(BUTTON_1,decrement, &currentMenuOption,0);
				close(BUTTON_3,&stopLoopMainMenu);
				break;
		}
	}
	escreverEmDuasLinhas("  Execucao      ", "  Encerrada     ");
    pthread_join(time_now,NULL);
    exit(-1);
	return 0;	
}

void sendRequestSituacaoNodeMCU(){
	publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST, GET_NODEMCU_SITUACAO);
	lcdClear(lcd);
    showSituacaoNodeMCU();
}

void showSituacaoNodeMCU(){
    while(!stopLoopMenuSituacaoNode) {
		switch(currentMenuStatusNode) {
			case 1:
				
                escreverEmDuasLinhas("NodeMCU", SituacaoNode);
				isPressed(BUTTON_2,increment,&currentMenuStatusNode,2);
				isPressed(BUTTON_1,decrement,&currentMenuStatusNode,1);
				break;
			case 2:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment,&currentMenuStatusNode,2);
				isPressed(BUTTON_1,decrement,&currentMenuStatusNode,1);
				close(BUTTON_3,&stopLoopMenuSituacaoNode);
				break;
		}
	}
    stopLoopMenuSituacaoNode = 0;
	currentMenuStatusNode = 1;
    lcdClear(lcd);
}

void sendRequestAnalogicSensor(){
	publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST, GET_NODEMCU_ANALOGICO_INPUT);
	lcdClear(lcd);
    showAnalogicSensorValue();
}

void showAnalogicSensorValue(){
    while(!stopLoopAnalogSensorsMenu) {
		switch(currentMenuAnalogSensorOption) {
			case 1:
				lcdHome(lcd);
				lcdPuts(lcd,"    SENSOR A0   ");
				lcdPosition(lcd,0,1);
				lcdPrintf(lcd,"    VALOR:%s  ",lastAnalogValue);
				isPressed(BUTTON_2,increment,&currentMenuAnalogSensorOption,2);
				isPressed(BUTTON_1,decrement,&currentMenuAnalogSensorOption,1);
				break;
			case 2:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment,&currentMenuAnalogSensorOption,2);
				isPressed(BUTTON_1,decrement,&currentMenuAnalogSensorOption,1);
				close(BUTTON_3,&stopLoopAnalogSensorsMenu);
				break;
		}
	}
	
	stopLoopAnalogSensorsMenu = 0;
	currentMenuAnalogSensorOption = 1;
    lcdClear(lcd);
}

void showDigitalSensorValue(){
    while(!stopLoopAnalogSensorsMenu) {
		switch(currentMenuAnalogSensorOption) {
			case 1:
				lcdHome(lcd);
				lcdPrintf(lcd,"    SENSOR:%s  ", sensor);
				lcdPosition(lcd,0,1);
				lcdPrintf(lcd,"    VALOR:%s  ", lastDigitalValue);
				isPressed(BUTTON_2,increment,&currentMenuAnalogSensorOption,2);
				isPressed(BUTTON_1,decrement,&currentMenuAnalogSensorOption,1);
				break;
			case 2:
                escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment,&currentMenuAnalogSensorOption,2);
				isPressed(BUTTON_1,decrement,&currentMenuAnalogSensorOption,1);
				close(BUTTON_3,&stopLoopAnalogSensorsMenu);
				break;
		}
	}
	
	stopLoopAnalogSensorsMenu = 0;
	currentMenuAnalogSensorOption = 1;
    lcdClear(lcd);
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

void isPressed(int btt, int (*function)(int, int), int* controller, int minMax){
	if(digitalRead(btt) == 0){
		delay(90);
		if(digitalRead(btt) == 0){
			*controller = function(*controller, minMax);
			while(digitalRead(btt) == 0);
		}
	} 	
}

void enter(int btt,void (*function)(void)){
	if(digitalRead(btt) == 0){
		delay(90);
		if(digitalRead(btt) == 0){
			function();
			while(digitalRead(btt) == 0);
		}
	} 
}

void close(int btt,int* stopFlag){
	if(digitalRead(btt) == 0){
		delay(90);
		if(digitalRead(btt) == 0){
			*stopFlag = 1;
			while(digitalRead(btt) == 0);
		}
	} 
}

// Incrementa uma variável se não tiver atingido seu valor máximo
int increment(int valueController, int max){
	if(valueController < max){
		valueController++;
	}
	return valueController;
}

// Decrementa uma variável se não tiver atingido seu valor mínimo
int decrement(int valueController, int min){
	if(valueController > min){
		valueController--;
	}
	return valueController;
}

void setLedState(){
	if(ledState == 0){
		ledState = 1;
		publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST, SET_ON_LED_NODEMCU);
	}else{
		ledState = 0;
		publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST, SET_OFF_LED_NODEMCU);
	}
}

void configMenu(){
	while(!stopLoopConfigMenu){
		switch(currentMenuIntervalOption){
			case 1:
				escreverEmDuasLinhas("     AJUSTAR    ", "    INTERVALO   ");
				isPressed(BUTTON_2,increment, &currentMenuIntervalOption,3);
				isPressed(BUTTON_1,decrement, &currentMenuIntervalOption,1);
				enter(BUTTON_3,setTimeInterval);
				break;
			case 2:
				escreverEmDuasLinhas("     AJUSTAR    ", "   UNID. TEMPO  ");
				isPressed(BUTTON_2,increment, &currentMenuIntervalOption,3);
				isPressed(BUTTON_1,decrement, &currentMenuIntervalOption,1);
				enter(BUTTON_3,setTimeUnit);
				break;
			case 3:
				escreverEmUmaLinha("      SAIR      ");
				close(BUTTON_3, &stopLoopConfigMenu);
				isPressed(BUTTON_2,increment, &currentMenuIntervalOption,3);
				isPressed(BUTTON_1,decrement, &currentMenuIntervalOption,1);
				break;
		}
		
	}
	
	stopLoopConfigMenu = 0;
	currentMenuIntervalOption = 1;
	lcdClear(lcd);
}

void sensorsMenu(){
	while(!stopLoopSensorsMenu){
		switch(currentMenuSensorOption){
			case 1:
                sprintf(sensor, "D0");
				escreverEmUmaLinha("    SENSOR D0   ");
				isPressed(BUTTON_2,increment, &currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement, &currentMenuSensorOption,1);
				enter(BUTTON_3, publishGetDigitalValue);
				break;
			case 2:
                sprintf(sensor, "D1");
				escreverEmUmaLinha("    SENSOR D1   ");
				isPressed(BUTTON_2,increment, &currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement, &currentMenuSensorOption,1);
				enter(BUTTON_3, publishGetDigitalValue);
				break;
			case 3:
                sprintf(sensor, "D2");
				escreverEmUmaLinha("    SENSOR D2   ");
				isPressed(BUTTON_2,increment,&currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement,&currentMenuSensorOption,1);
				enter(BUTTON_3, publishGetDigitalValue);
				break;
			case 4:
                sprintf(sensor, "D3");
				escreverEmUmaLinha("    SENSOR D3   ");
				isPressed(BUTTON_2,increment, &currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement, &currentMenuSensorOption,1);
				enter(BUTTON_3, publishGetDigitalValue);
				break;
			case 5:
                sprintf(sensor, "D4");
				escreverEmUmaLinha("    SENSOR D4   ");
				isPressed(BUTTON_2,increment,&currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement,&currentMenuSensorOption,1);
				enter(BUTTON_3, publishGetDigitalValue);
				break;
			case 6:
                sprintf(sensor, "D5");
				escreverEmUmaLinha("    SENSOR D5   ");
				isPressed(BUTTON_2,increment, &currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement, &currentMenuSensorOption,1);
				enter(BUTTON_3, publishGetDigitalValue);
				break;
			case 7:
                sprintf(sensor, "D6");
				escreverEmUmaLinha("    SENSOR D6   ");
				isPressed(BUTTON_2,increment, &currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement, &currentMenuSensorOption,1);
				enter(BUTTON_3, publishGetDigitalValue);
				break;
			case 8:
                sprintf(sensor, "D7");
				escreverEmUmaLinha("    SENSOR D7   ");
				isPressed(BUTTON_2,increment, &currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement, &currentMenuSensorOption,1);
				enter(BUTTON_3, publishGetDigitalValue);
				break;
			case 9:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment, &currentMenuSensorOption,9);
				isPressed(BUTTON_1,decrement, &currentMenuSensorOption,1);
				close(BUTTON_3, &stopLoopSensorsMenu);
				break;
		}	
	}
	stopLoopSensorsMenu = 0;
	lcdClear(lcd);
}

void historyDigitalSensors(){
	lcdClear(lcd);
	
	while(!stopLoopHistoryDigitalSensors){
		if(nextHistoryDigital == 0){
			lcdPrintf(lcd,"  SEM HISTORICO ",historyListDigital[0].values);
			lcdPosition(lcd,0,1);
			lcdPrintf(lcd,"                ");
			close(BUTTON_3,&stopLoopHistoryDigitalSensors);
		}else{
            lcdHome(lcd);
            lcdPrintf(lcd,"%s ",historyListDigital[currentHistoryDigitalSensorOption].values);
            lcdPosition(lcd,0,1);
            lcdPrintf(lcd,"T%i->  %s", currentHistoryDigitalSensorOption + 1,historyListDigital[currentHistoryDigitalSensorOption].time);
            isPressed(BUTTON_2, increment, &currentHistoryDigitalSensorOption, nextHistoryDigital - 1);
            isPressed(BUTTON_1, decrement, &currentHistoryDigitalSensorOption, 0);
            close(BUTTON_3,&stopLoopHistoryDigitalSensors);
		}
		
	}
	stopLoopHistoryDigitalSensors = 0;
	currentHistoryDigitalSensorOption = 1;
}

void historyAnalogSensors(){
	lcdClear(lcd);
	
	while(!stopLoopHistoryAnalogSensors){
		if(nextHistoryAnalog == 0){
			lcdPrintf(lcd,"  SEM HISTORICO ",historyListAnalog[0].values);
			lcdPosition(lcd,0,1);
			lcdPrintf(lcd,"                ");
			close(BUTTON_3,&stopLoopHistoryAnalogSensors);
		}else{urrentHistoryAnalogSensorOption

            lcdHome(lcd);
            lcdPrintf(lcd,"%s ",historyListAnalog[currentHistoryAnalogSensorOption].values);
            lcdPosition(lcd,0,1);
            lcdPrintf(lcd,"T%i-> %s", currentHistoryAnalogSensorOption + 1, historyListAnalog[currentHistoryAnalogSensorOption].time);
            isPressed(BUTTON_2, increment, &currentHistoryAnalogSensorOption, nextHistoryAnalog - 1);
            isPressed(BUTTON_1, decrement, &currentHistoryAnalogSensorOption, 0);
            close(BUTTON_3,&stopLoopHistoryAnalogSensors);
		}
		
	}
	stopLoopHistoryAnalogSensors = 0;
	currentHistoryAnalogSensorOption = 1;
}

void historyMenu(){
	while(!stopLoopMenuHistory){
		switch(currentMenuOptionHistory){
			case 1:
				lcdHome(lcd);
				lcdPuts(lcd,"HISTORICO:      ");
				lcdPosition(lcd,0,1);
				lcdPuts(lcd,"SENSOR DIGITAL  ");
				isPressed(BUTTON_2,increment,&currentMenuOptionHistory,3);
				isPressed(BUTTON_1,decrement,&currentMenuOptionHistory,1);
				enter(BUTTON_3,historyDigitalSensors);
				break;
			case 2:
				lcdHome(lcd);
				lcdPuts(lcd,"HISTORICO:      ");
				lcdPosition(lcd,0,1);
				lcdPuts(lcd,"SENSOR ANALOGICO");
				isPressed(BUTTON_2,increment,&currentMenuOptionHistory,3);
				isPressed(BUTTON_1,decrement,&currentMenuOptionHistory,1);
				enter(BUTTON_3,historyAnalogSensors);
				break;
			case 3:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment,&currentMenuOptionHistory,3);
				isPressed(BUTTON_1,decrement,&currentMenuOptionHistory,1);
				close(BUTTON_3,&stopLoopMenuHistory);
				break;
		}
	}
	stopLoopMenuHistory = 0;
	currentMenuOptionHistory = 1;
	lcdClear(lcd);
}

void updateHistoryAnalog(){
	//se o vetor não estiver cheio coloca a medição na proxima posição disponivel
	if(nextHistoryAnalog<10){
		sprintf(historyListAnalog[nextHistoryAnalog].values,"%s",lastAnalogValue);
		strcpy(historyListAnalog[nextHistoryAnalog].time,timeLastValueAnalogSensors);
		nextHistoryAnalog++;
	}else{
		//se o vetor estiver cheio, desloca os 9 ultimos para as 9 primeiras posições e 
		//sobrescreve o valor da ultima posição com a medição atual
		for(int i=0;i<9;i++){
			sprintf(historyListAnalog[i].values,"%s",historyListAnalog[i+1].values);
			sprintf(historyListAnalog[i].time,"%s",historyListAnalog[i+1].time);
		}
		sprintf(historyListAnalog[nextHistoryAnalog-1].values,"%s",lastAnalogValue);
		strcpy(historyListAnalog[nextHistoryAnalog-1].time,timeLastValueAnalogSensors);
	}
}

void updateHistoryDigital(){
	//se o vetor não estiver cheio coloca a medição na proxima posição disponivel
	if(nextHistoryDigital < 10){
		sprintf(historyListDigital[nextHistoryDigital].values,"%c,%c,%c,%c,%c,%c,%c,%c", lastValueDigitalSensors[0],lastValueDigitalSensors[1],lastValueDigitalSensors[2],lastValueDigitalSensors[3],lastValueDigitalSensors[4],lastValueDigitalSensors[5],lastValueDigitalSensors[6],lastValueDigitalSensors[7]);
		strcpy(historyListDigital[nextHistoryDigital].time,timeLastValueDigitalSensors);
		nextHistoryDigital++;
	}else{
		//se o vetor estiver cheio, desloca os 9 ultimos para as 9 primeiras posições e 
		//sobrescreve o valor da ultima posição com a medição atual
		for(int i=0;i<9;i++){
			sprintf(historyListDigital[i].values,"%s",historyListDigital[i+1].values);
			sprintf(historyListDigital[i].time,"%s",historyListDigital[i+1].time);
		}
		sprintf(historyListDigital[nextHistoryAnalog-1].values,"%c,%c,%c,%c,%c,%c,%c,%c", lastValueDigitalSensors[0],lastValueDigitalSensors[1],lastValueDigitalSensors[2],lastValueDigitalSensors[3],lastValueDigitalSensors[4],lastValueDigitalSensors[5],lastValueDigitalSensors[6],lastValueDigitalSensors[7]);
		strcpy(historyListDigital[nextHistoryAnalog-1].time,timeLastValueDigitalSensors);
	}
}

void setDigitalValueSensors(){
  char * substr =  malloc(50);
  substr = strtok(bufDigitalValues, ",");
   // loop through the string to extract all other tokens
  while( substr != NULL ) {
      char *pinName = malloc(2);
      strncpy(pinName, substr,2);
      int index = ((int)pinName[1]) - ((int)'0');

      char *pinValue = malloc(2);
      strncpy(pinValue, substr+3,1);
      lastValueDigitalSensors[index] = *pinValue;

      substr = strtok(NULL, ",");
   }
   getTime(timeLastValueDigitalSensors);
   updateHistoryDigital();
}

// Ajustar o intervalo de tempo em que os sensores serão atualizados
void setTimeInterval(){
	lcdClear(lcd);
	lcdPuts(lcd,"   INTERVALO    ");
	while(!stopLoopSetTimeInterval){
		lcdPosition(lcd,0,1);
		lcdPrintf(lcd,"1%c",timeUnit);
		for(int i=1;i <=timeInterval; i++){
			lcdPutchar(lcd,0xFF);
		}
		
		for(int aux = timeInterval; aux < 10; aux++){
			lcdPuts(lcd," ");
		}
		
		lcdPrintf(lcd,"10%c ",timeUnit);
		isPressed(BUTTON_2,increment,&timeInterval,10);
		isPressed(BUTTON_1,decrement,&timeInterval,1);
		close(BUTTON_3,&stopLoopSetTimeInterval);
	}
	if(timeUnit == 'h'){
		timeSeconds = timeInterval * 3600;
	} else if(timeUnit == 'm'){
		timeSeconds = timeInterval * 60;
	}else{
		timeSeconds = timeInterval; 
	}
    publishTimeInterval();
	stopLoopSetTimeInterval = 0;
	lcdClear(lcd);
}

void setTimeUnit(){
	lcdClear(lcd);
	while(!stopLoopSetTimeUnit){
		lcdHome(lcd);
		
		if(timeUnitAux == 0){
			timeUnit = 's';
		}else if(timeUnitAux == 1){
			timeUnit = 'm';
		}else{
			timeUnit = 'h';
		}
		
		if(timeUnit == 's'){
			lcdPuts(lcd,"UNID. TEMPO: SEG");
		}else if(timeUnit == 'm'){
			lcdPuts(lcd,"UNID. TEMPO: MIN");
		}else{
			lcdPuts(lcd,"UNID. TEMPO: HOR");
		}
		lcdPosition(lcd,0,1);
		lcdPuts(lcd,"                ");	
		isPressed(BUTTON_2, increment, &timeUnitAux,2);
		isPressed(BUTTON_1, decrement, &timeUnitAux,0);
		close(BUTTON_3, &stopLoopSetTimeUnit);
	}
	lcdClear(lcd);
	stopLoopSetTimeUnit = 0;
}

void publishTimeInterval(){
	stopLoopSetTimeUnit = 1;
	char buf[10];
	sprintf(buf,"%ld",timeSeconds);
	publish(client, MQTT_PUBLISH_TOPIC_ESP_TIMER_INTERVAL, buf);
}

void publishGetDigitalValue() {
	char payload[10];
	sprintf(payload,"%d", currentMenuSensorOption);
	publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST_SENSOR_DIGITAL, payload);
    showDigitalSensorValue();
}

void getTime(char buf[]){
	//ponteiro para struct que armazena data e hora  
	struct tm *timeNow;     
	
	//variável do tipo time_t para armazenar o tempo em segundos  
	time_t seconds;
	 
	//obtendo o tempo em segundos  
	time(&seconds);   

	//para converter de segundos para o tempo local  
	//utilizamos a função localtime  
	timeNow = localtime(&seconds);  
	
	char buf_hour[3];
	char buf_min[3];
	char buf_sec[3];
	
	timeNow -> tm_hour <=9 ? sprintf(buf_hour,"0%d",(timeNow -> tm_hour)):sprintf(buf_hour,"%d",(timeNow -> tm_hour));

	timeNow -> tm_min <=9 ? sprintf(buf_min,"0%d",(timeNow -> tm_min)):sprintf(buf_min,"%d",(timeNow -> tm_min));

	timeNow -> tm_sec <=9 ? sprintf(buf_sec,"0%d",(timeNow -> tm_sec)):sprintf(buf_sec,"%d",(timeNow -> tm_sec));
	
	sprintf(buf,"%s:%s:%s",buf_hour,buf_min,buf_sec);
      
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
    //escreverEmDuasLinhas("Mensagem:", payload);
    if(strcmp(topicName, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE) == 0) {
        if(strcmp(payload, "0x200") == 0) {
            printf("NodeMCU executando normalmente\n");
            sprintf(SituacaoNode, "funcionando");
        } else if(strcmp(payload, "0x1F") == 0) {
            printf("NodeMCU com problema\n");
            sprintf(SituacaoNode, "com problema");
        } else if(strcmp(payload, SET_ON_LED_NODEMCU) == 0) {
            printf("LED ligado");
        } else if(strcmp(payload, SET_OFF_LED_NODEMCU) == 0) {
            printf("LED apagado");
        } else if(strcmp(payload,"0xFA") == 0){
			printf("Intervalo mudado\n");
        } 
		printf("%s", SituacaoNode);
    } else if(strcmp(MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_ANALOGICO, topicName) == 0) {
        printf("Leitura do sensor analógico: %s", payload);
        strcpy(lastAnalogValue, payload);
		printf("%s\n", lastAnalogValue);
        getTime(timeLastValueAnalogSensors);
		printf("%s\n", timeLastValueAnalogSensors);
        updateHistoryAnalog();
    } else if(strcmp(topicName, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_DIGITAL) == 0) {
        printf("LVL Sensor: %s\n", payload);
        char value [50];
        sprintf(value, "%s", payload);
        strcpy(lastDigitalValue, value);
	} else if (strcmp(topicName, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_HISTORY_SENSOR_DIGITAL) == 0) {
        bufDigitalValues = payload;
    	setDigitalValueSensors();
    }
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}
