#ifndef PIN_MAPPING_H
#define PIN_MAPPING_H

// ============================
// Mapeamento de pinos Heltec WiFi LoRa 32 V2
// ============================

// LoRa (SX1276/78) via SPI
static constexpr uint8_t LORA_SCK  = 5;
static constexpr uint8_t LORA_MISO = 19;
static constexpr uint8_t LORA_MOSI = 27;
static constexpr uint8_t LORA_NSS  = 18;
static constexpr uint8_t LORA_RST  = 14;
static constexpr uint8_t LORA_DIO0 = 26;
static constexpr uint8_t LORA_DIO1 = 35;
static constexpr uint8_t LORA_DIO2 = 34;

// I2C principal (INA219)
static constexpr uint8_t I2C_SDA = 21;
static constexpr uint8_t I2C_SCL = 22;

// OLED onboard (não utilizado neste exemplo, somente referência)
static constexpr uint8_t OLED_SDA = 4;
static constexpr uint8_t OLED_SCL = 15;
static constexpr uint8_t OLED_RST = 16;

#endif // PIN_MAPPING_H