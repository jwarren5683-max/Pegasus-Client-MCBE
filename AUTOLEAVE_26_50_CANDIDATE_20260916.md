# AutoLeave 26.50 candidate

Separate candidate; previously confirmed visual and Reach desktop packages are
not changed. Disabled by default. Do not load two clients into one session.

## Repair

- Attach AutoLeave to the byte-verified current-version LocalPlayer tick, not
  the legacy gameplay hook. Rearm on player/dimension change and enable revision.
- Read health through ReadProcessMemory, validated component membership and
  sorted unique attribute keys. The AttributesComponent remains hash FD3B0613,
  stride80; AttributeInstance grew from128 to136. Current health is +7C, not
  the default at+70. Native getter25397A0 confirms component stride; accessor
 31A11C0 confirms instance stride136 and the current-value field.
- Verify native health definition11DC54F0 holds key7. Live user damage moved
  only key7 from20 to17.5; rounded heart icons are not exact internal values.
- Validate vector sizes, finite values, maximum health and unchanged vector
  headers. Missing/stale/malformed health fails closed, never becomes zero.
- Use exact ClientInstance vtableE9731B0 slot14, method5DA3B00, prologue and
  native layout instruction gates. Its native diagnostic names
  `virtual ClientInstance::requestLeaveGameAsync`. Call on the engine tick and
  request the normal asynchronous save/disconnect, not process termination.
- Preserve 0.5-10-heart slider, half-heart steps, inclusive threshold, two
  consecutive low samples and a one-shot request. Skip snapshot work after
  a request while native disconnect completes.

## Test and limits

Health-layout regressions cover current vs default, 136-byte instances, old
stride rejection, missing/duplicate keys, NaN/over-max health, unreadable memory
and changing vector headers. Actual world exit has NOT yet been user-tested.
Only normal maximum health up to20 points is accepted by this candidate.
Modified maximum-health worlds are deliberately unavailable pending testing.

Safe first test: in a disposable local world, set Leave at10 hearts, then enable
AutoLeave. It should request normal Save & Quit after two player ticks without
needing further damage. Verify return to the menu and reopen the saved world.
Never interpret client tests as a guarantee against death, network delay, server
combat-logout penalties or lost progress. Private-server testing remains separate.

The read-only health probe never invokes the leave method or changes world
memory. Game captures and live memory/probe output stay local, not on GitHub.

Fresh full Release build completed. The final GameplayModules object was built
after the last source/header change; AutoLeave and gameplay fixtures were forced
Rebuild. All22 tests passed. Read-only live probe verified native health key7.
Candidate DLL SHA256:
B2E1BBDF39DCFCFD5BDE3FA185C56C002DB451012C586F366EBB825B7C35E2B1.
Separate desktop package: `Pegasus Clients/Pegasus AutoLeave TEST - 2026-09-16`.
Actual automatic save/disconnect still requires user activation and confirmation.

