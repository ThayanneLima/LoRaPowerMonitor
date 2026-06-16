#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <cstring>
#include <Adafruit_INA219.h>
//#include <Adafruit_BMP280.h>
#include <lmic.h>
#include <hal/hal.h>
#include "PowerMode.h"
#include "BatteryManager.h"
#include "SystemManager.h"
#include "pin_mapping.h"

// ==========================================
// Credenciais LoRaWAN OTAA
// APPEUI e DEVEUI em little-endian; APPKEY em big-endian.
// ==========================================
static const u1_t PROGMEM APPEUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u1_t PROGMEM DEVEUI[8] = {
    0xAA, 0x4D, 0x07, 0xD0, 0x7E, 0xD5, 0xB3, 0x70
};

static const u1_t PROGMEM APPKEY[16] = {
    0xFD, 0xE1, 0x58, 0x5F, 0x53, 0x5E, 0x1D, 0xDC,
    0x79, 0x47, 0x7E, 0xA5, 0xA2, 0x02, 0xBD, 0x0C
};

// ==========================================
// Configurações gerais do ciclo de envio
// ==========================================
static constexpr uint32_t TX_INTERVAL_MS = 60000UL;  // Intervalo de 1 minuto
static constexpr uint8_t  LORAWAN_FPORT  = 1;

// Sensores: INA219 (bateria) e BMP280 (ambiente), ambos no I2C principal.
Adafruit_INA219  ina219(0x40);
//Adafruit_BMP280 bmp280;

// Job da pilha LMIC para agendamento assíncrono de uplinks.
static osjob_t sendjob;

// Instâncias concretas dos modos de energia (herança de PowerMode).
ActiveMode      active_mode;
ModemSleepMode  modem_sleep_mode;
LightSleepMode  light_sleep_mode;
DeepSleepMode   deep_sleep_mode;
HibernationMode hibernation_mode;

// Gerenciador de bateria: lê tensão via INA219 e decide o modo de energia.
BatteryManager battery_manager(
    ina219,
    active_mode,
    modem_sleep_mode,
    light_sleep_mode,
    deep_sleep_mode,
    hibernation_mode);

// Orquestrador do sistema: lê sensores, monta payload e gerencia energia.
SystemManager system_manager(ina219, battery_manager);

// Última medição consolidada, compartilhada entre ciclos.
static SensorData latest_data;

// ==========================================
// Mapeamento de pinos da pilha LoRaWAN LMIC
// ==========================================
const lmic_pinmap lmic_pins = {
    .nss  = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst  = 14,
    .dio  = { 26, 35, 34 },
};

// ==========================================
// Callbacks obrigatórios de credenciais OTAA
// ==========================================
void os_getArtEui(u1_t *buf) { memcpy_P(buf, APPEUI, 8); }
void os_getDevEui(u1_t *buf) { memcpy_P(buf, DEVEUI, 8); }
void os_getDevKey(u1_t *buf) { memcpy_P(buf, APPKEY, 16); }

// ==========================================
// Ciclo principal: lê sensores e envia payload
// de 15 bytes via LoRaWAN
// ==========================================
static void executarCiclo(osjob_t *job) {
  (void)job;

  // Aguarda o join OTAA antes do primeiro envio.
  if ((LMIC.devaddr == 0) || (LMIC.opmode & OP_JOINING)) {
    Serial.println(F("Aguardando join LoRaWAN OTAA antes do envio..."));
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(5), executarCiclo);
    return;
  }

  // Aguarda fim de TX/RX pendente antes de encaminhar novo uplink.
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("LoRaWAN ocupado com TX/RX pendente, reagendando..."));
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(5), executarCiclo);
    return;
  }

  // Lê INA219, BMP280 e bateria via SystemManager.
  system_manager.read_sensors(latest_data);

  // Seleciona o modo de energia antes de enviar, para incluir no payload.
  system_manager.update_mode(latest_data);

  // Envia payload compacto de 15 bytes via LoRaWAN.
  system_manager.send_data(latest_data, LORAWAN_FPORT);

  // Log de diagnóstico no monitor serial.
  //Serial.print(F("Temp(C*100): "));    Serial.print(latest_data.temperatura_c100);
 // Serial.print(F(" Pres(hPa*10): "));  Serial.print(latest_data.pressao_hPa10);
  Serial.print(F(" V(mV): "));         Serial.print(latest_data.tensao_mV);
  Serial.print(F(" I(mA): "));         Serial.print((float)latest_data.corrente_x10mA / 10.0f, 3);
  Serial.print(F(" P(mW): "));         Serial.print(latest_data.potencia_mW);
  Serial.print(F(" E(mWh): "));        Serial.print(latest_data.energia_mWh);
  Serial.print(F(" Bat(V): "));        Serial.print((float)latest_data.tensao_mV / 1000.0f, 2);
  Serial.print(F(" Modo: "));          Serial.println(latest_data.modo_energia);

  // Pequena pausa para estabilidade após enfileirar uplink.
  delay(50);
}

