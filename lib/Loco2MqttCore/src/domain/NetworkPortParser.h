#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Validates and parses a TCP/UDP port number from user-supplied text (a
// serial command argument or a web-form field) — shared by CommandLineParser
// and WebFormCommissioningAdapter so both front doors reject the same
// malformed input the same way.
std::optional<uint16_t> parseNetworkPort(const std::string& text);
