# Changelog

All notable changes to this project are documented in this file.

## [1.0.0] - unreleased

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
  end-to-end (neither `bacpypes3` 0.0.106 nor `BAC0` implement a
  SubscribeCOVPropertyMultipleRequest APDU - `bacpypes3/apdu.py` marks
  `subscribeCOVPropertyMultiple = 30 ###TODO`) and AcknowledgeAlarm (the
  service enables without error and the callback matches B-AAC's already
  wire-verified implementation byte-for-byte, but was not independently
  exercised on the wire). See the PR description for the full verification
  list.
