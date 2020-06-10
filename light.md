# Light service protocol

Realistically, with 1 mbit JACDAC, we can transmit under 2k of data per animation frame (at 20fps).
If transmitting raw data that would be around 500 pixels, which is not enough for many
installation and it would completely clog the network.

Thus, light service defines a domain-specific language for describing light animations
and efficiently transmitting them over wire.

Light commands are not JACDAC commands.
Light commands are efficiently encoded as sequences of bytes and typically sent as payload
of `LIGHT_RUN` command.

## Supported commands

Definitions:
* `P` - position in the strip
* `R` - number of repetitions of the command
* `N` - number of pixels affected by the command
* `C` - single color designation
* `C+` - sequence of color designations

Commands:
* `0xD0: set(P, R, C+)` - set pixels to given color pattern, which is repeated `R` times
* `0xD1: fade(P, N, C+)` - set `N` pixels to color between colors in sequence
* `0xD2: fade_hsv(P, N, C+)` - similar to `fade()`, but convert colors to HSV first, and do fading based on that
* `0xD3: rotate_fwd(P, N, K)` - rotate (shift) pixels by `K` positions away from the connector in given range
* `0xD4: rotate_back(P, N, K)` - same, but towards the connector
* `0xD5: wait(M)` - wait `M` milliseconds

The `P`, `R`, `N` and `K` can be omitted.
If only one of the two number is omitted, the remaining one is assumed to be `P`.
Default values:
```
R = 1
K = 1
N = length of strip
P = 0
M = 50
```

`set(P, N, C)` (with single color) is equivalent to `fade(P, N, C)` (or `fade_hsv()`).

## Command encoding

A number `k` is encoded as follows:
* `0 <= k < 128` -> `k`
* `128 <= k < 16383` -> `0x80 | (k >> 8), k & 0xff`
* bigger and negative numbers are not supported

Thus, bytes `0xC0-0xFF` are free to use for commands.

Formats:
* `0xC1, R, G, B` - single color parameter
* `0xC2, R0, G0, B0, R1, G1, B1` - two color parameter
* `0xC3, R0, G0, B0, R1, G1, B1, R2, G2, B2` - three color parameter
* `0xC0, N, R0, G0, B0, ..., R(N-1), G(N-1), B(N-1)` - `N` color parameter

Commands are encoded as command byte, followed by parameters in the order
from the command definition.

TODO:
* subranges - stateful - drop fade etc parameters?