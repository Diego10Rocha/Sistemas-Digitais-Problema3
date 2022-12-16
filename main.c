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

/* Substitua aqui os topicos de publish e subscribe por topicos exclusivos de sua aplicacao */
#define MQTT_PUBLISH_TOPIC_ESP_REQUEST    "SBCESP/REQUEST"
#define MQTT_PUBLISH_TOPIC_ESP_REQUEST_SENSOR_DIGITAL    "SBCESP/REQUEST/DIGITAL"
#define MQTT_PUBLISH_TOPIC_ESP_TIMER_INTERVAL "SBC/TIMERINTERVAL"
#define MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE  "ESPSBC/RESPONSE"
#define MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_ANALOGICO  "ESPSBC/RESPONSE/ANALOGICO"
#define MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_DIGITAL  "ESPSBC/RESPONSE/DIGITAL"
#define MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_HISTORY_SENSOR_DIGITAL  "ESPSBC/RESPONSE/HISTORY/DIGITAL"
#define MQTT_PUBLISH_TOPIC_SW     "SBCSW"
#define MQTT_SUBSCRIBE_TOPIC_SW   "SWSBC"

typedef struct{
	char values[16];
	char time[10];
} History;

MQTTClient client;
int lcd;

// Controles de navegação dos menus
int currentMenuOption = 0;
int currentMenuSensorOption = 1;
int currentMenuIntervalOption = 1;

// Flags de parada
int stopLoopMainMenu = 0;
int stopLoopConfigMenu = 0;
int stopLoopSensorsMenu = 0;
int stopLoopSetTimeInterval = 0;
int stopLoopSetTimeUnit = 0;
int stopLoopMenu = 0;
// Intervalo de Tempo
int timeInterval = 1;
char timeUnit = 's';
int timeUnitAux = 0;
long int timeSeconds = 0;

char lastAnalogValue[5];
char lastDigitalValue[2];
char lastValueDigitalSensors[] = {'n','n','n','n','n','n','n','n'};
char timeLastValueDigitalSensors[10];
char* bufDigitalValues;
int currentMenuAnalogSensorOption = 1;
int stopLoopAnalogSensorsMenu = 0;
char *sensor;
char *SituacaoNode;

History historyListDigital[10];
History historyListAnalog[10];
int nextHistoryDigital = 0;
int nextHistoryAnalog = 0;

int currentHistoryAnalogSensorOption = 0;
int currentHistoryDigitalSensorOption = 0;

// Led
int ledState = 0;

//Thread
pthread_t time_now;

void escreverEmUmaLinha(char linha1[]);
void escreverEmDuasLinhas(char linha1[], char linha2[]);
void publishGetDigitalValue();
void showDigitalSensorValue();
void showAnalogicSensorValue();
void sendRequestSituacaoNodeMCU();
void showSituacaoNodeMCU();

void isPressed(int btt, int (*function)(int, int), int* controller, int minMax);
void enter(int btt,void (*function)(void));
void close(int btt,int* stopFlag);
// Incrementa uma variável se não tiver atingido seu valor máximo
int increment(int valueController, int max);
// Decrementa uma variável se não tiver atingido seu valor minimo
int decrement(int valueController, int min);
void setLedState();
void sensorsMenu();
void configMenu();
void historyDigitalSensors();
void historyAnalogSensors();
void historyMenu();
void updateHistoryAnalog();
void setDigitalValueSensors();
// Ajustar o intervalo de tempo em que os sensores serão atualizados
void setTimeInterval();
void setTimeUnit();

void publishTimeInterval();
void sendRequestAnalogicSensor();

void publish(MQTTClient client, char* topic, char* payload);
int on_message(void *context, char *topicName, int topicLen, MQTTClient_message *message);

