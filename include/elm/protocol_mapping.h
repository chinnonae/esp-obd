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

}  // namespace esp_obd::elm
