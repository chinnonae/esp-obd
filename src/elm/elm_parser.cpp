#include "elm/elm_parser.h"

#include <cctype>
#include <cstring>

namespace esp_obd::elm {

namespace {

bool isHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }

uint8_t hexDigitValue(char c) { return c <= '9' ? (c - '0') : (c - 'A' + 10); }

}  // namespace

NormalizedLine normalizeLine(const char* rawLine) {
  NormalizedLine out;
  for (const char* p = rawLine; *p != '\0'; ++p) {
    if (*p == ' ') {
      continue;
    }
    out += static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
  }
  return out;
}

HexRequestValidation validateHexRequestLine(const NormalizedLine& line,
                                             bool automaticFormattingEnabled) {
  HexRequestValidation result;
  const char* text = line.c_str();
  size_t len = line.size();

  size_t payloadLen = len;
  std::optional<uint8_t> maxResponses;

  if (len % 2 != 0) {
    char lastChar = text[len - 1];
    if (lastChar == '0' || !isHexDigit(lastChar)) {
      return result;  // odd length ending in '0', or a non-hex trailing char
    }
    payloadLen = len - 1;
    if (payloadLen == 0) {
      return result;  // no payload before the response-count nibble
    }
    maxResponses = hexDigitValue(lastChar);
  }

  for (size_t i = 0; i < payloadLen; ++i) {
    if (!isHexDigit(text[i])) {
      return result;  // invalid hex
    }
  }

  size_t payloadBytes = payloadLen / 2;
  size_t maxPayloadBytes = automaticFormattingEnabled ? 4095 : 8;  // CAF1 vs CAF0
  if (payloadBytes > maxPayloadBytes) {
    return result;  // oversized
  }

  result.valid = true;
  result.payloadHex.assign(text);
  // Truncate to just the payload portion if a response-count nibble was
  // stripped; NormalizedLine has no substring op, so rebuild it.
  if (payloadLen != len) {
    NormalizedLine payloadOnly;
    for (size_t i = 0; i < payloadLen; ++i) {
      payloadOnly += text[i];
    }
    result.payloadHex = payloadOnly;
  }
  result.maxResponses = maxResponses;
  return result;
}

size_t decodeHexBytes(const char* hexDigits, uint8_t* out, size_t maxOut) {
  size_t written = 0;
  for (; hexDigits[0] != '\0' && hexDigits[1] != '\0' && written < maxOut; hexDigits += 2) {
    if (!isHexDigit(hexDigits[0]) || !isHexDigit(hexDigits[1])) {
      break;
    }
    out[written++] =
        static_cast<uint8_t>((hexDigitValue(hexDigits[0]) << 4) | hexDigitValue(hexDigits[1]));
  }
  return written;
}

}  // namespace esp_obd::elm
