//Versão Thay
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <cstring>
#include <Adafruit_INA219.h>
#include <lmic.h>
#include <hal/hal.h>
#include "pin_mapping.h"

static const u1_t PROGMEM APPEUI[8] = {                 //lsb
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u1_t PROGMEM DEVEUI[8] = {                 //lsb 
    0xAA, 0x4D, 0x07, 0xD0, 0x7E, 0xD5, 0xB3, 0x70
};

static const u1_t PROGMEM APPKEY[16] = {                 //msb
    0xFD, 0xE1, 0x58, 0x5F, 0x53, 0x5E, 0x1D, 0xDC, 
    0x79, 0x47, 0x7E, 0xA5, 0xA2, 0x02, 0xBD, 0x0C
};

// ==========================================
// Configurações de envio
// ==========================================
static constexpr uint32_t TX_INTERVAL_MS = 60000UL;
static constexpr uint8_t LORAWAN_FPORT = 1;

// Objeto do sensor INA219.
Adafruit_INA219 ina219;

// Job do LMIC para agendamento de envio.
static osjob_t sendjob;

// Variáveis de acumulação de energia.
static uint32_t energia_mWh_acumulada = 0;
static uint32_t ultimo_delta_ms = 0;

// ==========================================
// Mapeamento de pinos LMIC para Heltec V2
// ==========================================
const lmic_pinmap lmic_pins = {
    .nss = LORA_NSS,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = LORA_RST,
    .dio = {LORA_DIO0, LORA_DIO1, LORA_DIO2},
    .rxtx_rx_active = 0,
    .rssi_cal = 0,
    .spi_freq = 8000000
};

// ==========================================
// Funções de callback exigidas pela LMIC
// ==========================================
void os_getArtEui(u1_t *buf) { memcpy_P(buf, APPEUI, 8); }
void os_getDevEui(u1_t *buf) { memcpy_P(buf, DEVEUI, 8); }
void os_getDevKey(u1_t *buf) { memcpy_P(buf, APPKEY, 16); }

// ==========================================
// Lê INA219 e calcula energia acumulada
// ==========================================
static void lerSensorEAtualizarEnergia(
    uint16_t &tensao_mV,
    int16_t &corrente_x10mA,
    uint16_t &potencia_mW,
    uint32_t &energia_mWh) {

  // Leitura da tensão no barramento em mV.
  float busVoltage_V = ina219.getBusVoltage_V();
  tensao_mV = (uint16_t)roundf(busVoltage_V * 1000.0f);

  // Leitura da corrente em mA.
  float current_mA = ina219.getCurrent_mA();
  corrente_x10mA = (int16_t)roundf(current_mA * 10.0f);

  // Potência instantânea em mW.
  float power_mW_float = ina219.getPower_mW();
  if (power_mW_float < 0) {
    power_mW_float = 0;
  }
  potencia_mW = (uint16_t)roundf(power_mW_float);

  // Integração simples da energia em mWh.
  uint32_t agora_ms = millis();
  uint32_t delta_ms = (ultimo_delta_ms == 0) ? 0 : (agora_ms - ultimo_delta_ms);
  ultimo_delta_ms = agora_ms;

  float delta_h = (float)delta_ms / 3600000.0f;
  float incremento_mWh = power_mW_float * delta_h;
  energia_mWh_acumulada += (uint32_t)roundf(incremento_mWh);
  energia_mWh = energia_mWh_acumulada;
}

// ==========================================
// Monta payload compacto e agenda envio LoRaWAN
// Payload:
// [0..1] tensão mV (uint16)
// [2..3] corrente mA*10 (int16)
// [4..5] potência mW (uint16)
// [6..9] energia mWh (uint32)
// ==========================================
static void enviarPayload(osjob_t *j) {
  (void)j;

  // Aguarda a conclusão do join OTAA antes do envio.
  if ((LMIC.devaddr == 0) || (LMIC.opmode & OP_JOINING)) {
    Serial.println(F("Aguardando join OTAA antes do envio..."));
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(5), enviarPayload);
    return;
  }

  // Se já existe transmissão em andamento, apenas reagenda.
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("LMIC ocupado (TX/RX pendente), reagendando..."));
    os_setTimedCallback(&sendjob, os_getTime() + ms2osticks(TX_INTERVAL_MS), enviarPayload);
    return;
  }

  uint16_t tensao_mV = 0;
  int16_t corrente_x10mA = 0;
  uint16_t potencia_mW = 0;
  uint32_t energia_mWh = 0;

  lerSensorEAtualizarEnergia(tensao_mV, corrente_x10mA, potencia_mW, energia_mWh);

  uint8_t payload[10];
  payload[0] = (uint8_t)(tensao_mV >> 8);
  payload[1] = (uint8_t)(tensao_mV & 0xFF);
  payload[2] = (uint8_t)((uint16_t)corrente_x10mA >> 8);
  payload[3] = (uint8_t)((uint16_t)corrente_x10mA & 0xFF);
  payload[4] = (uint8_t)(potencia_mW >> 8);
  payload[5] = (uint8_t)(potencia_mW & 0xFF);
  payload[6] = (uint8_t)(energia_mWh >> 24);
  payload[7] = (uint8_t)((energia_mWh >> 16) & 0xFF);
  payload[8] = (uint8_t)((energia_mWh >> 8) & 0xFF);
  payload[9] = (uint8_t)(energia_mWh & 0xFF);

  LMIC_setTxData2(LORAWAN_FPORT, payload, sizeof(payload), 0);

  Serial.print(F("Enviando -> V(mV): "));
  Serial.print(tensao_mV);
  Serial.print(F(" I(mA*10): "));
  Serial.print(corrente_x10mA);
  Serial.print(F(" P(mW): "));
  Serial.print(potencia_mW);
  Serial.print(F(" E(mWh): "));
  Serial.println(energia_mWh);

  // Delay curto para estabilidade entre envios/agendamentos.
  delay(50);
}

