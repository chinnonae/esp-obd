#include "elm/at_commands_settings.h"

#include <cstring>

#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"

namespace esp_obd::elm {

namespace {

bool isHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }
uint8_t hexValue(char c) { return c <= '9' ? static_cast<uint8_t>(c - '0')
                                            : static_cast<uint8_t>(c - 'A' + 10); }

bool parseTwoHexDigits(const char* text, uint8_t* outValue) {
  if (!isHexDigit(text[0]) || !isHexDigit(text[1]) || text[2] != '\0') {
    return false;
  }
  *outValue = static_cast<uint8_t>((hexValue(text[0]) << 4) | hexValue(text[1]));
  return true;
}

}  // namespace

std::optional<ElmReply> dispatchSettingsCommand(const ElmSession& session,
                                                 PersistedSettingsCache& cache,
                                                 core::ISettingsStore& store, const char* r) {
  if (std::strcmp(r, "@2") == 0) {
    return textReply(session, cache.deviceId.data());
  }

  if (std::strncmp(r, "@3", 2) == 0) {
    const char* digits = r + 2;
    if (std::strlen(digits) != kDeviceIdHexDigits) {
      return std::nullopt;
    }
    for (size_t i = 0; i < kDeviceIdHexDigits; ++i) {
      if (!isHexDigit(digits[i])) {
        return std::nullopt;
      }
    }
    std::memcpy(cache.deviceId.data(), digits, kDeviceIdHexDigits);
    cache.deviceId[kDeviceIdHexDigits] = '\0';
    if (cache.persistenceEnabled) {
      store.setDeviceId(cache.deviceId.data());
    }
    return textReply(session, kOkText);
  }

  if (std::strcmp(r, "M0") == 0) {
    cache.persistenceEnabled = false;
    return textReply(session, kOkText);
  }
  if (std::strcmp(r, "M1") == 0) {
    cache.persistenceEnabled = true;
    return textReply(session, kOkText);
  }

  if (std::strcmp(r, "FE") == 0) {
    cache.persistenceEnabled = true;
    store.eraseAll();
    cache.loadFrom(store);
    return textReply(session, kOkText);
  }

  if (std::strcmp(r, "RD") == 0) {
    ElmReplyText body;
    appendHexByte(body, cache.savedDataByte);
    return textReply(session, body.c_str());
  }

  if (std::strncmp(r, "SD", 2) == 0) {
    uint8_t value = 0;
    if (!parseTwoHexDigits(r + 2, &value)) {
      return std::nullopt;
    }
    cache.savedDataByte = value;
    if (cache.persistenceEnabled) {
      store.setSavedDataByte(value);
    }
    return textReply(session, kOkText);
  }

  return std::nullopt;
}

}  // namespace esp_obd::elm
