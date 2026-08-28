#include "elm/elm_command_engine.h"

#include "elm/at_commands_addressing.h"
#include "elm/at_commands_core.h"
#include "elm/at_commands_monitoring.h"
#include "elm/at_commands_protocol.h"
#include "elm/at_commands_settings.h"
#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"

namespace esp_obd::elm {

ElmReply ElmCommandEngine::execute(const char* rawLine) {
  NormalizedLine normalized = normalizeLine(rawLine);

  // Empty command: repeat the most recently *recognized* non-empty command
  // (docs/ELM_COMMAND_BEHAVIOR.md section 1.1). Before any such command,
  // produce no response at all.
  if (normalized.empty()) {
    if (!hasLastCommand_) {
      ElmReply reply;
      reply.kind = ElmReplyKind::NoReply;
      reply.appendPrompt = false;
      return reply;
    }
    normalized = lastCommand_;
  }

  ElmReply reply;
  bool recognized = false;

  if (startsWithAt(normalized)) {
    const char* atRemainder = normalized.c_str() + 2;
    auto handled = dispatchCoreCommand(session_, atRemainder);
    if (!handled.has_value()) {
      handled = dispatchSettingsCommand(session_, persisted_, store_, atRemainder);
    }
    if (!handled.has_value()) {
      handled = dispatchProtocolCommand(session_, atRemainder);
    }
    if (!handled.has_value()) {
      handled = dispatchAddressingCommand(session_, atRemainder);
    }
    if (!handled.has_value()) {
      handled = dispatchMonitoringCommand(session_, atRemainder);
    }
    if (handled.has_value()) {
      reply = *handled;
      recognized = true;
    } else {
      reply = textReply(session_, kUnknownCommandText);
    }
  } else {
    HexRequestValidation validated =
        validateHexRequestLine(normalized, session_.automaticFormattingEnabled);
    if (validated.valid) {
      reply.kind = ElmReplyKind::DiagnosticRequest;
      reply.text = validated.payloadHex;
      reply.diagnosticMaxResponses = validated.maxResponses;
      reply.appendPrompt = false;  // T06 replies once the transaction completes
      recognized = true;
    } else {
      reply = textReply(session_, kUnknownCommandText);
    }
  }

  if (recognized) {
    lastCommand_ = normalized;
    hasLastCommand_ = true;
  }

  return reply;
}

}  // namespace esp_obd::elm
