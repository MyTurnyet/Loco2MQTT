#pragma once

#include <optional>
#include <string>

// A bidirectional, line-oriented text stream. Shaped like UartPort
// (readLine()/writeLine()) deliberately, but kept as its own port rather
// than reusing UartPort: this seam represents a socket-backed connection
// that can be up or down (isConnected()), a concept USB serial in this
// codebase doesn't need. See docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md.
class LineStream
{
public:
    virtual ~LineStream() = default;
    virtual std::optional<std::string> readLine() = 0;
    virtual void writeLine(const std::string& line) = 0;
    virtual bool isConnected() const = 0;
};
