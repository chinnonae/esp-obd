#pragma once

#include <optional>

#include "core/i_settings_store.h"
#include "elm/elm_reply.h"
#include "elm/elm_session.h"
#include "elm/persisted_settings.h"

// Handlers for AT@2, AT@3hhhhhhhhhhhh, ATM0/ATM1, ATFE, ATRD, ATSDhh (not
// AT@1: see docs/tasks/03-elm-core-and-formatting.md). A handler may
// mutate `cache` and, if persistence is enabled, `store`; it never
// touches ICanPort.

namespace esp_obd::elm {

// Dispatches `atRemainder` (the normalized command text after "AT") to a
// settings-persistence handler. Returns std::nullopt if this is not a
// command this family recognizes -- the caller then treats it as unknown,
// same as an unrecognized core command.
std::optional<ElmReply> dispatchSettingsCommand(const ElmSession& session,
                                                 PersistedSettingsCache& cache,
                                                 core::ISettingsStore& store,
                                                 const char* atRemainder);

}  // namespace esp_obd::elm
