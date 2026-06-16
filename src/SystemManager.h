#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <Adafruit_INA219.h>
//#include <Adafruit_BMP280.h>
#include <lmic.h>
#include "BatteryManager.h"

// ==========================================
// IDs numéricos dos modos de energia
// Incluídos como último byte do payload LoRaWAN.
// ==========================================
static constexpr uint8_t MODE_ID_ACTIVE      = 0;
static constexpr uint8_t MODE_ID_MODEM_SLEEP = 1;
static constexpr uint8_t MODE_ID_LIGHT_SLEEP = 2;
static constexpr uint8_t MODE_ID_DEEP_SLEEP  = 3;
static constexpr uint8_t MODE_ID_HIBERNATION = 4;

// ==========================================
// Estrutura com todos os dados do ciclo de medição
// ==========================================
struct SensorData {
  // Dados do BMP280 (ambiente)
  //int16_t  temperatura_c100  = 0;    // Temperatura °C * 100  (ex: 2534 → 25.34 °C)
  //uint16_t pressao_hPa10     = 0;    // Pressão hPa * 10      (ex: 10132 → 1013.2 hPa)

  // Dados do INA219 (bateria)
  uint16_t tensao_mV         = 0;    // Tensão da bateria em mV
  int16_t  corrente_x10mA    = 0;    // Corrente mA * 10
  uint16_t potencia_mW       = 0;    // Potência instantânea em mW
  uint32_t energia_mWh       = 0;    // Energia acumulada em mWh

  // Dados do framework de energia
  uint8_t  modo_energia      = 0;    // 0=Active,1=ModemSleep,2=LightSleep,3=DeepSleep,4=Hibernation

};

// ==========================================
// Orquestrador principal do sistema
// ==========================================
class SystemManager {
 public:
  SystemManager(Adafruit_INA219& ina219,
                //Adafruit_BMP280& bmp280,
                BatteryManager&  battery_manager)
      : ina219_(ina219),
        //bmp280_(bmp280),
        battery_manager_(battery_manager) {}

  // Marca o instante inicial para o cálculo de integração de energia.
  void init() {
    ultimo_delta_ms_ = millis();
  }

  // Inicializa o BMP280 no endereço I2C padrão 0x76.
  // Retorna false se o sensor não for encontrado.
  /*bool init_bmp280() {
    return bmp280_.begin(0x76);
  }*/

  // Lê INA219, BMP280 e bateria; preenche a estrutura SensorData completa.
  void read_sensors(SensorData& data) {
    // --- INA219: tensão, corrente, potência e energia acumulada ---
    float bus_voltage_V   = ina219_.getBusVoltage_V();
    float shunt_voltage_V = ina219_.getShuntVoltage_mV() / 1000.0f;
    float load_voltage_V  = bus_voltage_V + shunt_voltage_V;  // Tensão real da bateria (V+)
    float current_mA      = ina219_.getCurrent_mA();
    float power_mW        = ina219_.getPower_mW();

    if (power_mW < 0.0f) {
      power_mW = 0.0f;
    }

    data.tensao_mV      = (uint16_t)roundf(load_voltage_V * 1000.0f);
    data.corrente_x10mA = (int16_t)roundf(current_mA * 10.0f);
    data.potencia_mW    = (uint16_t)roundf(power_mW);

    // Integração de energia: E(mWh) += P(mW) × Δt(h)
    uint32_t agora_ms  = millis();
    uint32_t delta_ms  = agora_ms - ultimo_delta_ms_;
    ultimo_delta_ms_   = agora_ms;
    float    delta_h   = (float)delta_ms / 3600000.0f;
    energia_mWh_acumulada_ += (uint32_t)roundf(power_mW * delta_h);
    data.energia_mWh = energia_mWh_acumulada_;

    // --- BMP280: temperatura e pressão atmosférica ---
    //float temp_C   = bmp280_.readTemperature();
    //float pres_hPa = bmp280_.readPressure() / 100.0f;   // Pa → hPa
    //data.temperatura_c100 = (int16_t)roundf(temp_C   * 100.0f);
    //data.pressao_hPa10    = (uint16_t)roundf(pres_hPa * 10.0f);

  }

