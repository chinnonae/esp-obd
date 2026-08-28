#include "elm/elm_formatter.h"

#include <cstdio>

namespace esp_obd::elm {

namespace {

constexpr const char* kResponseEndingL0 = "\r\r";
constexpr const char* kResponseEndingL1 = "\r\n\r\n";

void appendHexByte(ElmReplyText& out, uint8_t value) {
  static const char kHexDigits[] = "0123456789ABCDEF";
  out += kHexDigits[(value >> 4) & 0xF];
  out += kHexDigits[value & 0xF];
}

void appendHexId(ElmReplyText& out, uint32_t id, bool extended) {
  char buf[9];
  if (extended) {
    std::snprintf(buf, sizeof(buf), "%08X", static_cast<unsigned>(id));
  } else {
    std::snprintf(buf, sizeof(buf), "%03X", static_cast<unsigned>(id));
  }
  out += buf;
}

void appendBytes(ElmReplyText& out, const uint8_t* bytes, size_t count, const char* sep) {
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      out += sep;
    }
    appendHexByte(out, bytes[i]);
  }
}

}  // namespace

const char* responseEnding(const ElmSession& session) {
  return session.linefeedsEnabled ? kResponseEndingL1 : kResponseEndingL0;
}

ElmReply textReply(const ElmSession& session, const char* body) {
  ElmReply reply;
  reply.kind = ElmReplyKind::Text;
  reply.text = body;
  reply.text += responseEnding(session);
  reply.appendPrompt = true;
  return reply;
}

ElmReplyText formatResponseBody(const ElmSession& session, const can::CanFrame& rawFrame,
                                 const uint8_t* payload, size_t payloadLen,
                                 size_t rawBytesToShow) {
  ElmReplyText out;
  const char* sep = session.spacesEnabled ? " " : "";

  if (session.headersEnabled) {
    appendHexId(out, rawFrame.id, rawFrame.extended);
    if (session.displayDlcEnabled) {
      out += sep;
      out += static_cast<char>('0' + rawFrame.dlc);  // dlc is always 0..8
    }
    out += sep;
    appendBytes(out, rawFrame.data.data(), rawBytesToShow, sep);
  } else {
    appendBytes(out, payload, payloadLen, sep);
  }
  return out;
}

}  // namespace esp_obd::elm
