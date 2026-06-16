/**
 * Decoder TTN/TTS para payload compacto do monitor de energia LoRaWAN.
 *
 * Estrutura do payload (15 bytes, big-endian):
 *  [0..1]   temperatura_c100   int16   Temperatura °C × 100  (ex: 2534 → 25.34 °C)
 *  [2..3]   pressao_hPa10      uint16  Pressão hPa × 10      (ex: 10132 → 1013.2 hPa)
 *  [4..5]   tensao_mV          uint16  Tensão da bateria em mV
 *  [6..7]   corrente_x10mA     int16   Corrente mA × 10      (ex: 153 → 15.3 mA)
 *  [8..9]   potencia_mW        uint16  Potência instantânea em mW
 *  [10..13] energia_mWh        uint32  Energia acumulada em mWh
 *  [14]     modo_energia       uint8   0=Active,1=ModemSleep,2=LightSleep,3=DeepSleep,4=Hibernation
 */

// Lê 2 bytes como uint16 big-endian.
function decodeU16BE(bytes, index) {
  return (bytes[index] << 8) | bytes[index + 1];
}

// Lê 2 bytes como int16 big-endian (com sinal).
function decodeI16BE(bytes, index) {
  var value = decodeU16BE(bytes, index);
  if (value & 0x8000) {
    value = value - 0x10000;
  }
  return value;
}

// Lê 4 bytes como uint32 big-endian.
function decodeU32BE(bytes, index) {
  return (
    (bytes[index] * 0x1000000) +
    (bytes[index + 1] << 16) +
    (bytes[index + 2] << 8) +
    bytes[index + 3]
  ) >>> 0;
}

// Retorna o nome legível do modo de energia a partir do ID numérico.
function nomeModoEnergia(id) {
  var modos = ["Active", "ModemSleep", "LightSleep", "DeepSleep", "Hibernation"];
  return (id >= 0 && id < modos.length) ? modos[id] : "Desconhecido";
}

/**
 * Função principal para TTN V3 (The Things Stack).
 * Entrada esperada: { bytes: [...], fPort: number }
 */
function decodeUplink(input) {
  var bytes = input.bytes;

  if (!bytes || bytes.length !== 15) {
    return {
      errors: ["Payload invalido: esperado 15 bytes, recebido " + (bytes ? bytes.length : 0) + "."]
    };
  }

  // Decodificação de cada campo conforme estrutura do payload.
  var temperatura_c100  = decodeI16BE(bytes, 0);
  var pressao_hPa10     = decodeU16BE(bytes, 2);
  var tensao_mV         = decodeU16BE(bytes, 4);
  var corrente_x10mA    = decodeI16BE(bytes, 6);
  var potencia_mW       = decodeU16BE(bytes, 8);
  var energia_mWh       = decodeU32BE(bytes, 10);
  var modo_energia_id   = bytes[14];

  return {
    data: {
      // Dados de ambiente (BMP280)
      temperatura_C:    temperatura_c100 / 100,
      pressao_hPa:      pressao_hPa10   / 10,

      // Dados da bateria (INA219)
      tensao_bateria_mV:  tensao_mV,
      tensao_bateria_V:   tensao_mV / 1000,
      corrente_bateria_mA: corrente_x10mA / 10,
      potencia_bateria_mW: potencia_mW,
      potencia_bateria_W:  potencia_mW / 1000,
      energia_acumulada_mWh: energia_mWh,
      energia_acumulada_Wh:  energia_mWh / 1000,

      // Modo de energia do framework
      modo_energia_id:   modo_energia_id,
      modo_energia_nome: nomeModoEnergia(modo_energia_id)
    }
  };
}
