#pragma once

#include "core/i_settings_store.h"
#include "elm/elm_parser.h"
#include "elm/elm_reply.h"
#include "elm/elm_session.h"
#include "elm/persisted_settings.h"

namespace esp_obd::elm {

// Transforms one complete input line into a structured reply. Owns the
// session and the empty-command repeat state; never touches ICanPort or a
// Stream -- see docs/ARCHITECTURE.md.
//
// `store` must outlive this engine. Persisted settings (AT@2/AT@3/ATM/
// ATFE/ATRD/ATSD; see T11) are loaded from it once, at construction --
// ATZ/ATD/ATWS reset the session but deliberately never touch them.
class ElmCommandEngine {
 public:
  explicit ElmCommandEngine(core::ISettingsStore& store) : store_(store) {
    persisted_.loadFrom(store_);
  }

  ElmReply execute(const char* rawLine);

  const ElmSession& session() const { return session_; }
  ElmSession& session() { return session_; }

  const PersistedSettingsCache& persistedSettings() const { return persisted_; }

 private:
  ElmSession session_;
  core::ISettingsStore& store_;
  PersistedSettingsCache persisted_;
  bool hasLastCommand_ = false;
  NormalizedLine lastCommand_;
};

}  // namespace esp_obd::elm
