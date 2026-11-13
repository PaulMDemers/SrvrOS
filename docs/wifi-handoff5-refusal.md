# Native second-connection handoff refusal

The first refusal is retained under IRQ exclusion. `wifi-handoff5-refusal`
is observation only: it neither drains an event nor permits reuse or retry.
Reasons are checked in order, preserving the original short-circuit guard.
Only reasons41/56 sample an event, after controller/ring admission; earlier
refusals do not dereference potentially retired event storage. A sampled event
is the current head at capture, not proof of radio delivery.
No packet, address, key or DMA pointer is printed. `event_owner` classifies only
the sampled pointer range:0 unknown/not applicable,1 first command ring,2 first
OUT ring,3 first IN ring. A range match is not completion or retirement proof.

| Reason | Refused predicate |
| --- | --- |
| 1 | `wifi_fence5_active!=s` |
| 2 | `wifi_fence5_connection_switched` |
| 3 | `!s->bound` |
| 4 | `s->retired` |
| 5 | `s->failed` |
| 6 | `s->round` |
| 7 | `s->busy` |
| 8 | `s->noop` |
| 9 | `!old_owner` |
| 10 | `s->owner!=old_owner` |
| 11 | `s->epoch!=old_epoch` |
| 12 | `!old_epoch` |
| 13 | `!old_dma` |
| 14 | `s->dma!=old_dma` |
| 15 | `old_dma->identity!=(uintptr_t)old_dma` |
| 16 | `old_dma->epoch!=old_epoch` |
| 17 | `old_dma->state!=MT76_DMA5_RETIRED` |
| 18 | `old_dma->positions!=s->positions` |
| 19 | `!next_owner` |
| 20 | `next_owner==old_owner` |
| 21 | `next_epoch<=old_epoch` |
| 22 | `!closed` |
| 23 | `!xhci.operational` |
| 24 | `!xhci.controller_owned` |
| 25 | `xhci.stopping` |
| 26 | `xhci.stopped` |
| 27 | `xhci.reclaiming` |
| 28 | `xhci.reclaimed` |
| 29 | `xhci.command_pending` |
| 30 | `xhci.command_failed` |
| 31 | `xhci.sync_users` |
| 32 | `xhci.poll_users` |
| 33 | `xhci.command_ring!=s->command_ring` |
| 34 | `xhci.command_ring_phys!=s->command_phys` |
| 35 | `xhci.command_cycle!=s->cycle` |
| 36 | `s->first>=63` |
| 37 | `xhci.command_enqueue>=63` |
| 38 | `xhci.command_enqueue<s->first` |
| 39 | `xhci.command_enqueue+2+WIFI_FENCE5_CLEANUP_COMMANDS>63` |
| 40 | `!wifi_fence5_closed_records(s)` |
| 41 | `!wifi_fence5_empty()` |
| 42 | `next fence storage is not virgin` |
| 43 | `!closed(old_owner,old_epoch,old_dma)` |
| 44 | `stamp!=wifi_fence5_state_stamp(s,sizeof *s)` |
| 45 | `dma_stamp!=wifi_fence5_state_stamp(old_dma,sizeof *old_dma)` |
| 46 | `wifi_fence5_active!=s` |
| 47 | `wifi_fence5_connection_switched` |
| 48 | `xhci.command_ring!=s->command_ring` |
| 49 | `xhci.command_ring_phys!=s->command_phys` |
| 50 | `xhci.command_cycle!=s->cycle` |
| 51 | `xhci.command_enqueue!=frontier` |
| 52 | `xhci.command_pending` |
| 53 | `xhci.command_failed` |
| 54 | `xhci.sync_users` |
| 55 | `xhci.poll_users` |
| 56 | `!wifi_fence5_empty()` |

`wifi-handoff5-closure` identifies the last closed-owner proof failure (zero
means that proof passed). It is relevant to guard reason43.

