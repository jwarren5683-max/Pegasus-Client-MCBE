# AutoLeave test preparation

Existing desktop clients have not been changed or removed. The user confirmed
the Reach test and other features appear to work on 2026-09-16.

This folder currently holds an unchanged copy of that confirmed Reach package,
not a repaired AutoLeave release. Do not launch it over the currently loaded
client. The launcher is deliberately labeled DO NOT LAUNCH YET.

AutoLeave is not ready for Minecraft 1.26.5101.0: the current-version LocalPlayer
tick does not call its health/threshold monitor, availability uses the old
gameplay-ready flag, and health component/hash and attribute-definition addresses
are from the old build. The leave-game vtable slot also needs exact-version
validation before any call. An executable address alone does not prove its ABI.

The planned test uses the normal save/disconnect lifecycle, not process killing.
Threshold units are hearts, 0.5-10 in half-heart increments. Existing threshold
policy tests are not proof of a working native health reader or safe world exit.

Before testing a replacement: Save & Quit and fully close Minecraft. Test in a
disposable local world first, one client only. Automatic leaving is not a
guarantee against death or lost progress, particularly on remote servers.
