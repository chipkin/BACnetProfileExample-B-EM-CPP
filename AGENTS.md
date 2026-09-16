# AGENTS.md

Guidance for AI coding agents working in this repository. See
<https://agents.md/> for the format. Human contributors should read
[README.md](README.md) first, then [TUTORIAL.md](TUTORIAL.md).

## What this project is

A **tutorial** C++ example that implements the BACnet **B-EM (Elevator
Monitor)** profile as fully as the standard CAS BACnet Stack supports. It is
one of a series - one git repo per BACnet profile. B-EM is **read-only**: it
demonstrates DS-RP-B / DS-RPM-B, COV (DS-COV-B) and COV-Multiple (DS-COVM-B),
intrinsic alarming (AE-N-I-B / AE-ACK-B / AE-INFO-B) and DM-DCC-B against an
Elevator Group / Lift / Escalator object family - and accepts **no
WriteProperty, WritePropertyMultiple, Schedule, ReinitializeDevice or
TimeSynchronization of any kind**. This repository is the series' canonical
source for two shared feature blocks later repos copy byte-for-byte:
**F-COVM** (SubscribeCOVPropertyMultiple) and **F-ELEVATOR** (the Elevator
Group / Lift / Escalator object family and its four list/sequence callbacks).
B-EC (Wave 3) seeds from this repo and adds writes back for the lift's
commandable pieces - keep every comment here accurate for that reuse. The top
priority is that the code reads like a tutorial a customer can learn from and
copy-paste. Favour clarity over cleverness.

## Layout

This repository is self-contained:

- `main.cpp` - the example device.
- `common/` - the shared helper (vendored).
- `README.md` - what this example is. Keep it short and about THIS example only.
- `TUTORIAL.md` - how to extend and review the example. Long-form material that
  would bloat the README belongs here.
- `docs/PICS.md` - the Protocol Implementation Conformance Statement. Its
  objects-and-properties section is GENERATED from `docs/objects.json`; do not
  hand-edit between the `OBJECTS-PROPERTIES` markers.
- `docs/objects.json` - the input to that generator. Update it in the same change
  as any `main.cpp` change that adds an object or a `GetProperty*` branch.
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack** as a git submodule
  (private; compiled from source). After cloning, run
  `git submodule update --init --recursive`.

The `PROFILE-TABLE` block in README.md is also generated, from the example-series
repository's `docs/profile-table.md`. Edit it there, not here.

## Build

Plain CMake, identical on every platform, in the adapter's default SOURCE mode
(the stack's sources are compiled into the executable - no prebuilt library, no
DLL, no per-platform pre-step):

```bash
git submodule update --init --recursive   # once, if not cloned with --recursive
cmake -B build -S .
cmake --build build --config Release
```

The first build compiles the whole stack (~600 files) and takes a few minutes;
rebuilds after that are incremental and fast. Use `-D CAS_STACK_DIR=...` only if
your stack lives outside the bundled submodule. Do not reintroduce a link-mode
flag or a series-root build script into the documented build: a customer
downloads this repository on its own and must be able to build it with the two
commands above.

## Run

```bash
./build/BACnetExampleBEM [--port 47808] [--deviceID 389015]   # Linux/macOS
.\build\Release\BACnetExampleBEM.exe [--port 47808] [--deviceID 389015]   # Windows
```

Interactive keys while running: `h` help, `q` quit, up/down nudge Analog Input
1 (Bronze) - also the F-COVM demonstration (watch a SubscribeCOVPropertyMultiple
subscription on Present_Value + Status_Flags fire together). There is no key
that changes anything else: this device has no writable property, and the
Lift's Passenger_Alarm alarm is simulated on a 30-second timer rather than any
interactive command.

## Conventions

- Device is named "Rainbow"; objects use the series' colour names; vendor id 389.
- **Read-only, unconditionally.** No `BACnetStack_RegisterCallbackSetProperty*`
  call appears anywhere in `main.cpp`, and WriteProperty / WritePropertyMultiple
  are never enabled with `BACnetStack_SetServiceEnabled`. If you are tempted to
  add one while extending this example for a different profile, you are
  building a different profile - do it in a seed repo (e.g. B-EC), not here.
- F-ELEVATOR: `BACnetStack_AddElevatorGroupObject` (a group of Lifts here;
  `supportLandingCallStatus=true` turns on the read side of `Landing_Calls`)
  then `BACnetStack_AddLiftOrEscalatorObject` per Lift/Escalator (pass the
  spec's "no reference" sentinel, `NETWORK_PORT_REFERENCE_PORT_NONE` /
  4194303, for an object not part of that group). Four callbacks page through
  the list/array-shaped elevator properties:
  `RegisterCallbackGetListOfEnumerations` (Fault_Signals),
  `RegisterCallbackGetListElevatorGroupLandingCallStatus` (Landing_Calls),
  `RegisterCallbackGetSequenceLiftAssignedLandingCall` (Assigned_Landing_Calls),
  `RegisterCallbackGetSequenceLiftRegisteredCarCall` (Registered_Car_Call), and
  `RegisterCallbackGetSequenceLiftLandingDoorStatus` (Landing_Door_Status).
  **Never register `RegisterCallbackSetElevatorGroupLandingCallControl`** in
  this repository - that is the write-side callback and would contradict
  "read-only, unconditionally" above.
