#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_INA219.h>
#include "PowerMode.h"

// ==========================================
// Gerenciador de bateria via INA219
//
// Topologia do hardware:
//   Bateria Li-Ion 3.7V nominal
//     → INA219 em série (mede tensão, corrente e potência)
//     → Regulador 3.7V → 3.3V
//     → Heltec WiFi LoRa 32 V2
//
// A tensão real da bateria é calculada pelo INA219 como:
//   tensão_real = getBusVoltage_V() + (getShuntVoltage_mV() / 1000.0f)
// Isso recompõe a tensão total antes do resistor shunt.
// ==========================================
class BatteryManager {
 public:
  // ------------------------------------------
  // Limiares de tensão para seleção do modo de energia
  // Baseados na faixa operacional de uma Li-Ion 3.7V nominal:
  // ------------------------------------------
  // Acima de 3.60V → ActiveMode       (bateria plena, operação normal)
  static constexpr float THRESHOLD_ACTIVE = 3.60f;
  // Entre 3.50V e 3.59V → ModemSleepMode  (bateria boa, economia leve)
  static constexpr float THRESHOLD_MODEM  = 3.50f;
  // Entre 3.40V e 3.49V → LightSleepMode  (bateria média, economia moderada)
  static constexpr float THRESHOLD_LIGHT  = 3.40f;
  // Entre 3.30V e 3.39V → DeepSleepMode   (bateria baixa, economia agressiva)
  static constexpr float THRESHOLD_DEEP   = 3.30f;
  // Abaixo de 3.30V    → HibernationMode  (bateria crítica, proteção máxima)

  // O INA219 é o único sensor de tensão da bateria.
  // Os modos de energia são passados por referência para uso polimórfico.
  BatteryManager(Adafruit_INA219& ina219,
                 PowerMode& active_mode,
                 PowerMode& modem_sleep_mode,
                 PowerMode& light_sleep_mode,
                 PowerMode& deep_sleep_mode,
                 PowerMode& hibernation_mode)
      : ina219_(ina219),
        active_mode_(active_mode),
        modem_sleep_mode_(modem_sleep_mode),
        light_sleep_mode_(light_sleep_mode),
        deep_sleep_mode_(deep_sleep_mode),
        hibernation_mode_(hibernation_mode) {}

  // Retorna a tensão real atual da bateria em Volts.
  // O valor é calculado como tensão do barramento + queda no shunt.
  float get_battery_voltage() const {
    float bus_voltage_V = ina219_.getBusVoltage_V();
    float shunt_voltage_V = ina219_.getShuntVoltage_mV() / 1000.0f;
    return bus_voltage_V + shunt_voltage_V;
  }

  // Seleciona o modo de energia comparando a tensão da bateria
  // com os limiares definidos acima.
  //   > 3.60V  → Ativo        (plena carga)
  //   > 3.50V  → Modem Sleep  (carga boa)
  //   > 3.40V  → Light Sleep  (carga média)
  //   > 3.30V  → Deep Sleep   (carga baixa)
  //  <= 3.30V  → Hibernação   (tensão crítica, protege a bateria)
  PowerMode& select_mode() {
    float bus_voltage_V = ina219_.getBusVoltage_V();
    float shunt_voltage_V = ina219_.getShuntVoltage_mV() / 1000.0f;
    float v = bus_voltage_V + shunt_voltage_V;

    if (v >= THRESHOLD_ACTIVE) {
      return active_mode_;
    }
    if (v >= THRESHOLD_MODEM) {
      return modem_sleep_mode_;
    }
    if (v >= THRESHOLD_LIGHT) {
      return light_sleep_mode_;
    }
    if (v >= THRESHOLD_DEEP) {
      return deep_sleep_mode_;
    }
    // Tensão crítica: hiberna para proteger a bateria contra descarga profunda.
    return hibernation_mode_;
  }

 private:
  Adafruit_INA219& ina219_;         // Referência ao sensor INA219
  PowerMode&       active_mode_;
  PowerMode&       modem_sleep_mode_;
  PowerMode&       light_sleep_mode_;
  PowerMode&       deep_sleep_mode_;
  PowerMode&       hibernation_mode_;
};

#endif // BATTERY_MANAGER_H