void getTime(char buf[]);

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
	
	//escreverEmDuasLinhas("     MI - SD    ", "Protocolo MQTT");
	
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
	
	MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE, 2);
    MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_DIGITAL, 2);
    MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_HISTORY_SENSOR_DIGITAL, 2);
	MQTTClient_subscribe(client, MQTT_SUBSCRIBE_TOPIC_SW, 2);

    while(!stopLoopMainMenu){
        escreverEmDuasLinhas("     MI - SD    ", "Protocolo MQTT  ");
        close(BUTTON_3, &stopLoopMainMenu);
    }
    stopLoopMainMenu = 0;

	while(!stopLoopMainMenu){
		switch(currentMenuOption){
			case 0:
				escreverEmDuasLinhas("    Situacao    ", "    NODEMCU     ");
				isPressed(BUTTON_2,increment, &currentMenuOption,5);
				isPressed(BUTTON_1,decrement, &currentMenuOption,1);
                enter(BUTTON_3, sendRequestSituacaoNodeMCU);
				break;
			case 1:
				escreverEmDuasLinhas("LEITURA:        ", "SENSOR DIGITAL  ");
				isPressed(BUTTON_2,increment,&currentMenuOption,5);
				isPressed(BUTTON_1,decrement,&currentMenuOption,1);
				enter(BUTTON_3, sensorsMenu);
				break;
			case 2:
				escreverEmDuasLinhas("LEITURA:        ", "SENSOR ANALOGICO");
				isPressed(BUTTON_2,increment, &currentMenuOption,5);
				isPressed(BUTTON_1,decrement, &currentMenuOption,1);
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
				isPressed(BUTTON_2,increment, &currentMenuOption,5);
				isPressed(BUTTON_1,decrement, &currentMenuOption,1);
				enter(BUTTON_3,setLedState);
				break;

			case 4:
				escreverEmUmaLinha("  CONFIGURACOES ");
				isPressed(BUTTON_2,increment, &currentMenuOption,5);
				isPressed(BUTTON_1,decrement, &currentMenuOption,1);
				enter(BUTTON_3,configMenu);
				break;
            case 5:
				escreverEmUmaLinha("    HISTORICO   ");
				isPressed(BUTTON_2,increment,&currentMenuOption,7,1);
				isPressed(BUTTON_1,decrement,&currentMenuOption,7,1);
				enter(BUTTON_3,historyMenu);
				break;
			case 6:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment, &currentMenuOption,5);
				isPressed(BUTTON_1,decrement, &currentMenuOption,1);
				close(BUTTON_3,&stopLoopMainMenu);
				break;

		}
	}
	escreverEmDuasLinhas("     MI - SD    ", "Protocolo MQTT");
    pthread_join(time_now,NULL);
    exit(-1);
	return 0;	
}

void sendRequestSituacaoNodeMCU(){
	publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST, GET_NODEMCU_SITUACAO);
    showSituacaoNodeMCU();
}

void showSituacaoNodeMCU(){
    while(!stopLoopAnalogSensorsMenu) {
		switch(currentMenuAnalogSensorOption) {
			case 1:
				
                escreverEmDuasLinhas("NodeMCU", SituacaoNode);
				isPressed(BUTTON_2,increment,&currentMenuAnalogSensorOption,2,1);
				isPressed(BUTTON_1,decrement,&currentMenuAnalogSensorOption,2,1);
				break;
			case 2:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment,&currentMenuAnalogSensorOption,2,1);
				isPressed(BUTTON_1,decrement,&currentMenuAnalogSensorOption,2,1);
				close(BUTTON_3,&stopLoopAnalogSensorsMenu);
				break;
		}
	}
    stopLoopAnalogSensorsMenu = 0;
	currentMenuAnalogSensorOption = 1;
    lcdClear(lcd);
}

void sendRequestAnalogicSensor(){
	publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST, "0x04");
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
				isPressed(BUTTON_2,increment,&currentMenuAnalogSensorOption,2,1);
				isPressed(BUTTON_1,decrement,&currentMenuAnalogSensorOption,2,1);
				break;
			case 2:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment,&currentMenuAnalogSensorOption,2,1);
				isPressed(BUTTON_1,decrement,&currentMenuAnalogSensorOption,2,1);
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
				lcdPuts(lcd,"    SENSOR %s   ", sensor);
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

