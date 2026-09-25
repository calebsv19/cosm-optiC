# fisiCs portable annotation header

`include/fisics/extensions.h` is an unchanged copy of the fisiCs public
annotation header. It allows ordinary C compilers to build standalone optiC
source archives without a neighboring fisiCs checkout. The compiler and runtime
are not bundled.

Upstream source: `fisiCs/include/fisics/extensions.h` in CodeWork.
SHA-256: `7df00db0931827c33bfb90f5000dda3b2e134a57629166e13e02d8c5b621bc80`.

When refreshing, copy the upstream header unchanged and update this digest.
Workspace builds prefer the neighboring header; `FISICS_INCLUDE_DIR` can override
either default.