| Closure reason | Refused predicate |
| --- | --- |
| 1 | failed HTTP scope was not detached |
| 2 | `(s!=&wifi_stream5_slots[0] && s!=&wifi_stream5_slots[1])` |
| 3 | `s!=wifi_stream5_current` |
| 4 | `!s->connection_mode` |
| 5 | `!s->connection_closed` |
| 6 | `s->epoch!=epoch` |
| 7 | `dma!=&s->dma` |
| 8 | `dma->identity!=(uintptr_t)dma` |
| 9 | `dma->positions!=127` |
| 10 | `dma->epoch!=epoch` |
| 11 | `dma->state!=MT76_DMA5_RETIRED` |
| 12 | `!s->retired` |
| 13 | `!s->restored` |
| 14 | `!s->events_empty` |
| 15 | `(!s->complete && !failed)` |
| 16 | `s->failed` |
| 17 | `s->phase!=9` |
| 18 | `s->active` |
| 19 | `s->data_active` |
| 20 | `!s->closed` |
| 21 | `s->pending[0]` |
| 22 | `s->pending[1]` |
| 23 | `s->ready_rx` |
| 24 | `s->cached_present` |
| 25 | `s->loan.ticket` |
| 26 | `dma->borrowed!=dma->released` |
| 27 | `dma->faults` |
| 28 | `(!s->exchange.complete && !failed)` |
| 29 | `!s->exchange.keys_erased` |
| 30 | `!s->exchange.leave_sent` |
| 31 | `s->exchange.unsafe` |
| 32 | `s->exchange.handshake.keys_may_live` |
| 33 | `!wpa2_ccmp_erased(&s->exchange.keys,epoch)` |
| 34 | `!s->mac.mac_disabled` |
| 35 | `!s->mac.tx_drained` |
| 36 | `!s->mac.rx_drained` |
| 37 | `!s->mac.filter_restored` |
| 38 | `!s->mac.txop_restored` |
| 39 | `s->stop_events[0]!=1` |
| 40 | `s->stop_events[1]!=1` |
| 41 | `!s->d` |
| 42 | `s->d!=wifi_cal5.d` |
| 43 | `s->d->input_context_phys!=s->input_phys` |
| 44 | `s->d->output_context_phys!=s->output_phys` |
| 45 | `(uintptr_t)s->d->input_context!=s->input_pointer` |
| 46 | `(uintptr_t)s->d->output_context!=s->output_pointer` |
| 47 | `!wifi_stream5_parent()` |
| 48 | `!wifi_handoff5_equal(s->d->input_context,s->input,4096)` |
| 49 | `!wifi_handoff5_equal(&wifi_join5,s->parent,sizeof wifi_join5)` |
| 50 | `!wifi_handoff5_equal(&wifi_cal5,s->cal,sizeof wifi_cal5)` |
| 51 | `!wifi_handoff5_equal(&wifi_power5,s->power,sizeof wifi_power5)` |
| 52 | `!wifi_handoff5_equal(&wifi_handoff5,s->handoff,sizeof wifi_handoff5)` |
| 53 | `!wifi_handoff5_equal(&wifi_peer5,s->binding,sizeof wifi_peer5)` |
| 54 | `!wifi_handoff5_equal(&wifi_tx5,s->preview,sizeof wifi_tx5)` |
| 55 | `(context_read32(s->d->output_context,8,0)&7)` |
| 56–87 | restored context mismatch: base + context index ×8 + word; DCIs0/11/16/9 |
| 88 | stack/provider detached or drained proof failed |

## Native observation, November13

Runtimefe9d0af2/released33d2d9894d8cc4b captured reason41 after one
complete actual-process HTTP connection. The retained head is type32/slot2/
DCI1/CC1/event_owner0 with no pending command or sync/poll users and closure0.
This proves the shared empty-head guard refused a control-endpoint completion
outside the old Wi-Fi data rings. Inventory identifies slot2 as the internal
class9 hub0a5c:4500. The observation does not identify the pointer's exact TD owner
or accept consuming that event. Parent RF restoration remains incomplete2636;
second preparation never runs. Native supervisorcase26 reproduces this category
without changing production admission. An owned dispatch correction and its
native success/late/invalid/topology/exhaustion tests remain pending.
