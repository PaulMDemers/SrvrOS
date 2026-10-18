# stb_image

Upstream: https://github.com/nothings/stb
Pinned commit: `2c980bb59875b0d32144a71867fbdebb2f77cd20`
File: `stb_image.h` (v2.30)
Original upstream SHA-256: `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`

The file carries its MIT/public-domain license at the end. SrvrOS uses the MIT option. The image worker enables only PNG, JPEG and BMP,
with in-memory input and no HDR/float or SIMD path. Allocation and input/dimension
limits are supplied by the wrapper, outside the upstream file.

Local patch: the scalar JPEG inverse DCT uses 64-bit intermediates in both
passes and clamps the final 64-bit sample before narrowing. The damaged-JPEG
mutation test triggered signed overflow in the original 32-bit second pass;
valid reference images retain their decoded pixels. SIMD is disabled in this
port. `stdint.h` is supplied by the decoder wrapper before the implementation.
