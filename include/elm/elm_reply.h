#pragma once

#include <cstdint>
#include <optional>

#include "core/fixed_string.h"

namespace esp_obd::elm {

inline constexpr size_t kElmReplyTextCapacity = 256;
using ElmReplyText = FixedString<kElmReplyTextCapacity>;

enum class ElmReplyKind {
  Text,               // text is a complete response body + ending
  NoReply,            // empty command with no prior command to repeat
  DiagnosticRequest,  // valid hex request; text holds the normalized
                      // payload hex (PCI/response-count nibble already
                      // stripped); no CAN transmitted yet -- T06 executes it
};

struct ElmReply {
  ElmReplyKind kind = ElmReplyKind::Text;
  ElmReplyText text;
  bool appendPrompt = true;
  std::optional<uint8_t> diagnosticMaxResponses;  // only for DiagnosticRequest
};

}  // namespace esp_obd::elm
