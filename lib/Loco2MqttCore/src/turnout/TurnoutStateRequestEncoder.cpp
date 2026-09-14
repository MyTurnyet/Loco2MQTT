#include "turnout/TurnoutStateRequestEncoder.h"

#include "domain/LocoNetChecksum.h"

namespace
{
    constexpr uint8_t kOpcSwState = 0xBC;
}

// SW1/SW2 carry the same zero-based address encoding OPC_SW_REQ uses (see
// TurnoutLocoNetEncoder) — confirmed against JMRI's LnConstants.java and a
// real command station's observed behavior (JMRI issue #4986): the DIR/ON
// bits that matter for OPC_SW_REQ are don't-cares here, so SW2 carries only
// the address's high bits and OPC_SW_REP comes back either way.
LocoNetMessage TurnoutStateRequestEncoder::encode(TurnoutAddress address) const
{
    const int zeroBasedAddress = address.value() - 1;
    const uint8_t sw1 = zeroBasedAddress & 0x7F;
    const uint8_t sw2 = (zeroBasedAddress >> 7) & 0x0F;
    const uint8_t checksum = computeLocoNetChecksum({kOpcSwState, sw1, sw2});
    return LocoNetMessage({kOpcSwState, sw1, sw2, checksum});
}
