# Changelog

All notable changes to this project are documented in this file.

## [1.2.0] - unreleased

Profile-functional (series bar, `docs/GLOSSARY.md` in the series repository).
ANSI/ASHRAE 135-2024 L.13.3: "A B-EM device performs monitoring of an elevator
control system." It now monitors a moving lift car and reports its faults.

### Added

- **The lift car Simulation** (`simulation/ElevatorCar.cpp`, README "What is
  Simulation"): a car serving floors 1-8, one floor every 2 s, door cycle
  (1 s open / 2 s dwell / 1 s close), collective control, and a doorway that
  can be obstructed: after three failed closes the car raises
  `DOOR_CLOSING_FAULT` and serves no stops until the door closes. Simulated
  passengers ride a fixed tour when the car has been idle 3 s. Nothing in it
  knows about BACnet.
- **Lift 1 "Mauve" presents the car**: `Car_Position`, `Car_Moving_Direction`
  (`UP` / `DOWN` / `STOPPED`), `Car_Door_Status` (`SAFETY_LOCKED` while moving,
  then `OPENING` / `OPENED` / `CLOSING` / `CLOSED`), `Fault_Signals`,
  `Registered_Car_Call` (the car's pending stops) and `Landing_Door_Status`
  (floors 1-8, the car's floor following the car door) - all live, each change
  reported with `BACnetStack_UpdateValue`. `Car_Position`,
  `Car_Moving_Direction` and `Car_Door_Status` are subscribable (DS-COVM-B on
  the car).
- **FAULT_LISTED on the Lift** (cl. 12.59: "For reliability-evaluation, the
  FAULT_LISTED fault algorithm can be applied"), with `Reliability` enabled:
  a car fault takes `Reliability` to `FAULTS_LISTED` and `Event_State` to
  `FAULT`, and Crimson's recipients get a to-fault (CHANGE_OF_RELIABILITY)
  notification; to-fault is now enabled.
- **`Number_Of_APDU_Retries` = 3** (cl. 12.11.29's suggested default), so a
  confirmed notification a recipient missed is sent again.
- **Test-only hooks**, neither a command-line option: `BEM_SIMULATION_CONTROL_PORT`
  (a loopback-only UDP control channel into the Simulation: alarm button,
  doorway, car calls, passengers on/off, speed) and
  `BEM_TEST_EVENT_RECIPIENT_PORT` (a second, confirmed Crimson recipient at
  127.0.0.1 - the Wire Tests' operator workstation).
- **`s` key**: press / release the car's passenger alarm button.
- **Wire Tests**: `car_monitor_test.py` (the Profile Function: the car followed
  floor by floor over COV and RPM, door cycle, speed, car calls, landing
  doors), `car_fault_test.py` (Failure Mode: the obstructed door, FAULT_LISTED,
  to-fault / to-normal notifications, a faulted car serves no stops) and
  `passenger_alarm_test.py` (CHANGE_OF_STATE alarm, GetEventInformation,
  AcknowledgeAlarm; Failure Modes: acknowledging a non-alarm, every write
  refused). Generic helpers proposed for `wiretest/` 0.7.0 live in `tests/`:
  `simulation_control.py`, `property_multiple.py` (RPM client side) and
  `cov_multiple.py` (SubscribeCOVPropertyMultiple). Default port 48030.

### Changed

- The passenger alarm no longer toggles on a 30-second timer; it is the car's
  alarm button (`s`, or the control channel).
- `Assigned_Landing_Calls` is always empty (a monitor's group takes no landing
  calls) instead of a fixed demonstration entry.
- Application_Software_Version 1.2.0.

## [1.1.0] - unreleased

### Fixed

- **Elevator Group 1 (Maroon) had two properties a client could not read**,
  found by the new discovery Wire Test: `Landing_Calls` (470) and
  `Landing_Call_Control` (471) both answered Error `property` /
  `read-access-denied`.
  - `Landing_Calls`: the `GetListElevatorGroupLandingCallStatus` callback
    answered "no calls" by returning `true` without supplying an element; the
    stack's contract is to return `false` on the first call for an empty list.
    It now does, and `Landing_Calls` reads as an empty list.
  - `Landing_Call_Control` is no longer present. 135-2024 cl. 12.58.10: "If it
    is present, it shall be writable" - and this device is read-only.
    `AddElevatorGroupObject` is now called with `supportLandingCallStatus =
    false` (which enables both) and `Landing_Calls` is enabled on its own with
    `SetPropertyEnabled`.

### Added

- **Wire Tests** (`tests/`, README "Wire Tests"): `tests/discovery_test.py`,
  B-SS-CPP's discovery test with this example's object list - Who-Is, the
  Device's `Object_List` (control: exactly the ten objects), then every object's
  `Property_List` and a ReadProperty of every listed property; any Error,
  Reject, Abort or silence fails. `tests/run_wire_tests.py` runs it; CI runs it
  on Linux after the smoke test. 16/16 on Windows and Linux.
- `wiretest/` 0.5.0, the series' shared Wiretest Harness, vendored verbatim.

### Changed

- **`common/` 3.3.0 -> 3.4.0** and **`wiretest/` 0.5.0 -> 0.6.0**, both copied
  verbatim (`common/` from B-SS-CPP `5d78299`, `wiretest/` from B-LS-CPP). With
  `common/` 3.4.0 a broadcast the stack addresses to this socket's own address
  goes to the subnet broadcast instead; `wiretest/` 0.6.0 only adds helpers.
  `tests/discovery_test.py` now sends its Who-Is a second time, with a visible
  `[RETRY]` line, if no I-Am comes back in 3 s: Who-Is is unconfirmed and the
  example's UDP receive can drop a datagram on Windows. Discovery results are
  unchanged: 16/16 on Windows and Linux, no `[RETRY]`.
- **Stack Pin** `6.x` @ `abd4cee1` (6.0.21) -> `6.x` @ `64b2cfac5` (6.0.23). No
  API change needed.
- **`common/` 3.0.0 -> 3.3.0** (B-SS-CPP `b640a81`, verbatim).
- **Protocol_Revision 24 -> 30** (the stack's default at the new pin).
- **Segmentation is now supported** (`segmented-both`, `Max_Segments_Accepted`
  16, `APDU_Segment_Timeout` 5000 ms): the stack's customer build compiles
  segmentation in from this pin (stack #2992). `docs/PICS.md` section 5 updated.
- **README is honest about what moves**: a status note says the example is
  BIBB-conformant but not yet Profile-functional, and the Lift's car position,
  direction and doors are constants (the object tree used to say Mauve has "car
  position/doors/alarm").
- README, AGENTS.md and PICS version lines: stack 6.0.23, `common/`
  3.3.0, Protocol_Revision 30 (the README still said example v1.0.0 and
  `common/` 2.5.0). The series profile table is re-synced. APP_VERSION 1.0.2 ->
  1.1.0.

## [1.0.2] - unreleased

### Changed

- **Device renamed from the series' colour placeholder "Rainbow" to "Chipkin
  Example B-EM"** so devices from different examples in the series are
  distinguishable from each other on the same BACnet network - every example
  previously announced the identical Object_Name "Rainbow", which made two
  examples on one subnet indistinguishable by name. Sub-object names (Analog
  Input 1 "Bronze", etc.) are unchanged - only the Device object's name
  changed. `docs/colour-table.md` (series root) updated to match. APP_VERSION
  bumped 1.0.1 -> 1.0.2.

## [Unreleased]

### Fixed

- **`Application_Software_Version` (12) and `Firmware_Revision` (44) were
  hardcoded and stale** - both were separate `static const char*` constants
  literally `"1.0.0"`, never wired to anything, so they would have silently
  drifted from the real build the moment `APP_VERSION` was next bumped (as it
  is right here, to `1.0.1`). Fixed the same way as the identical bug in
  `BACnetProfileExample-B-SCHUB-CPP`: `Application_Software_Version` now
  reads `APP_VERSION` directly (one source of truth, can't drift from
  `--version`'s own banner again). `Firmware_Revision` is now built at
  runtime from the CAS BACnet Stack's own `BACnetStack_GetAPIMajorVersion()`/
  `GetAPIMinorVersion()`/`GetAPIPatchVersion()`/`GetAPIBuildVersion()` (the
  same 4 calls `common/CASExampleHelper.cpp`'s `PrintVersion()` already uses
  for the startup banner), populated once into `g_firmwareRevision` right
  after `LoadBACnetFunctions()` succeeds. Verified with a clean Release
  build and a real ReadProperty against the running device (via
  `bacpypes3`): `Application_Software_Version = "1.0.1"`,
  `Firmware_Revision = "6.0.21.0"` - both now match the actual running
  build instead of a hardcoded `"1.0.0"`.

### Changed

- **Documentation restructure**, matching `BACnetProfileExample-B-SS-CPP`:
  `README.md` is cut down to this example only (device tree, BIBBs/services/
  object types, licensed-stack notice, prerequisites, build, run, verify,
  footprint, series table, references); the extending/reviewing material and
  the silent-failure warnings moved to a new `TUTORIAL.md`; a new
  `docs/PICS.md` (ANSI/ASHRAE 135 Annex A shape) replaced the README's inline
  "Objects and properties" section, with the application-services section
  spelling out that WriteProperty/WPM are not enabled. `docs/objects.json`
  gained a Device object entry (previously the generated tables omitted the
  Device object entirely); regenerating produces zero ⚠ rows.
- **Build switched from a prebuilt STATIC library to the adapter's default
  SOURCE mode**, matching every other repo in the series that has completed
  this restructure: `cmake -B build -S .` / `cmake --build build --config
  Release`, with no `tools/build-stack-static.sh` step and no
  `-DCAS_BACNET_STACK_LINK=STATIC` flag. `CMakeLists.txt`'s header comment,
  `AGENTS.md`, and `.github/workflows/release.yml` (drops the static-library
  cache/build steps and the matrix `lib:` entries, asserts `SOURCE` instead of
  `STATIC`, and packages `TUTORIAL.md` + `docs/PICS.md` into the release
  artifact) were all updated to match. The Footprint table's numbers were
  measured from the old STATIC build; the next release remeasures them under
  the documented SOURCE build.
- `main.cpp`'s `CHANGE ALL OF THIS BEFORE YOU SHIP` block absorbed the
  per-field guidance that used to live in the README's ship-checklist table,
  including the `DEVICE_NAME` uniqueness warning.

## [1.0.0] - 2026-09-15

### Added

- Initial B-EM (Elevator Monitor) example, built from the plan-only stub.
- CAS BACnet Stack pinned to `6.x` @ `abd4cee1c7f28ca8e1af4720849c4081082bbe82`
  (reports 6.0.21), linked as a **STATIC** library
  (`CAS_BACNET_STACK_LINK=STATIC`; built first by `tools/build-stack-static.sh`
  from the stack's own project files - see README "Link mode").
- `common/` vendored from `BACnetProfileExample-B-SS-CPP` (series source of
  truth), copied verbatim.
- DS-RP-B, DS-RPM-B, DS-COV-B, DS-COVM-B, AE-N-I-B, AE-ACK-B, AE-INFO-B,
  DM-DDB-B, DM-DOB-B, DM-DCC-B - the full B-EM required BIBB set.
- Objects: the three base read-only sensors (Analog Input 1 "Bronze", Binary
  Input 1 "Emerald", Multi-State Input 1 "Hot Pink") + Network Port 1
  ("Vermilion") + Elevator Group 1 ("Maroon") + Lift 1 ("Mauve") + Escalator 1
  ("Mint") + Notification Class 1 ("Crimson"); Positive Integer Value 1
  ("Turquoise") is created implicitly as the Elevator Group's Machine_Room_ID
  target.
- **F-COVM** (canonical for the series): `SetCOVMultipleSettings` plus
  `SetPropertySubscribable` on two properties of one object (Analog Input 1's
  `Present_Value` and `Status_Flags`), so one SubscribeCOVPropertyMultiple
  request yields one multi-property notification.
- **F-ELEVATOR** (canonical for the series): the Elevator Group / Lift /
  Escalator object family (`AddElevatorGroupObject`,
  `AddLiftOrEscalatorObject`) and its four list/sequence callbacks
  (`GetListOfEnumerations`, `GetListElevatorGroupLandingCallStatus`,
  `GetSequenceLiftAssignedLandingCall`, `GetSequenceLiftRegisteredCarCall`,
  `GetSequenceLiftLandingDoorStatus`). The fifth, write-side elevator callback
  (`SetElevatorGroupLandingCallControl`) is deliberately not registered - this
  device accepts no writes of any kind.
- Intrinsic alarming: an intrinsic ChangeOfState (boolean) algorithm on Lift 1
  (Mauve)'s `Passenger_Alarm`, routed through Notification Class 1 (Crimson).
  `Passenger_Alarm` has no writable API in this stack, so it is driven by a
  30-second simulation timer in the main loop rather than a client write or an
  interactive key.
- `docs/objects.json` + the generated "Objects and properties" README block.
- The series profile-table block and footprint placeholder in README.

### Notes

- No gaps: every BIBB this profile requires is implemented against the pinned
  stack. `docs/property-profile-reference.md`'s generated Lift table omits
  several genuinely required properties present in `CASBACnetStackDLL.h`'s own
  doc comment (a documentation gap in the stack repo, not an API gap) - see
  `docs/objects.json`'s notes and AGENTS.md for detail. This example serves
  every one of them regardless.
- Two stack-documentation discrepancies found by testing against a running
  instance, not by reading the doc comments alone: (1)
  `BACnetStack_AddElevatorGroupObject`'s doc comment implies the stack creates
  the Machine_Room_ID's Positive Integer Value object automatically if
  missing - measured, it does not; the call fails until the application adds
  that object first (fixed: Positive Integer Value 1 "Turquoise" is added
  explicitly before Elevator Group 1). (2) Each Lift per-door `BACnetARRAY`
  property needs its element count served via
  `GetPropertyUnsignedInteger(useArrayIndex, propertyArrayIndex=0)` before the
  stack pages the elements through the matching callback - undocumented for
  these properties specifically; omitting it produced `BuildArrayProperty
  "failed to get the size of the array"` server-side and `Abort(other)` on the
  wire.
- Not independently wire-verified this session: SubscribeCOVPropertyMultiple
  end-to-end (the client library available in this session did not implement
  a SubscribeCOVPropertyMultipleRequest APDU) and AcknowledgeAlarm (the
  service enables without error and the callback matches B-AAC's already
  wire-verified implementation byte-for-byte, but was not independently
  exercised on the wire). See the PR description for the full verification
  list.
