#pragma once

#include <optional>

#include "can/obd_addresses.h"
#include "elm/elm_session.h"

// Reconciles elm::ElmProtocol (5 values, includes "AutomaticSearch"
// session state) with can::obd::ObdCanProtocol (4 wire configs, no
// "automatic" member -- diagnostic/ can't depend on elm/, so it can't use
// ElmProtocol itself). Shared here so at_commands_protocol.cpp (T09) and
// ElmApplication (T08) don't each keep their own copy.

namespace esp_obd::elm {

std::optional<can::obd::ObdCanProtocol> toObdCanProtocol(ElmProtocol protocol);
ElmProtocol fromObdCanProtocol(can::obd::ObdCanProtocol protocol);

// Makes `protocol` the current one, as ATSP/ATTP do: a concrete protocol is
// connected straight away; AutomaticSearch is disconnected and searches on
// the next request. Does not change session.savedProtocol.
void applyProtocol(ElmSession& session, ElmProtocol protocol);

// ATZ/ATD/ATWS: every setting back to its default, except that the protocol
// returns to session.savedProtocol (the last ATSP) rather than to
// automatic search.
void resetToDefaultsKeepingSavedProtocol(ElmSession& session);

struct RequestCanId {
  uint32_t id = 0;
  bool extended = false;
};

// The CAN id a request is transmitted on under `protocol`. Without ATSH it
// is the protocol's functional id. With ATSH it is built from the stored
// header bytes the way an ELM327 builds it: 11-bit protocols use the low
// 11 bits; 29-bit protocols prefix the priority bits (ATCP, or the first
// byte of an 8-digit ATSH; 0x18 by default).
RequestCanId requestCanIdFor(const ElmSession& session, can::obd::ObdCanProtocol protocol);

}  // namespace esp_obd::elm