- `docs/property-profile-reference.md`'s generated Lift table is **incomplete**
  relative to the required properties documented in
  `CASBACnetStackDLL.h`'s own comment above
  `BACnetStack_AddLiftOrEscalatorObject` (a documentation gap in the stack
  repo). Trust the DLL header's doc comment for what a Lift/Escalator actually
  requires, not the generated markdown table; `docs/objects.json`'s notes
  record exactly which properties this affects.
- F-COVM: `BACnetStack_SetPropertySubscribable` on more than one property of
  the same object (here, Analog Input 1's `Present_Value` AND `Status_Flags`)
  plus `BACnetStack_SetCOVMultipleSettings` for the SubscribeCOVPropertyMultiple
  table capacity (parallel to `BACnetStack_SetCOVSettings` for plain COV/COVP).
  Call `BACnetStack_UpdateValue` after changing a subscribable property so the
  stack evaluates outstanding subscriptions.
- Intrinsic alarming: arm an object with `SetIntrinsic*Algorithm` + a
  Notification Class (`AddNotificationClassObject` +
  `AddRecipientToNotificationClass`) + `SetAlarmsAndEventsForObjectEnabled(...,
  true)` (6.x dropped that trailing argument's default - pass it explicitly).
  This example arms `SetIntrinsicChangeOfStateAlgorithmBool` on the Lift's
  `Passenger_Alarm` - a property no API in this stack can write, so the
  "drive the monitored value" step is a 30-second simulation timer in
  `main()`'s loop, not a WriteProperty or a key, consistent with "read-only,
  unconditionally" above.
- DeviceCommunicationControl (DM-DCC-B): the stack runs the enable/disable state
  machine; the `DeviceCommunicationControl` callback just validates
  `DCC_PASSWORD` and logs. This is a device-management service, not a property
  write, and does not conflict with the read-only conventions above.
- Match the surrounding code style: `const`-correct parameters, check every stack
  return value, keep `main.cpp` linear and well-commented.
- **Never edit `common/` in this repo alone** - it is a vendored copy shared by
  every example in the series, with its own version (`COMMON_VERSION`) and
  changelog (`common/CHANGELOG.md`). To change it: edit, bump the version, add
  a changelog entry, then re-copy `common/` into every example repository.

## How to verify a change

There are no unit tests; verification is behavioural:

1. Build, then run one instance on a clear UDP port.
2. With a BACnet client (e.g. the CAS BACnet Explorer, or `bacpypes3`/`BAC0`),
   send **Who-Is** and confirm **I-Am** from the device instance.
3. **ReadProperty** every required property of every object and confirm the
   values; confirm `Protocol_Revision` is 24 and `Object_List` lists all
   objects, including Positive Integer Value 1 (Turquoise), which this
   application adds explicitly before Elevator Group 1 (Maroon).
4. **Confirm NO write is accepted anywhere**: WriteProperty to any property of
   any object (including Analog Input 1's Present_Value, and every elevator
   property) is rejected as an unsupported service; WritePropertyMultiple is
   rejected the same way.
5. **DS-COVM-B**: SubscribeCOVPropertyMultiple naming Analog Input 1's
   Present_Value AND Status_Flags in one request; nudge the value with the
   up/down keys and confirm ONE multiple-notification carries both properties.
   Also confirm a plain SubscribeCOV on Analog Input 1 works (DS-COV-B).
6. **F-ELEVATOR**: ReadProperty every Elevator Group / Lift / Escalator
   property (the generated objects-and-properties block is the checklist);
   confirm `Group_Members` on Maroon lists Mauve; confirm the optional
   `Assigned_Landing_Calls` / `Registered_Car_Call` / `Landing_Door_Status`
   properties on Mauve each read back the one demonstration entry.
7. **Alarming**: wait up to 30 seconds and confirm the Lift's `Event_State`
   transitions and an EventNotification is sent when the simulated
   `Passenger_Alarm` goes active, and again when it clears; AcknowledgeAlarm
   and GetEventInformation both respond.
8. **Device management**: DCC `disable-initiation`/`enable` SimpleACKs; a wrong
   password (if set) is rejected.
9. If you changed the objects or their properties, regenerate `docs/PICS.md`
   (`python tools/gen-objects-properties.py BACnetProfileExample-B-EM-CPP` from
   the series root) and confirm no row comes out flagged with ⚠.

Verification is manual (no in-repo test suite ships). During development a
raw-socket smoke script was used against a running instance on a clear `--port`
(mind the SO_REUSEADDR gotcha - kill stale instances first).

## Releasing

Bump `APP_VERSION` in `main.cpp` and add an entry to [CHANGELOG.md](CHANGELOG.md),
then tag `vX.Y.Z`. The GitHub Actions workflow builds and publishes the release.

## License

See [LICENSE](LICENSE). The CAS BACnet Stack is a separate, commercially
licensed product and is not covered by it.
