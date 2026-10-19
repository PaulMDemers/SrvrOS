# SrvrOS agent entry point

This repository is developed by agents working locally on separate Ubuntu Macs.
Start with `docs/ubuntu-device-agent-setup.md`, `docs/machines/README.md` and the
record for this host's actual model. The reference machine is MacBookAir7,2;
Air7,1, Pro11,2, Pro11,3 and Mini7,1 are planned targets, not accepted siblings.
Read `docs/agent-coordination.md` before shared work. Fetch and inspect the actual
integration baseline, relevant PRs and other machines' active scope; publish your
bounded objective, touched subsystems and dependencies in your own unit record.

- Confirm the local model, source branch/commit, USB UUID and installed recovery
  before physical tests. Do not copy another host's UUID, credentials, firmware
  assumptions or acceptance results. Read the user's existing sudo/reboot
  authorization; do not add repeated permission gates when already authorized.
- Use one device branch/checkout per local agent. Coordinate shared subsystem
  edits and publish reviewable commits; preserve user changes. Do not run
  concurrent staging/build/VM harnesses in a shared checkout: many use `latest`
  pointers or fixed output paths.
- Follow the guide's build, exact-stage VM, install, one-shot boot and collection
  sequence. Keep Ubuntu as the recovery OS and retain a known-good boot payload.
  A source change is not physically accepted until the machine's evidence says so.
- Before reboot, save an on-disk run record with model, source/release, workspace,
  recovery, reboot state and checks. After return, collect the matching session
  before starting a new run. Halts and missing completion markers are failures
  or incomplete evidence, not passes.
- Update the machine record and relevant current guides when behavior changes.
  Preserve dated journals as history. Raw images/logs belong in local ignored
  build/evidence storage; accepted backups must survive `make distclean`.
- Keep optional AML probing out of ordinary desktop startup. A successful
  isolated probe or Q35 runtime fixture does not accept native EC/SCI ownership.
  Match device profiles to captured evidence; do not widen
  register/codec/firmware gates merely to make a sibling model initialize.

This entry point does not request parallel subagents or authorize contacting
another task. The local machine agent and shared-code integration owner are
coordination roles, not an automatic task/message scheduler.
