# Comunicação e gestão de dados com dispositivos IoT
O objetivo deste projeto foi a implementação de um sistema Iot com o uso do protocolo de comunicação MQTT a fim de entender como integrar diferentes sistemas. Desde da a integração de diferentes sensores e o controle desses em uma NodeMCU,  a qual se comunica com um computador central que possui uma interface local IHM composta por um display LCD, até uma interface remota em forma de aplicativo.

## Equipe de desenvolvimento
- [Lara Esquivel](github.com/laraesquivel)
- [Diego Rocha](github.com/Diego10Rocha)
- [Israel Braitt](github.com/israelbraitt)

## Descrição do problema

O sistema proposto é composto por três módulos: o do ESP8266, a SBC e a interface em aplicativo, todavia o implementado é composto por apenasa dois módulos e um software externo utilizado em testes que foi utilizado como interface que é o MQTT Explorer. Mas de forma geral foi implementado o protocolo de comunicação MQTT entre uma SBC (Single Board Computer) Orange Pi PC Plus e um ESP8266 NodeMCU ESP-12, o protocolo não ocorre entre a SBC e o software externo. Primeiramente não foi implementado na SBC uma comunicação com um outro sistema além do ESP8266 , além disso o software consegue ver qualquer requisição que tenha sido feita por meio do Broker do LEDS.

![diagrama.drawio.png](./resources/diagrama.drawio.png "Diagrama da arquitetura do sistema proposto")

#### Recursos utilizados
- Orange Pi PC Plus
- GPIO Extension Board
- Display LCD Hitachi HD44780U
- Botões
- ESP8266 NodeMCU ESP-12
- Software MQTT Explorer



## Solução


