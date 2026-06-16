#ifndef POWER_MODE_H
#define POWER_MODE_H

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <driver/adc.h>

// ==========================================
// Classe base abstrata para modos de energia
// ==========================================
class PowerMode {
 public:
  virtual ~PowerMode() = default;
  virtual void enter_mode(uint64_t sleep_time_us) = 0;
  virtual const char* name() const = 0;
};

// ==========================================
// Modo ativo: mantém CPU e periféricos acordados
// ==========================================
class ActiveMode : public PowerMode {
 public:
  void enter_mode(uint64_t sleep_time_us) override {
    (void)sleep_time_us;
    delay(50);
  }

  const char* name() const override {
    return "Ativo";
  }
};

// ==========================================
// Modem sleep: reduz consumo desligando Wi-Fi/Bluetooth
// ==========================================
class ModemSleepMode : public PowerMode {
 public:
  void enter_mode(uint64_t sleep_time_us) override {
    esp_wifi_stop();
    delay((uint32_t)(sleep_time_us / 1000ULL));
  }

  const char* name() const override {
    return "ModemSleep";
  }
};

// ==========================================
// Light sleep: mantém RAM e acorda por temporizador
// ==========================================
class LightSleepMode : public PowerMode {
 public:
  void enter_mode(uint64_t sleep_time_us) override {
    esp_sleep_enable_timer_wakeup(sleep_time_us);
    esp_light_sleep_start();
  }

  const char* name() const override {
    return "LightSleep";
  }
};

// ==========================================
// Deep sleep: reinicializa o chip ao acordar
// ==========================================
class DeepSleepMode : public PowerMode {
 public:
  void enter_mode(uint64_t sleep_time_us) override {
    esp_sleep_enable_timer_wakeup(sleep_time_us);
    esp_deep_sleep_start();
  }

  const char* name() const override {
    return "DeepSleep";
  }
};

// ==========================================
// Hibernação: desligamento mais agressivo possível
// ==========================================
class HibernationMode : public PowerMode {
 public:
  void enter_mode(uint64_t sleep_time_us) override {
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_enable_timer_wakeup(sleep_time_us);
    esp_deep_sleep_start();
  }

  const char* name() const override {
    return "Hibernacao";
  }
};

#endif // POWER_MODE_H