// ==========================================
// Tratamento de eventos da pilha LoRaWAN LMIC
// ==========================================
void onEvent(ev_t ev) {
  switch (ev) {
    case EV_JOINING:
      Serial.println(F("Realizando join LoRaWAN OTAA..."));
      break;

    case EV_JOINED:
      Serial.println(F("Join LoRaWAN OTAA concluido com sucesso."));
      LMIC_setLinkCheckMode(0);
      // Inicia o primeiro ciclo 1 segundo após o join.
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(1), executarCiclo);
      break;

    case EV_JOIN_FAILED:
      Serial.println(F("Falha no join LoRaWAN OTAA. Reiniciando tentativa."));
      LMIC_reset();
      LMIC_startJoining();
      break;

    case EV_TXCOMPLETE: {
      Serial.println(F("Transmissao LoRaWAN concluida."));
      if (LMIC.dataLen) {
        Serial.print(F("Downlink recebido, bytes: "));
        Serial.println(LMIC.dataLen);
      }

      // Seleciona o modo de energia com base na bateria e atualiza latest_data.modo_energia.
      PowerMode& current_mode = system_manager.update_mode(latest_data);
      Serial.print(F("Modo de energia selecionado: "));
      Serial.println(current_mode.name());

      // Entra no modo de energia pelo tempo de 1 minuto (em microssegundos).
      current_mode.enter_mode((uint64_t)TX_INTERVAL_MS * 1000ULL);

      // Ao acordar (Light/Deep/Hibernation) ou após o delay (Active/Modem),
      // agenda o próximo ciclo de medição e envio.
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(1), executarCiclo);
      break;
    }

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("Inicializando Heltec V2 + INA219 + BMP280 + LoRaWAN OTAA + gerenciamento de energia..."));

  // Inicializa o barramento I2C compartilhado por INA219 e BMP280.
  Wire.begin(OLED_SDA, OLED_SCL);
  //Wire.begin(I2C_SDA, I2C_SCL);

  // Inicializa o INA219 (medição de bateria).
  if (!ina219.begin()) {
    Serial.println(F("ERRO: INA219 nao encontrado. Verifique conexoes I2C."));
    while (true) { delay(1000); }
  }
  ina219.setCalibration_16V_400mA();
  Serial.println(F("INA219 inicializado."));

  // Inicializa o BMP280 (temperatura e pressao atmosferica).
  /*if (!system_manager.init_bmp280()) {
    Serial.println(F("ERRO: BMP280 nao encontrado. Verifique conexoes I2C (endereco 0x76)."));
    while (true) { delay(1000); }
  }
  Serial.println(F("BMP280 inicializado.")); */ 

  // Inicializa SPI do rádio LoRa usado pela pilha LoRaWAN.
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  // Inicializa o framework de gerenciamento de energia.
  system_manager.init();

  // Inicializa a pilha LoRaWAN LMIC e configura parâmetros de link.
  os_init();
  LMIC_reset();
  LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
  LMIC_setLinkCheckMode(0);
  LMIC.dn2Dr = DR_SF12CR;
  LMIC_setDrTxpow(DR_SF10, 14);

  // Inicia o processo de join OTAA com a rede LoRaWAN.
  LMIC_startJoining();
}

void loop() {
  // Mantém a pilha LoRaWAN processando eventos de forma cooperativa.
  os_runloop_once();
}