### O protocolo MQTT
O protocolo de comunicação utilizado é o [MQTT](https://mqtt.org/) (Message Queuing Telemetry Transport ou Transporte de Filas de Mensagem de Telemetria), que permite a transmissão e recebimento de mensagens através do modelo Publisher-Subscriber, em que os dispositivos podem publicar ou receber mensagens a depender da sua função no sistema.

As mensagens são enviadas através de tópicos, que funcionam como endereços pelo quais as mensagens serão enviadas. Um dispositivo pode emitir uma mensagem para determinado tópico, sendo chamado de publicador (publisher) ou receber uma mensagem de um tópico, sendo chamado de inscrito (subscriber). Esses tópicos ficam organizados em um servidor denominado broker que é responsável por intermediar as comunicações, tornando possível o recebimento, enfileiramento e envio das mensagens. Os dispositivos que têm capacidade de interagir com o broker são chamados de clientes.

Os pacotes de dados trocados entre os clientes e o broker são denominados mensagens, estes podem possuir um header que possui informações como o tipo de comando e o tamanho do pacote de dados e payload que seria a carag útil ou conteúdo da mensagem.

### Comunicação entre dispositivos
O SBC é responsável por iniciar as comunicações, enviando solicitações para o NodeMCU, requerindo os dados dos sensores analógicos e digitais. O NodeMCU deve retornar mensagens indicando os estados dos sensores e até mesmo da placa. A tabela abaixo indica as mensagens utilizadas para comunicação entre os dois dispositivos.

![Tabelas de comandos](https://github.com/israelbraitt/comunicacao-mqtt-dispositivos-iot/blob/main/resources/Tabelas%20de%20comandos.png)

Cada mensagem descrita na tabela acima representa uma situação diferente que pode ocorrer na interação entre os dispositivos, sendo que ambos devem identificar as suas mensagens de requisição e as mensagens de resposta do outro dispositivo.

A comunicação ocorre da seguinte forma: a Orange Pi faz uma requisição a um NodeMCU, através de uma mensagem enviada no tópico SBCESP (SBC to ESP), o NodeMCU responde com os dados requisitados através do tópico ESPSBC (ESP to SBC). Quando os dados chegam na Orange Pi ele é adicionado ao histórico dos dados, podendo ser apresentado no display e também retransmitido para a interface remota através do tópico SBCSW (SBC to Software). O modelo da comunicação é representado no diagrama abaixo.

![Diagrama da arquitetura da comunicação](https://github.com/israelbraitt/comunicacao-mqtt-dispositivos-iot/blob/main/resources/Diagrama%20da%20arquitetura%20de%20comunica%C3%A7%C3%A3o.png)

### Controlando a Orange Pi PC Plus
O código do arquivo [main.c](https://github.com/israelbraitt/comunicacao-mqtt-dispositivos-iot/blob/main/main.c) é responsável por controlar a Orange Pi, implementando a lógica de comunicação do protocolo MQTT nessa placa e controlando os periféricos conectados na mesma.

Para fazer o mapeamento dos pinos da Orange Pi foi utilizada a bilbioteca **WiringPi**, possibilitando a conexão da mesma com o display LCD e com os botões (que representa a interface local do usuário).

Através da interface local do usuário é possível configurar o intervalo e a unidade de tempo em que serão solicitados os dados dos sensores, assim como ver a medição mais recente dos sensores.

Para configurar o protocolo MQTT foi utilizada a bilbioteca **Paho MQTT C Client** (ver Materiais de Referência) responsável por fazer a conexão da placa como um cliente no broker, se inscrever e publicar nos devidos tópicos.

Os parâmetros _username_, _password_, _broker_address_ e _client_id_ estão configurados com os dados usados nos testes realizados durante o desenvolvimento em laboratório, mas podem ser alterados com os dados necesários de alguma outra aplicação.

### Controlando a ESP8266 NodeMCU ESP-12
O código do arquivo [nodemcu.ino](https://github.com/israelbraitt/comunicacao-mqtt-dispositivos-iot/blob/main/nodemcu.ino) é responsável por controlar o NodeMCU, implementando a lógica de comunicação do protocolo MQTT nessa placa e controlando os sensores analógico e digitais conectados na mesma.

Nesse arquivo são utilizadas as bibliotecas **ESP8266Wifi**, **ESP8266mDNS**, **WifiUdp** e **ArduinoOTA** (ver Materiais de Referência) para efetuar a conexão sem fio com a NodeMCU, definindo as configurações iniciais da placa e possibilitando o envio de códigos por conexão sem fio. A configuração dos pinos é efetuada com linguagem Arduino.

Através da biblioteca **PubSubClient** (ver Materiais de Referência) é possível configurar o protocolo MQTT, conectando a placa como um cliente no broker, fazendo com que seja possível se incriver e publicar nos devidos tópicos.

Os parâmetros _ssid_, _password_, _mqtt_server_ e _device_id_ estão configurados com os dados usados nos testes realizados durante o desenvolvimento em laboratório, mas podem ser alterados com os dados necesários de alguma outra aplicação.

### Interface remota
Como interface remota foi utilizado o software [MQTT Explorer](https://mqtt-explorer.com/) desenvolvido por [Thomas Nordquist](https://github.com/thomasnordquist) que é um cliente MQTT que fornece uma visão estruturada dos tópicos MQTT. Com ele é possível: visualizar tópicos e suas atividades, gerenciar e publicar nos tópicos, além de ser possível visualizar um histórico das insformações publicadas.

## Como executar o projeto
Realize o download dos arquivos sbc_orangepi.c, nodemcu.ino e makefile.

Transfira o arquivo sbc_orangepi.c e makefile para a Orange Pi PC Plus.
Execute o comando a seguir para compilar o código:
```
$ make
```
Em seguida execute o comando a seguir para executar o programa:
```
$ sudo ./main
```

Utilize o software [Arduino IDE](https://www.arduino.cc/en/software) para executar o arquivo nodemcu.ino, se certificando que a placa esteja configurada para receber códigos via OTA através do Wi-Fi.

Para visualizar as informações dos tópicos através do software MQTT Explorer, acesse o site oficial e baixe a versão correspondente ao seu sistema operacional.

## Materiais de referência
[Orange Pi PC Plus Documentation](http://www.orangepi.org/html/hardWare/computerAndMicrocontrollers/service-and-support/Orange-Pi-Pc-Plus.html)

[Display LCD HD44780U](https://www.google.com/url?sa=t&source=web&rct=j&url=https://www.sparkfun.com/datasheets/LCD/HD44780.pdf&ved=2ahUKEwjso46tlqn6AhVGL7kGHSe6BMEQFnoECGIQAQ&usg=AOvVaw076YT-P88DM3oFFvTDUv43)

[WiringPi Reference](http://wiringpi.com/reference/)

[WiringiPi LCD Library (HD44780U)](http://wiringpi.com/dev-lib/lcd-library/)

[Paho MQTT C Client Library](https://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/index.html)

[ESP8266 Arduino Core Documentation](https://readthedocs.org/projects/arduino-esp8266/downloads/pdf/latest/)

[Documentação de Referência da Linguagem Arduino](https://www.arduino.cc/reference/pt/)

[ESP8266WiFi library](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html)

[ESP8266mDNS library](https://www.arduino.cc/reference/en/libraries/esp8266_mdns/)

[WifiUDP library](https://www.arduino.cc/reference/en/libraries/wifi/wifiudp/)

[ArduinoOTA](https://www.arduino.cc/reference/en/libraries/arduinoota/)

[PubSubClient library](https://www.arduino.cc/reference/en/libraries/pubsubclient/)

[Wire library](https://github.com/esp8266/Arduino/blob/master/libraries/Wire/Wire.h)

[MQTT Explorer](https://mqtt-explorer.com/)