  // Monta o payload compacto de 15 bytes e envia via LoRaWAN.
  //
  // Estrutura do payload (big-endian):
  //  [0..1]   temperatura_c100   int16   °C × 100
  //  [2..3]   pressao_hPa10      uint16  hPa × 10
  //  [4..5]   tensao_mV          uint16  mV
  //  [6..7]   corrente_x10mA     int16   mA × 10
  //  [8..9]   potencia_mW        uint16  mW
  //  [10..13] energia_mWh        uint32  mWh
  //  [14]     modo_energia       uint8   0–4
  void send_data(const SensorData& data, uint8_t port) {
    uint8_t payload[11];

    // Temperatura (int16)
    //payload[0]  = (uint8_t)((uint16_t)data.temperatura_c100 >> 8);
    //payload[1]  = (uint8_t)((uint16_t)data.temperatura_c100 & 0xFF);

    // Pressão (uint16)
    //payload[2]  = (uint8_t)(data.pressao_hPa10 >> 8);
   // payload[3]  = (uint8_t)(data.pressao_hPa10 & 0xFF);

    // Tensão da bateria (uint16)
    payload[0]  = (uint8_t)(data.tensao_mV >> 8);
    payload[1]  = (uint8_t)(data.tensao_mV & 0xFF);

    // Corrente da bateria (int16)
    payload[2]  = (uint8_t)((uint16_t)data.corrente_x10mA >> 8);
    payload[3]  = (uint8_t)((uint16_t)data.corrente_x10mA & 0xFF);

    // Potência da bateria (uint16)
    payload[4]  = (uint8_t)(data.potencia_mW >> 8);
    payload[5]  = (uint8_t)(data.potencia_mW & 0xFF);

    // Energia acumulada (uint32)
    payload[6] = (uint8_t)(data.energia_mWh >> 24);
    payload[7] = (uint8_t)((data.energia_mWh >> 16) & 0xFF);
    payload[8] = (uint8_t)((data.energia_mWh >>  8) & 0xFF);
    payload[9] = (uint8_t)(data.energia_mWh & 0xFF);

    // Modo de energia atual (uint8)
    payload[10] = data.modo_energia;

    LMIC_setTxData2(port, payload, sizeof(payload), 0);
  }

  // Seleciona o modo de energia lendo a tensão da bateria diretamente,
  // grava o ID numérico em data.modo_energia e retorna o modo polimórfico.
  PowerMode& update_mode(SensorData& data) {
    PowerMode& mode = battery_manager_.select_mode();

    // Mapeia o nome do modo para o ID numérico do payload.
    const char* nome = mode.name();
    if      (strcmp(nome, "Ativo")       == 0) { data.modo_energia = MODE_ID_ACTIVE;      }
    else if (strcmp(nome, "ModemSleep")  == 0) { data.modo_energia = MODE_ID_MODEM_SLEEP; }
    else if (strcmp(nome, "LightSleep")  == 0) { data.modo_energia = MODE_ID_LIGHT_SLEEP; }
    else if (strcmp(nome, "DeepSleep")   == 0) { data.modo_energia = MODE_ID_DEEP_SLEEP;  }
    else                                        { data.modo_energia = MODE_ID_HIBERNATION; }

    return mode;
  }

 private:
  Adafruit_INA219& ina219_;
  //Adafruit_BMP280& bmp280_;
  BatteryManager&  battery_manager_;
  uint32_t         energia_mWh_acumulada_ = 0;
  uint32_t         ultimo_delta_ms_       = 0;
};

#endif // SYSTEM_MANAGER_H
