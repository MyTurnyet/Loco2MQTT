#pragma once

#include <variant>

#include "domain/SetTurnoutPosition.h"

using DomainCommand = std::variant<SetTurnoutPosition>;