// Debounce
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
	if(ledState == 1){
		ledState = 0;
		publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST, "0x06");
	}else{
		ledState = 1;
		publish(client, MQTT_PUBLISH_TOPIC_ESP_REQUEST, "0x07");
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
            lcdPrintf(lcd,"T%i->  %s",historyListDigital[currentHistoryDigitalSensorOption].time, currentHistoryDigitalSensorOption + 1);
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
	
	while(!stopLoopMenu){
		if(nextHistoryAnalog == 0){
			lcdPrintf(lcd,"  SEM HISTORICO ",historyListAnalog[0].values);
			lcdPosition(lcd,0,1);
			lcdPrintf(lcd,"                ");
			close(BUTTON_3,&stopLoopMenu);
		}else{
            lcdHome(lcd);
            lcdPrintf(lcd,"%s ",historyListAnalog[currentHistoryAnalogSensorOption].values);
            lcdPosition(lcd,0,1);
            lcdPrintf(lcd,"T%i-> %s",historyListAnalog[currentHistoryAnalogSensorOption].time, currentHistoryAnalogSensorOption + 1);
            isPressed(BUTTON_2, increment, &currentHistoryAnalogSensorOption, nextHistoryAnalog - 1);
            isPressed(BUTTON_1, decrement, &currentHistoryAnalogSensorOption, 0);
            close(BUTTON_3,&stopLoopMenu);
		}
		
	}
	stopLoopHistoryAnalogSensors = 0;
	currentHistoryAnalogSensorOption = 1;
}

void historyMenu(){
	while(!stopLoopMenu){
		switch(currentMenuOption){
			case 1:
				lcdHome(lcd);
				lcdPuts(lcd,"HISTORICO:      ");
				lcdPosition(lcd,0,1);
				lcdPuts(lcd,"SENSOR DIGITAL  ");
				isPressed(BUTTON_2,increment,&currentMenuOption,3);
				isPressed(BUTTON_1,decrement,&currentMenuOption,1);
				enter(BUTTON_3,historyDigitalSensors);
				break;
			case 2:
				lcdHome(lcd);
				lcdPuts(lcd,"HISTORICO:      ");
				lcdPosition(lcd,0,1);
				lcdPuts(lcd,"SENSOR ANALOGICO");
				isPressed(BUTTON_2,increment,&currentMenuOption,3);
				isPressed(BUTTON_1,decrement,&currentMenuOption,1);
				enter(BUTTON_3,historyAnalogSensors);
				break;
			case 3:
				escreverEmUmaLinha("      SAIR      ");
				isPressed(BUTTON_2,increment,&currentMenuOption,3);
				isPressed(BUTTON_1,decrement,&currentMenuOption,1);
				close(BUTTON_3,&stopLoopMenu);
				break;
		}
	}
	stopLoopMenu = 0;
	currentMenuOption = 1;
	lcdClear(lcd);
}

void updateHistoryAnalog(){
	if(nextHistoryAnalog<10){
		sprintf(historyListAnalog[nextHistoryAnalog].values,"%s",lastAnalogValue);
		strcpy(historyListAnalog[nextHistoryAnalog].time,timeLastValueAnalogSensors);
		nextHistoryAnalog++;
	}else{
		memcpy(historyListAnalog, &historyListAnalog[1], 9*sizeof(*historyListAnalog));
		sprintf(historyListAnalog[nextHistoryAnalog].values,"%s",lastAnalogValue);
		strcpy(historyListAnalog[9].time,timeLastValueAnalogSensors);
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
   updateHistoryDigital(nextHistoryDigital);
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
	sprintf(payload,"%i", currentMenuSensorOption);
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
            sprintf(SituacaoNode, "funcionando")
        } else if(strcmp(payload, "0x1F") == 0) {
            printf("NodeMCU com problema\n");
            sprintf(SituacaoNode, "com problema")
        } else if(strcmp(payload, "0x06") == 0) {
            printf("LED ligado");
        } else if(strcmp(payload, "0x07") == 0) {
            printf("LED apagado");
        } else if(strcmp(payload,"0xFA") == 0){
			printf("Intervalo mudado\n");
        } 
    } else if(strcmp(MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_ANALOGICO, "0X01") == 0) {
        printf("Leitura do sensor analógico: %s", payload);
        strcpy(lastAnalogValue, payload);
        getTime(timeLastValueAnalogSensors);
        updateHistoryAnalog();
    } else if(strcmp(topicName, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_SENSOR_DIGITAL)) {
        printf("LVL Sensor: %s\n", payload);
        char value [16];
        sprintf(value, "%s", payload);
        strcpy(lastDigitalValue, value);
	} else if (strcmp(topicName, MQTT_SUBSCRIBE_TOPIC_ESP_RESPONSE_HISTORY_SENSOR_DIGITAL) == 0) {
        bufDigitalValues = msg;
    	setDigitalValueSensors();
    }
    
 
    /* Faz echo da mensagem recebida */
    publish(client, topicName, payload);//WARNING, WARNING, WARNING -> SE NÃO FUNCIONAR É PQ DO topicName -> REMOVER O PUBLISH.
 
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}
