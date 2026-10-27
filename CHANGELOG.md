# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

_No changes yet._

## [1.3.0] - 2026-10-27

Built on CAS BACnet Stack 6.x (`69b6249f`).

### Added

- **`--deviceName <text>`** sets the Device's Object_Name at start-up, like
  `--deviceID` sets its instance (ASHRAE 135-2024 cl. 12.1.1.3 and 12.1.1.4).
  Object_Name must be unique on the internetwork, so two units can now be
  installed together. The default is unchanged (`Chipkin Example B-EM`); a name
  must be 1 to 255 bytes with no control characters, otherwise a warning is
  printed and the default is kept.
- **`--xml`** prints each received and sent frame as an XML block.

### Changed

- **Notification Class 1's Recipient_List is writable**, so a commissioning
  tool can choose who hears the alarms. The default entry is the standard
  local-broadcast entry of cl. 12.21 (Process Identifier 0), and a written list
  lasts until the example restarts. This is the only property that accepts
  WriteProperty; every other write is refused with write-access-denied, and
  WritePropertyMultiple is still not supported.
- **`--help` lists only the options the example reads.** It no longer lists
  `--dcc-password`; the DeviceCommunicationControl password is the
  `DCC_PASSWORD` constant in `main.cpp`.
- **Test scripts** under `tests/` default to this example's own port, 47832.
- Vendored `common/` updated to 3.5.0; see `common/CHANGELOG.md`.
- **Documentation**: the PICS lists the COV notifications the device initiates,
  documents Database_Revision as 10, and now matches what the device serves
  (Lift required properties, `Property_List` on the Elevator Group, Lift,
  Escalator and Positive Integer Value). No behaviour change.

### Fixed

- **Analog Input 1 now has COV_Increment** (read-only, 1.0), as required by
  cl. 12.2 for an Analog Input that supports COV reporting.
- **Requests no longer go unanswered on Windows**: a request arriving just as
  a receive timed out could be silently discarded (vendored `common/` 3.4.1).

## [1.2.0] - 2026-10-16

Built on CAS BACnet Stack 6.x (`64b2cfac5`, 6.0.23).

The example now monitors a moving lift car and reports its faults (ANSI/ASHRAE
135-2024 Annex L.13.3). This release also rolls up the earlier 1.0.1 through
1.1.0 work, which was never released separately.

### Added

- **Lift car simulation** (`simulation/ElevatorCar.cpp`, see README "What is
  Simulation"): floors 1 to 8, one floor every 2 seconds, a door cycle,
  collective control, and a doorway that can be obstructed. After three failed
  closes the car raises `DOOR_CLOSING_FAULT` and serves no stops until the door
  closes. Simulated passengers ride a fixed tour when the car is idle.
- **Lift 1 presents the car**: `Car_Position`, `Car_Moving_Direction`,
  `Car_Door_Status`, `Fault_Signals`, `Registered_Car_Call` and
  `Landing_Door_Status` are live. `Car_Position`, `Car_Moving_Direction` and
  `Car_Door_Status` are subscribable (DS-COVM-B).
- **Lift fault reporting** (FAULT_LISTED, cl. 12.59): a car fault sets
  `Reliability` to `FAULTS_LISTED` and `Event_State` to `FAULT`, and
  Notification Class 1's recipients receive a to-fault notification.
- **`Number_Of_APDU_Retries`** is 3 (cl. 12.11.29).
- **`s` key** presses and releases the car's passenger alarm button.
- **Automated self-tests** under `tests/`: car monitoring over COV and
  ReadPropertyMultiple, car fault handling, and passenger alarm and
  acknowledgement.

### Changed

- **Segmentation is supported** (`segmented-both`, 16 segments, 5000 ms segment
  timeout); `docs/PICS.md` updated.
- **Protocol_Revision** is 30.
- **The passenger alarm** is driven by the car's alarm button, not a 30-second
  timer.
- **`Assigned_Landing_Calls`** is always empty, since a monitor's group takes
  no landing calls.
- **Device renamed** from "Rainbow" to "Chipkin Example B-EM" so examples from
  this series can be told apart on one network.
- **Build** uses the stack's source mode by default
  (`cmake -B build -S .` then `cmake --build build --config Release`); no
  prebuilt static library step is needed.
- **Documentation** restructured into README, `TUTORIAL.md` and `docs/PICS.md`;
  the README now says plainly what the example does and does not model.
- Vendored `common/` updated to 3.4.0 (a broadcast addressed to the socket's
  own address now goes to the subnet broadcast).
- Application_Software_Version is 1.2.0.

### Fixed

- **Elevator Group 1 `Landing_Calls`** could not be read (read-access-denied);
  it now reads as an empty list. `Landing_Call_Control` is no longer present,
  because the standard requires it to be writable (cl. 12.58.10) and this
  device is read-only.
- **`Application_Software_Version` and `Firmware_Revision`** were hard-coded
  and stale; they now follow the example and stack versions.

## [1.0.0] - 2026-09-15

Built on CAS BACnet Stack 6.x (`abd4cee1`, 6.0.21).

### Added

- **Initial B-EM (Elevator Monitor) example** with the full required BIBB set:
  DS-RP-B, DS-RPM-B, DS-COV-B, DS-COVM-B, AE-N-I-B, AE-ACK-B, AE-INFO-B,
  DM-DDB-B, DM-DOB-B and DM-DCC-B.
- **Objects**: Analog Input 1, Binary Input 1, Multi-State Input 1, Network
  Port 1, Elevator Group 1, Lift 1, Escalator 1, Notification Class 1, and a
  Positive Integer Value for the Elevator Group's Machine_Room_ID.
- **Multi-property COV**: Analog Input 1's `Present_Value` and `Status_Flags`
  can be subscribed with one SubscribeCOVPropertyMultiple request.
- **Intrinsic alarm** on Lift 1's `Passenger_Alarm`, routed through
  Notification Class 1. The device accepts no writes.
- Generated "Objects and properties" tables in the README.