// ==========================================
// Callback de eventos LMIC
// ==========================================
void onEvent(ev_t ev) {
  switch (ev) {
    case EV_JOINING:
      Serial.println(F("Realizando join OTAA..."));
      break;

    case EV_JOINED:
      Serial.println(F("Join OTAA concluido com sucesso."));
      LMIC_setLinkCheckMode(0);
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(1), enviarPayload);
      break;

    case EV_JOIN_FAILED:
      Serial.println(F("Falha no join OTAA. Nova tentativa em 30 segundos."));
      LMIC_reset();
      LMIC_startJoining();
      break;

    case EV_TXCOMPLETE:
      Serial.println(F("TX concluido."));
      if (LMIC.dataLen) {
        Serial.print(F("Downlink recebido, bytes: "));
        Serial.println(LMIC.dataLen);
      }
      os_setTimedCallback(&sendjob, os_getTime() + ms2osticks(TX_INTERVAL_MS), enviarPayload);
      break;

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("Inicializando Heltec V2 + INA219 + LoRaWAN OTAA..."));

  // Inicializa I2C do INA219.
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ina219.begin()) {
    Serial.println(F("Falha ao encontrar INA219. Verifique conexoes I2C."));
    while (true) {
      delay(1000);
    }
  }
  Serial.println(F("INA219 inicializado."));

  // Inicializa SPI para o rádio LoRa usado pela pilha LoRaWAN.
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  // Inicializa pilha LMIC.
  os_init();
  LMIC_reset();

  // Define canais EU868 por padrão (TTN V3).
  // Ajuste esta parte para sua região (US915, AU915, AS923, etc).
  LMIC_disableChannel(3);
  LMIC_disableChannel(4);
  LMIC_disableChannel(5);
  LMIC_disableChannel(6);
  LMIC_disableChannel(7);
  LMIC_disableChannel(8);

  // Parâmetros de link.
  LMIC_setLinkCheckMode(0);
  LMIC.dn2Dr = DR_SF9;
  LMIC_setDrTxpow(DR_SF7, 14);

  // Inicia o processo de join OTAA.
  LMIC_startJoining();
}

void loop() {
  // Loop de execução da pilha LMIC.
  os_runloop_once();
}