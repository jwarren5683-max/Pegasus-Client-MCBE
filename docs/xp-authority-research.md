# Loki 2.4 XP authority research

## Acceptance criterion

A normal guest in a friend's locally hosted Minecraft Bedrock 26.52 world runs `.xp`; the resulting XP must be accepted by the host, spendable by an authoritative enchanting/anvil operation where applicable, and still present after reconnect.

A HUD-only level/XP change is a failed test.

## Confirmed baseline

The historical Horion XpCommand only calls `LocalPlayer::addExperience` or `LocalPlayer::addLevels`. It does not explicitly create or send an XP packet in the command implementation.

Loki 2.3 XP Test cannot call the same modern virtual entries because the 26.52 LocalPlayer entries corresponding to the historical XP calls are empty return stubs. Loki therefore calls the inherited Player implementations directly at the byte-verified 26.52 RVAs.

User testing establishes that this changes the visible XP state but is not authoritative: it cannot be spent and disappears after reconnect.

## Working hypothesis

The old Horion result, where reported to work as a non-owner in another player's local world, depended on behavior that was below the command layer: either an older LocalPlayer override performed additional synchronization, or the older integrated-server protocol accepted a client-side state transition that modern 26.52 no longer accepts in the same form.

Do not assume packet 66 (SpawnExperienceOrbPacket) is a valid serverbound XP award request. Current Mojang protocol metadata notes that packet ID 66 has a different client-to-server interpretation and that client-side side effects can occur regardless of server acceptance. That is exactly the class of false positive this branch must avoid.

## 26.52 protocol constraint

Modern Bedrock uses server-authoritative item stack requests for container UI operations including enchanting. Client predictions can be rejected and discarded by the host. Therefore bypassing only the local XP requirement or locally editing the XP fields cannot satisfy the acceptance criterion.

## Investigation plan

1. Capture packet serialization during a legitimate XP-orb pickup as a guest in a friend-hosted world.
2. Capture the same interval when invoking the current Loki `.xp` implementation.
3. Capture a legitimate enchanting spend.
4. Compare outbound packet IDs, bodies, and the corresponding inbound host responses.
5. Trace the 26.52 call graph around the legitimate XP mutation to identify whether a client-to-host request exists and which engine object owns it.
6. Only after identifying a host-processed request, reproduce that engine path in Loki behind exact-build/signature validation.
7. Verify with the acceptance test above.

## Useful current tooling

EndstoneMC/spyglass is a current in-client Bedrock packet diagnostic. Its implementation hooks Packet::write/read and uses MinecraftPackets::createPacket, which makes it useful as reference for tracing the 26.52 serialization path without guessing packet layouts.

## Rejected shortcuts

- Direct Player::addExperience/addLevels only: confirmed cosmetic/non-authoritative in guest testing.
- Patching the enchanting UI's local XP check only: server-authoritative inventory can reject the predicted operation.
- Blindly sending SpawnExperienceOrbPacket: direction semantics do not establish an XP-award request and can create client-only side effects.
