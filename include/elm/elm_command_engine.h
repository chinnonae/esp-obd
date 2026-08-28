#pragma once

#include "elm/elm_parser.h"
#include "elm/elm_reply.h"
#include "elm/elm_session.h"

namespace esp_obd::elm {

// Transforms one complete input line into a structured reply. Owns the
// session and the empty-command repeat state; never touches ICanPort or a
// Stream -- see docs/ARCHITECTURE.md.
class ElmCommandEngine {
 public:
  ElmReply execute(const char* rawLine);

  const ElmSession& session() const { return session_; }
  ElmSession& session() { return session_; }

 private:
  ElmSession session_;
  bool hasLastCommand_ = false;
  NormalizedLine lastCommand_;
};

}  // namespace esp_obd::elm
