#pragma once

#include <variant>

#include "domain/TurnoutStateChanged.h"

using DomainEvent = std::variant<TurnoutStateChanged>;
