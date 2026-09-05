# ESP32 LocoNet Adapter — Breadboard Build Guide

Phase 1 of 2. This gets a working RX/TX interface proven on a breadboard before any of it goes to a PCB. Each stage below is built and bench-verified on its own before you wire it into the next — same idea as testing a unit in isolation before integrating it, just with a multimeter instead of an assertion.

## How the circuit works

- **RX (LocoNet → ESP32):** LocoNet's data line idles at ~12V and is pulled low by whichever device is transmitting. A 6N137 high-speed optocoupler's LED sits between the LocoNet signal and ground through a current-limiting resistor, so it lights when the bus is idle-high and goes dark when the bus is pulled low. That means the opto's output is **inverted**: bus idle (12V) → RX pin low; bus active (pulled low) → RX pin high. The ESP32 LocoNet library handles this with an `InverseLogic` flag — more on that in troubleshooting.
- **TX (ESP32 → LocoNet):** you can't drive a shared open-collector bus with a normal push-pull GPIO. A 2N3904 transistor acts as the open-collector driver: GPIO high turns it on, pulling the bus line down to send a "dominant" bit; GPIO low lets the bus's own pull-up (provided by your DR5000, which terminates the bus) hold the line high.
- **Connector:** LocoNet uses RJ12 (6P6C). Pins 2 & 5 = ground, pins 3 & 4 = the data signal (redundant pair, same net), pins 1 & 6 = RailSync (a mirror of DCC track packets — not power, so don't try to draw supply current from there).
- **Voltage split:** the opto's LED side runs directly off the 12V bus (through R1), independent of logic voltage. But its *output* is open-collector, so the output pull-up resistor (R2) can go to **3.3V** instead of 5V — that gives you a clean 3.3V logic signal straight into the ESP32 without a separate level shifter. Power the opto chip itself (Vcc) from 5V per its datasheet spec (ESP32 dev boards expose a 5V pin from USB/VIN), but keep R2 on the 3.3V rail. Don't tie the ESP32 GPIOs to 5V anywhere else — they're not 5V-tolerant.

## Tools you'll need

- Two breadboards (see below for why)
- Jumper wires (a mix of short and long — you'll want some full-length ones)
- Multimeter (continuity + DC voltage)
- Ideally a cheap USB logic analyzer or oscilloscope — LocoNet runs at 16.66 kbps, so you want to actually see edges when debugging, not just guess
- Soldering iron (for the RJ12 breakout board's header pins, and if you go the spliced-cable route instead — see below)
- Your DR5000 with an open LocoNet port to test against, or a second known-good LocoNet device

## Which breadboard(s)?

Use two. Put the ESP32 dev board on its own breadboard, and build the opto/transistor interface on a second, smaller one, connected by five jumper wires (5V, 3.3V, GND, RX signal, TX signal).

The reason is physical, not electrical: you already confirmed on the Maltbee build that your ELEGOO ESP-WROOM-32 module uses 25.4mm (1") row spacing rather than the more common 22.86mm. That wider spacing means the two header rows straddle the entire width of a standard breadboard and typically cover every hole immediately next to the pins on both sides — there's no free real estate left beside the module to build anything else on the same board. A half-size breadboard is plenty for the interface circuit itself (it fits in under 30 columns, laid out below), so the second board can be small.

## Bill of materials (breadboard phase)

| Ref | Part | Manufacturer part number | Distributor / order code | Notes |
|---|---|---|---|---|
| U1 | 6N137 optocoupler, DIP-8 | **6N137** (Vishay Semiconductor Opto Division) | Digi-Key **751-1262-5-ND** | Same generic part number is also made by onsemi (Digi-Key `6N137QT-ND`) and Lite-On (`160-1791-ND`) — electrically interchangeable, buy whichever's in stock. Must be the fast variant; a 4N25/PC817 can't switch fast enough for 16.6kbps. |
| Q1 | 2N3904 NPN transistor, TO-92 | **2N3904** | search "2N3904" on Digi-Key/Mouser | Extremely generic part — onsemi, STMicro, Diotec, MCC, and Central Semi all sell pin- and spec-compatible TO-92 2N3904s. |
| R1 | 1kΩ, 1/4W, 5% THT resistor | **CFR-25JB-52-1K** (Yageo) | search that string on Digi-Key/Mouser | LED current-limit, sized for ~10mA from the 12V bus. |
| R2 | 1kΩ, 1/4W, 5% THT resistor | **CFR-25JB-52-1K** (Yageo) | (same as R1) | Opto output pull-up, to **3.3V**. Datasheet allows 330Ω–4kΩ if you want to tune later. |
| R3 | 2.2kΩ, 1/4W, 5% THT resistor | **CFR-25JB-52-2K2** (Yageo) | search that string on Digi-Key/Mouser | Transistor base resistor from ESP32 GPIO. |
| C1 | 0.1µF, 50V, X7R ceramic, radial THT | **K104K15X7RF5TL2** (Vishay BC Components) | Digi-Key **BC1084CT-ND** | Bypass cap across U1's Vcc/GND — cheap insurance against glitches. |
| J1 | RJ12 6P6C breadboard adapter | **PBC6P6C** (Winford Engineering) | winford.com direct, ~$6.95 | Breaks the RJ12 jack out to a 0.1" header row that plugs straight into a breadboard. Not stocked on Digi-Key/Mouser; buy direct from Winford. See alternative below if you'd rather not order from a third site. |
| — | ESP32 dev board | — | — | Any board works for the prototype |

Connector note: if you don't want to place a separate order with Winford, the fallback is to cut one end off a 6-wire LocoNet patch cable, strip the wires, and jumper them straight into the breadboard.

A caveat on the BOM: I confirmed the Digi-Key order codes for U1 and C1 directly, and Winford's PBC6P6C from their own product page. The 2N3904 and the resistors are generic commodity parts where the manufacturer part number is what matters — do a final check against the live listing before ordering.

## Breadboard layout — interface board (Board B)

Coordinates below assume a standard breadboard: columns numbered left to right, rows **a–e** on the upper half and **f–j** on the lower half, split by a center gap (where DIP chips straddle), with a top rail pair and a bottom rail pair. All five holes in a column on the same half (e.g., a–e at column 9) are one electrical strip; the gap keeps the upper and lower halves separate. If your specific board's rails are arranged differently, match by *net*, not by the literal numbers — that's what actually matters.

**Rail assignment** (set this up first):
- Top rail (+) = **5V** — fed from the ESP32 board's 5V/VIN pin
- Top rail (−) = **GND**
- Bottom rail (+) = **3.3V** — fed from the ESP32 board's 3.3V pin
- Bottom rail (−) = **GND**
- Jumper the top rail (−) to the bottom rail (−) once, anywhere clear (e.g. column 25) — easy to forget, and without it the two halves of your circuit don't share a ground reference.

**U1 (6N137), columns 8–11, straddling the gap:**

| Pin | Function | Hole |
|---|---|---|
| 1 | NC | e8 (unused) |
| 2 | Anode | e9 |
| 3 | Cathode | e10 |
| 4 | NC | e11 (unused) |
| 5 | GND | f11 |
| 6 | Vo (RX output) | f10 |
| 7 | Enable | f9 |
| 8 | Vcc | f8 |

**RX-side connections:**

| From | To | Purpose |
|---|---|---|
| R1 leg 1 | b4 (LocoNet bus node, from RJ12 — see below) | |
| R1 leg 2 | b9 (Anode, same strip as e9) | LED current limit |
| Jumper | b10 → top rail (−) | Cathode to GND |
| Jumper | h9 → h8 | Enable tied to Vcc |
| Jumper | i8 → top rail (+) | Vcc = 5V |
| Jumper | h11 → bottom rail (−) | Pin 5 to GND |
| R2 leg 1 | h10 (Vo, same strip as f10) | |
| R2 leg 2 | bottom rail (+) | Pull-up to 3.3V |
| Jumper | i10 → i13 | Taps Vo to a free column (RX node) |
| C1 leg 1 | j8 (Vcc) | |
| C1 leg 2 | j11 (GND) | Bypass cap right at the chip |

RX signal to the ESP32 comes off column 13 (h13 or i13) — that's your "RX node."

**Q1 (2N3904), columns 16–18, row c** (standard TO-92 pinout, flat face toward you, left to right = Emitter/Base/Collector — double-check against the datasheet before bending leads, orientation errors are easy to make):

| Pin | Hole |
|---|---|
| Emitter | c16 |
| Base | c17 |
| Collector | c18 |

**TX-side connections:**

| From | To | Purpose |
|---|---|---|
| Jumper | c16 → top rail (−) | Emitter to GND |
| R3 leg 1 | d17 (Base, same strip as c17) | |
| R3 leg 2 | d20 | Base resistor to a free column (TX node) |
| Jumper | c18 → c4 (LocoNet bus node) | Collector taps the shared bus |

TX signal from the ESP32 lands on column 20 (b20 or d20) — that's your "TX node."

**RJ12 breakout (J1), columns 1–6, row a** (the Winford board's 6-pin header plugs straight in — its own silkscreen tells you which physical pin is which; treat the mapping below as "typical," and verify against the board in hand before trusting it):

| Header position | RJ12 pin | Function | Hole |
|---|---|---|---|
| 1 | 1 | RailSync | a1 (leave unconnected) |
| 2 | 2 | GND | a2 |
| 3 | 3 | Signal | a3 (leave unconnected — pin 4 carries the same net) |
| 4 | 4 | Signal | a4 |
| 5 | 5 | GND | a5 (leave unconnected — pin 2 carries the same net) |
| 6 | 6 | RailSync | a6 (leave unconnected) |

| From | To | Purpose |
|---|---|---|
| Jumper | b2 → top rail (−) | RJ12 ground |
| — | column 4 (a–e strip) is your **LocoNet bus node** — R1 leg 1 and Q1's collector jumper both land here | |

## Connecting Board B to the ESP32 (Board A)

Five wires, board to board:

| Board B | Board A (ESP32) |
|---|---|
| Top rail (+) | 5V / VIN pin |
| Bottom rail (+) | 3.3V pin |
| Either GND rail | Any GND pin |
| RX node (col. 13) | Your chosen RX GPIO |
| TX node (col. 20) | Your chosen TX GPIO |

## Step-by-step

**1. Power rails.** Wire up Board B's rails per the assignment above before placing any components — top (+) = 5V, top (−) = GND, bottom (+) = 3.3V, bottom (−) = GND, with the two GND rails jumpered together. Leave Board A (ESP32) unpowered for now.

**2. Build the RX stage on Board B — and test it alone before touching TX.** Place U1 and wire it per the table above, but don't connect column 4 (the LocoNet bus node) to anything yet. Power Board B from a bench supply or the ESP32's 5V pin (ESP32 unprogrammed is fine, you just need the rail voltages present). With nothing driving column 4, the RX node (col. 13) should read 3.3V. Momentarily jumper a 12V source through R1's position into the Anode to simulate "bus idle," and confirm the RX node drops toward 0V. This proves the RX stage before it ever touches your layout.

**3. Build the TX stage on Board B — and test it alone.** Place Q1 and R3 per the table above. With Board B powered, jumper the TX node (col. 20) to 3.3V by hand and confirm with a multimeter that Q1's collector (c18) reads near 0V (saturated). Remove the jumper and confirm the collector floats. This proves the driver before it's anywhere near the shared bus.

**4. Wire in J1 (RJ12).** Place the RJ12 breakout at columns 1–6 as above, tie its ground pin to the GND rail, and connect column 4 to R1 and to Q1's collector jumper — this closes the loop between the bus, the RX stage, and the TX stage. Leave RailSync unconnected.

**5. Connect Board A (ESP32) to Board B.** Run the five wires from the table above. Pick your RX and TX GPIOs on the ESP32 carefully:
   - RX: a hardware UART-capable pin (e.g., UART2's RX, commonly GPIO16 on many dev boards — check your specific board's pin map). The timing at 16.6kbps needs the hardware UART, not a bit-banged read.
   - TX: any free GPIO, since TX is bit-level timed in software. **Check your board's boot-time GPIO state before picking one** — same discipline as avoiding strapping pins on Maltbee. A GPIO that boots high would momentarily turn Q1 on and pull the whole LocoNet bus low on every ESP32 reset, disrupting every other device on the layout.
   - Use the `LocoNetESP32HB` library (hardware UART receiver, bit-level transmitter — matches this circuit exactly). Configure `pinRx`/`pinTx` to your chosen GPIOs and set `InverseLogic = true` to match this opto circuit's inverted RX signal.

**6. Full loop smoke test.** Connect J1 to an open port on your DR5000 (or your existing LocoNet bus). Flash a minimal sketch that logs received LocoNet messages to serial. You should immediately see traffic (heartbeats, any throttle activity) if RX is working. Then send a simple message (e.g., a `reportSensor` or switch-state packet) from the ESP32 and confirm it shows up in JMRI's LocoNet monitor or on another device.

## Troubleshooting

- **Nothing on RX at all:** check R2 is actually going to the 3.3V rail (not left floating), and confirm the RX node idles near 0V with the bus connected (it should, per the inversion above) — if it's sitting at 3.3V with the bus connected, either the LED isn't lighting (check R1's connection and polarity) or Enable isn't tied high.
- **Garbled/misaligned data:** double check `InverseLogic` is set correctly — if you ever swap to a non-inverting variant later, this is the first thing to flip. Also confirm you're on a real hardware UART pin, not a software-serial pin.
- **TX seems to work but jams the bus / other devices error out:** almost certainly the boot-state GPIO issue from step 5 — verify the TX GPIO doesn't glitch high during ESP32 reset with a scope, or watch the bus voltage on a meter through a reset cycle.
- **Opto runs warm / LED current seems high:** recheck R1's value against your actual measured bus voltage (verify it's really ~12V, not your DR5000's slightly-hot ~15.6V some people see) before assuming the part is bad.
- **Signal seems to disappear at a specific hole:** breadboards do occasionally have a dead contact — if a node reads correctly one column over but not where you expect, try shifting one row within the same strip before assuming the circuit is wrong.

## Notes for the PCB phase (later)

Once this is proven on the bench, moving it to a PCB is mostly a packaging exercise, not a redesign:

- SMD equivalents exist for all of these (6N137 has SMD variants, 2N3904 → SOT-23 equivalent like MMBT3904), consistent with the 0603 passives you're already using on Maltbee.
- Keep the same 2EDG-style pluggable terminal blocks or go with a proper panel-mount RJ12 jack, depending on whether this lives inline on a patch cable or gets its own connector.
- Worth keeping some physical separation between the 12V LocoNet-side traces and the 3.3V ESP32-side traces on the layout, even without full galvanic isolation beyond the opto itself.
- This will get its own KiCad project when you're ready — happy to help scope that out once the breadboard is verified working.
