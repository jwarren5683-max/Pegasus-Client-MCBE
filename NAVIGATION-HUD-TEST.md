# Loki Navigation HUD test

This test build adds a north-up local radar and session waypoints for Minecraft Bedrock 1.26.5101.0 / 26.50. It does not include a breadcrumb trail.

## Test steps

1. Start a fresh Minecraft session and load Loki with the included `Pegasus.exe`.
2. Join a world or server, press **Tab**, open **Visual**, and enable **Navigation HUD**.
3. Confirm the green center dot stays fixed, loaded mobs appear blue, and loaded players appear red.
4. Adjust **Range** in the module settings. It supports 32–128 blocks.
5. In chat, enter `.waypoint add Home`, move away, and confirm the yellow `Home` marker shows its distance.
6. Try `.waypoint list`, `.waypoint remove Home`, and `.waypoint clear`.
7. Change world or dimension and confirm old waypoints disappear rather than carrying into the new session.

Waypoint names containing spaces must be quoted, for example `.waypoint add "Village Center"`. Waypoints are intentionally session-only in this first test build.

The HUD uses the same verified 26.50 read-only local-player and loaded-actor snapshot as ESP. It adds no new native hook or guessed game offset, and it hides itself when the snapshot becomes stale.
