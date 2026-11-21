# Native one-shot WPA response observations

The native stream's `wifi-handshake5` version1 record reports existing response
outcomes after the exclusive WPA owner returns. It adds no packet/key authority,
device I/O in the live response path, extra clock read, retry, secret retention
or change to cleanup. Historical logs
without these rows remain historical evidence; no categories are inferred for
them. In particular, the failed b475 second connection cannot retrospectively
be labeled a timeout, duplicate M1 or MIC failure.

At most two context summaries each carry up to four ordered responses: stage0
AUTH, stage1 association, stage2 M1 and stage3 M3. A later response requires the
previous stage's success; send/nonce refusal may end before the next response.
Each row records only the original start/last clock, poll/present/ignored counts,
EAPOL kind and categorical outcome. Existing polls supply the clocks. A frame
returned at or after the original deadline is still refused before parsing.

| Response result | Meaning |
| --- | --- |
| 1 | Original parser accepted the response |
| 2 | Original 750ms management or1.5s EAPOL deadline reached |
| 3 | Returned clock moved backward |
| 4 | Original ownership callback refused |
| 5 | Original poll callback failed |
| 6 | Poll returned invalid span/presence |
| 7 | Original delay callback failed |
| 8 | Original20,000-attempt limit reached |
| 9 | Original management/EAPOL parser rejected the returned response |

EAPOL kind0 means no parsed response,1 means M1 key-info flags,2 means M3
key-info flags, and3 means other flags/shape. These are observations of flags,
not a claim that a message is valid or came from the AP. No raw flags, replay
counter, address, nonce, MIC, key data or payload is written by these records.

For a current-lifetime EAPOL call, the unchanged one-shot handshake additionally
retains its last categorical receive result:1 accepted,2 envelope/peer shape,
3 M1 fields,4 PTK derivation refusal,5 unexpected phase,6 EAPOL version,
7 M3 key-info flags,8 replay relation,9 nonce mismatch,10 reserved fields,
11 MIC,12 key unwrap and13 selected RSNE/GTK key data. The same checks run
in the same order before key export. Stale-generation and alias calls leave
this observation unchanged. It survives secret erasure but never grants key
installation, activation, retry or DMA/native reuse.

The strict `tools/wifi_handshake5_log.py` observer requires canonical schema,
ordered bounded epochs/stages, outcome bounds, stage counts, original success/
timeout limits and agreement with existing `wifi-stream5-keys` outcomes.
Missing or malformed companions refuse; absence is not success. A future
source-frozen hardware collector must save this observation before full-protocol
success checks. Its `native_accepted` field is always false.

## Exact M1 duplicate during HTTP reconnect

The HTTP reconnect profile can skip one exact copy of the M1 accepted for the
current association while waiting for M3. It fingerprints the normalized
EAPOL bytes with SHA-256 after M1 validation. The fingerprint stays local to
the handshake wait and is cleared on all exits. The duplicate consumes the
existing poll budget and the original 1.5-second deadline remains in force.
The replay counter, nonce, EAPOL version and any key data must all match for
the duplicate to qualify. SrvrOS still sends one M2 and requires a valid M3
before key installation. A second duplicate or changed M1 goes through the
original rejection path. The other WPA profiles retain the original policy.

The profile emits `wifi-handshake5` version 2. During stage 3, `ignored=1`
records that the exact duplicate was skipped. `kind` and `reject` describe
only the terminal response, and remain zero if the wait times out after the
duplicate. The historical version 1 parser rules remain available.

## November 20 first-connection refusal

Release `d34e89fd74d5318a` repeated the preceding successful runtime with a
fresh journal. The first connection accepted AUTH, association and M1. After
the M2 USB submission, stage 3 recorded `result=9`, `kind=1`, `reject=7`, and
one present response 15.326 ms after starting the M3 wait. This confirms the
native abort on an M1 key-info pattern. The record supplies no equality proof
against the accepted M1 and no M2 radio-delivery proof. M3/M4, key activation,
DHCP and the second connection were never reached. The unused child and
worker retired, and the independent journal confirmed save and unmount.
Preserve this as a first-connection refusal distinct from a timeout or a
bad-MIC M3. [Unit record](machines/macbookpro11-2.md#current-handoff)

## Native fixture coverage

The actual native xHCI/HTTP fixtures model only the AP, HCD, scheduler and child
process. Cases56–58 complete the first production encrypted HTTP lifetime, then
exercise second-connection M3 silence, one exact M1 followed by silence, or a
bad-MIC M3.
They must not activate keys, start second DHCP, alter the closed first context,
claim full provider/RF retirement or reuse an uncertain native owner. Original
endpoint Stop/restoration and unused child/worker cleanup still apply. Case59
supplies eight unrelated frames before valid M3 and requires both original
HTTP lifetimes and full named cleanup to pass. Cases62–65 exercise a first
connection with one exact duplicate followed by valid M3, a changed replay
counter, a changed ANonce, and two exact duplicates. They check that the
original M2 transmit count, key-install boundary, and native cleanup still
hold. Hardware validation is still pending.
