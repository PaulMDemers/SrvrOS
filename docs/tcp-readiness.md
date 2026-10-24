# TCP listener readiness

A listening socket becomes readable when a completed connection is available to
accept, even if the peer has not sent application data. This allows protocols
where the server sends a greeting first. Accepted writable readiness and later
readable data/EOF are separate conditions.

## Passive open and queue ownership

`handle_tcp` validates the final ACK before moving from SYN-RECEIVED to
ESTABLISHED and placing one connection in the existing pending queue. SrvrOS
sends no data with its SYN, so the accepted acknowledgment equals the next local
sequence and the peer sequence equals the next expected sequence. Invalid ACKs
or sequences do not make the listener readable. This follows the passive-open
transition described in [RFC 9293, segment processing](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.10.7.4);
it is not a claim of complete TCP conformance.

Duplicate ACKs do not create duplicate queue entries. `accept` consumes the
entry, retains the connection and can return with zero buffered bytes. Later
payload uses the connection's read queue; FIN remains visible as EOF/hangup. A
full pending queue resets and retires the new connection, preserving the entries
already queued. This change does not expand the queue or implement per-listener
backlog limits.

Before this fix, only the inbound payload path called `ensure_pending_connection`.
An empty final ACK completed TCP state but left `poll(listener, POLLIN)` asleep.
The original readiness fixture exposed this by withholding payload until the
server reported acceptance. Packet capture confirmed a complete handshake before
the guest's ten-second listener timeout. Sending data early or extending that
timeout would conceal the defect.

## Receive callback ownership

Completing a handshake wakes applications earlier than the old payload-only
path. e1000 must retain ownership of its receive descriptor across the entire
packet callback: an awakened process can poll before the original callback has
returned the descriptor to hardware. An atomic receive-owner guard makes a
nested/competing poll return zero without consuming a descriptor. The owner
continues and releases the guard after its bounded batch; there is no spin wait
or additional interrupt masking. The direct monitor poll path uses the same
guard. A zero return during contention does not consume pending packets; the
active owner and normal notification/recovery paths still service them.

The initial TCP-only candidate exposed stalled e1000 HTTP traffic while ECM
traffic passed. A repeat of the accepted baseline also stalled, showing that
this hazard predates the handshake change. A nested-poll host fixture reproduces
duplicate descriptor access on the unguarded driver. Keep these failed runs
distinct from final validation; this work does not change USB or sibling gates.

## Local checks

- `make test-tcp-accept`: packet-level host regression for empty final ACK,
  repeated readiness, duplicate ACK, queue drain, later payload/FIN, invalid
  acknowledgment/sequence and queue exhaustion. This test fails on the old code.
- `make test-e1000-rx`: nested callback polling must not redeliver a DMA
  descriptor or skip the next packet; later calls and mapping refusal retain
  correct ownership. This test fails on the unguarded driver.
- `python3 tools/tcp_ready_smoke.py`: existing end-to-end listener and accepted
  socket readiness checks. Preserve the guest deadlines and host payload ordering.
- `python3 tools/tcpstress_smoke.py`: duplicated listener/accepted descriptors,
  close ordering, multiple clients, half-close and EOF.
- `python3 tools/web_smoke.py` and `python3 tools/usb_ecm_smoke.py`: application
  and provider regressions, including clients that send request data immediately.

Build matching images first with `tools/host-env.sh make -j2 all usb-image` and
run VM harnesses sequentially in a checkout. Use the host environment wrapper for
locally unpacked tools. These tests validate synthetic network paths, not physical
adapter acceptance, energy use or USB storage performance. See the
[Air7,2 record](machines/macbookair7-2.md) for exact checked source and results.


The web fixture explicitly runs `sync` and requires `web-log-sync=0` before
reading the supervised server's log. Its redirected descriptor stays open, and
VFS write buffers are published by flush/close; reading a separate descriptor
before publication incorrectly produced missing log markers on the accepted
baseline too. All response, access-log and error assertions remain. This test
step does not add synchronous per-line writes to production web serving.
