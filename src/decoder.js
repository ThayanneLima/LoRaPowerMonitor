/**
 * Decoder TTN/TTS para payload compacto do monitor de energia.
 *
 * Estrutura do payload (10 bytes):
 * [0..1] tensao_mV      -> uint16 (big-endian)
 * [2..3] corrente_x10mA -> int16  (big-endian)
 * [4..5] potencia_mW    -> uint16 (big-endian)
 * [6..9] energia_mWh    -> uint32 (big-endian)
 */
function decodeU16BE(bytes, index) {
  return (bytes[index] << 8) | bytes[index + 1];
}

function decodeI16BE(bytes, index) {
  var value = decodeU16BE(bytes, index);
  if (value & 0x8000) {
    value = value - 0x10000;
  }
  return value;
}

function decodeU32BE(bytes, index) {
  return (
    (bytes[index] * 0x1000000) +
    (bytes[index + 1] << 16) +
    (bytes[index + 2] << 8) +
    bytes[index + 3]
  ) >>> 0;
}

/**
 * Função para TTN V3 (The Things Stack):
 * Entrada esperada: { bytes: [...], fPort: number, ... }
 */
function decodeUplink(input) {
  var bytes = input.bytes;

  if (!bytes || bytes.length !== 10) {
    return {
      errors: ["Payload inválido: esperado 10 bytes."]
    };
  }

  var tensao_mV = decodeU16BE(bytes, 0);
  var corrente_x10mA = decodeI16BE(bytes, 2);
  var potencia_mW = decodeU16BE(bytes, 4);
  var energia_mWh = decodeU32BE(bytes, 6);

  return {
    data: {
      tensao_mV: tensao_mV,
      tensao_V: tensao_mV / 1000,
      corrente_mA: corrente_x10mA / 10,
      potencia_mW: potencia_mW,
      potencia_W: potencia_mW / 1000,
      energia_mWh: energia_mWh,
      energia_Wh: energia_mWh / 1000
    }
  };
}