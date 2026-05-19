# LoRaWAN Energy Monitor with ESP32 Heltec + INA219

## Sobre o Projeto

Este projeto foi desenvolvido utilizando a placa Heltec WiFi LoRa 32 V2 baseada no ESP32, com o objetivo de realizar monitoramento de tensão, corrente, potência e energia elétrica em tempo real utilizando o sensor INA219.

Os dados coletados são processados pelo ESP32, onde a potência instantânea é utilizada para calcular a energia acumulada ao longo do tempo em mWh. Após o processamento, as informações são transmitidas via LoRaWAN utilizando a biblioteca LMIC.

O projeto foi criado para aplicações IoT de baixo consumo e telemetria energética, podendo ser utilizado em:
- monitoramento de sistemas embarcados
- análise de consumo energético
- sistemas solares
- monitoramento remoto
- pesquisa acadêmica
- aplicações LoRaWAN

## Funcionalidades

- Leitura de tensão
- Leitura de corrente
- Cálculo de potência
- Cálculo de energia acumulada
- Transmissão LoRaWAN
- Payload compacto
- Compatível com TTN
- Desenvolvido em PlatformIO

## Hardware Utilizado

- ESP32 Heltec WiFi LoRa 32 V2
- Sensor INA219
- Gateway LoRaWAN
- Fonte de alimentação DC

## Bibliotecas Utilizadas

- MCCI LoRaWAN LMIC
- Adafruit INA219
- Wire
