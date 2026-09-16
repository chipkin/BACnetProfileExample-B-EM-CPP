# BACnet B-EM (Elevator Monitor) - C++ example

A tutorial example showing how to implement the BACnet **B-EM (Elevator
Monitor)** device profile, in C++, using the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack). It
answers **ReadProperty / ReadPropertyMultiple**, supports **SubscribeCOV** and
**SubscribeCOVPropertyMultiple**, generates **intrinsic alarms**
(EventNotifications when the Lift's Passenger_Alarm goes active), accepts
**AcknowledgeAlarm** and answers **GetEventInformation**, and handles
**DeviceCommunicationControl**. It is **read-only**: this device accepts **no
WriteProperty, WritePropertyMultiple, Schedule, ReinitializeDevice or
TimeSynchronization of any kind**.

**[Download a prebuilt binary](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP/releases)**
(Windows and Linux x64) - or build it yourself, see [Build](#build) below.

- **[TUTORIAL.md](TUTORIAL.md)** - how to extend this example and how to review
  it for conformance. Read it when you start turning this into your own device.
- **[docs/PICS.md](docs/PICS.md)** - the Protocol Implementation Conformance
  Statement: every object, every property, and who answers it.

> **Versions:** this document describes **example v1.0.0**, built and verified
> against **CAS BACnet Stack 6.0.21** (`6.x` @ `abd4cee1`), at
> **Protocol_Revision 24**, with the vendored `common/` helper at **v2.5.0**.
> Running the example prints all three - if what it prints disagrees with this
> line, trust the program and check `CHANGELOG.md`.

This example is also the series' **canonical source** for two shared feature
blocks later repos copy byte-for-byte: **F-COVM** (SubscribeCOVPropertyMultiple)
and **F-ELEVATOR** (the Elevator Group / Lift / Escalator object family).
[B-EC](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) (an Elevator
*Controller*, not just a monitor) seeds from this repository and adds writes
back for the lift's commandable pieces.

## What this example supports

The example implements exactly the capabilities below - and nothing more,
which is the point of a profile example.

### BIBBs (BACnet Interoperability Building Blocks)

| BIBB | Description | Supported |
|------|-------------|:---------:|
| DS-RP-B | Data Sharing - ReadProperty - B | ✅ |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B | ✅ |
| DS-COV-B | Data Sharing - COV - B | ✅ |
| DS-COVM-B | Data Sharing - COV-Property-Multiple - B | ✅ (canonical - F-COVM) |
| AE-N-I-B | Alarm and Event - Notification Internal - B | ✅ (intrinsic ChangeOfState on the Lift's Passenger_Alarm) |
| AE-ACK-B | Alarm and Event - ACK - B | ✅ |
| AE-INFO-B | Alarm and Event - Information - B | ✅ |
| DM-DCC-B | Device Management - Device Communication Control - B | ✅ |
| DM-DDB-B | Device Management - Dynamic Device Binding - B | ✅ (answer only) |
| DM-DOB-B | Device Management - Dynamic Object Binding - B | ✅ |

No WriteProperty, WritePropertyMultiple, Schedule, ReinitializeDevice or
TimeSynchronization BIBB is in this profile, and **none is implemented** - a
B-EM is deliberately read-only. See [docs/PICS.md](docs/PICS.md) for the full
statement, including which services are explicitly *not* enabled.

### Services (executed / B-side)

| Service | Notes |
|---------|-------|
| ReadProperty, ReadPropertyMultiple | Responds to property reads (DS-RP-B, DS-RPM-B). |
| SubscribeCOV, SubscribeCOVPropertyMultiple | Plain COV plus multi-property COV (DS-COV-B, DS-COVM-B - see F-COVM below). |
| Who-Is / I-Am, Who-Has / I-Have | Answers discovery requests (DM-DDB-B, DM-DOB-B). This device does not initiate its own Who-Is on start-up (no DM-DDB-A), but it does send an I-Am. |
| DeviceCommunicationControl | Accepts enable / disable-initiation, with an optional password (DM-DCC-B). |
| AcknowledgeAlarm, GetEventInformation | Accepts alarm acknowledgement and answers event-status queries (AE-ACK-B, AE-INFO-B). |
| (Unconfirmed/Confirmed)EventNotification | Sent when the Lift's intrinsic ChangeOfState algorithm fires (AE-N-I-B). |

### Object types

| Object type | Instance | Name |
|-------------|:--------:|------|
| Device | 389015 | Rainbow |
| Analog Input | 1 | Bronze |
| Binary Input | 1 | Emerald |
| Multi-State Input | 1 | Hot Pink |
| Network Port | 1 | Vermilion |
| Elevator Group | 1 | Maroon |
| Lift | 1 | Mauve |
| Escalator | 1 | Mint |
| Positive Integer Value | 1 | Turquoise |
| Notification Class | 1 | Crimson |

Every required property of every object, and who answers it, is in
[docs/PICS.md](docs/PICS.md).

## F-COVM: SubscribeCOVPropertyMultiple (the headline B-EM capability)

**Analog Input 1 "Bronze"** has both its `Present_Value` AND `Status_Flags`
marked subscribable (`BACnetStack_SetPropertySubscribable`), on top of the
device-wide `BACnetStack_SetCOVMultipleSettings` table-capacity call. A client's
**one** SubscribeCOVPropertyMultiple request naming both properties of Bronze
gets **one multi-property notification** whenever Bronze's value changes -
nudge it with the up/down keys and watch. A plain SubscribeCOV on Bronze still
works too (DS-COV-B): `SetPropertySubscribable` is not required for a standard
object type's `Present_Value`, only for a second property or COVProperty on any
property.

## F-ELEVATOR: the elevator object family

**Elevator Group 1 "Maroon"** groups **Lift 1 "Mauve"** (`isGroupOfLifts =
true`); **Escalator 1 "Mint"** is not a Lift, so it is added with the spec's
"no reference" sentinel instead of belonging to Maroon. Adding Maroon also
requires **Positive Integer Value 1 "Turquoise"** to exist first - this example
adds it explicitly, before Maroon, as the object Maroon's `Machine_Room_ID`
names (the stack's own doc comment on `BACnetStack_AddElevatorGroupObject`
reads as though the stack creates it automatically; measured against the
running stack, it does not).

The stack stores only a handful of each elevator object's properties
(`Object_Identifier`, `Group_ID`, `Installation_ID`, `Group_Members`, ...);
everything else - including several properties this profile treats as
required - comes from four callbacks this example registers, each paging
through a list- or array-shaped property one element at a time. See
[TUTORIAL.md](TUTORIAL.md#who-serves-what-lift-1-mauve) for the full
served-by breakdown.

A **fifth** elevator callback exists in the stack -
`RegisterCallbackSetElevatorGroupLandingCallControl`, which would accept a
client's landing-call *command* - and this example **deliberately does not
register it**. That single omission is what keeps `Landing_Call_Control`
non-writable and is part of how this device guarantees it accepts no command
of any kind.

## Intrinsic alarming: the Lift's Passenger_Alarm

**Lift 1 "Mauve"** arms an intrinsic **ChangeOfState** (boolean) algorithm on
its `Passenger_Alarm` property: `Passenger_Alarm == true` is the OFFNORMAL
condition. On each transition the stack sends an **EventNotification** to the
recipients of **Notification Class 1 "Crimson"**.

`Passenger_Alarm` reflects a physical alarm button in the elevator car - no API
in this stack makes it writable, and this device accepts no writes anyway - so
this example **simulates** it instead of driving it from a client command: the
main loop flips a simulated passenger-alarm condition every **30 seconds of
uptime** and calls `BACnetStack_UpdateValue`, letting the stack's own algorithm
decide whether to fire. No key press, no WriteProperty, is needed or possible;
a monitor reports what is happening, not what a client told it to do.

## The device this example creates

```
Device 389015  "Rainbow"   (Vendor 389 - Chipkin Automation Systems)
    ├── Analog Input  1            "Bronze"      read-only sensor (REAL, deg C); F-COVM demo (COV on 2 properties)
    ├── Binary Input  1            "Emerald"     read-only sensor (active/inactive)
    ├── Multi-State Input 1        "Hot Pink"    read-only sensor (state 1..3)
    ├── Network Port 1             "Vermilion"   the BACnet/IP port (required)
    ├── Elevator Group 1           "Maroon"      groups Mauve; F-ELEVATOR
    ├── Lift 1                     "Mauve"       car position/doors/alarm; intrinsic ChangeOfState ALARM
    ├── Escalator 1                "Mint"        not grouped (escalators aren't lifts)
    ├── Positive Integer Value 1   "Turquoise"   Maroon's Machine_Room_ID target (added before Maroon)
    └── Notification Class 1       "Crimson"     routes Mauve's Passenger_Alarm events
```

The three inputs plus the Network Port are the series' shared minimum;
**Maroon, Mauve, Mint, Turquoise and Crimson are the B-EM additions**. Object
names follow the series' colour convention (Device is always "Rainbow").

## Requires the CAS BACnet Stack (licensed product)

This example **builds against the CAS BACnet Stack, which is a commercial Chipkin
product** - it is not free or open source, and there is no public/trial build.
The stack is referenced here as the **private** git submodule
`submodules/cas-bacnet-stack`; you can only fetch and build it once you have a CAS
BACnet Stack license and access to that repository.

**To get the CAS BACnet Stack (and access to build this example), contact
Chipkin:** <https://store.chipkin.com/services/stacks/bacnet-stack> or
sales@chipkin.com.

You do not need a stack licence to *read* this example, or to run a
[prebuilt release binary](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP/releases).
The licence is what lets you *build* it - that is the part the stack submodule
gates.

## What's in this repository

This is a **self-contained** project. It ships:

- `main.cpp` - the example device.
- `common/` - the shared helper (UDP, callbacks, CLI, keyboard) vendored in.
- `CMakeLists.txt` - the build, the same on Windows, Linux, and macOS.
- `docs/PICS.md` - the conformance statement.
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack as a git submodule**
  (private; requires a license - see above). Its sources are compiled into the
  executable, so there is no library or DLL to build, ship, or install.

## Prerequisites

- A C++17 compiler (MSVC, GCC, or Clang).
- CMake >= 3.15.
- Git (to fetch the stack submodule).

### Windows

- **C++ compiler** - install
  [Visual Studio Community](https://visualstudio.microsoft.com/downloads/)
  (free) and select the **"Desktop development with C++"** workload.
- **CMake** - from <https://cmake.org/download/>, or `winget install Kitware.CMake`.

### Linux / macOS

- Debian/Ubuntu: `sudo apt install build-essential cmake git`
- macOS: `xcode-select --install` and `brew install cmake`

## Build

CMake only, and the same two commands on every platform:

```bash
git clone --recursive https://github.com/chipkin/BACnetProfileExample-B-EM-CPP.git
cd BACnetProfileExample-B-EM-CPP

cmake -B build -S .
cmake --build build --config Release
```

Already cloned without `--recursive`? Run `git submodule update --init --recursive`
first - the build needs the stack submodule.

> **The first build takes a few minutes** - it compiles the entire CAS BACnet
> Stack (~600 source files) into the executable. Rebuilds after that are
> incremental and take seconds.

If your CAS BACnet Stack lives somewhere other than the bundled submodule, point
CMake at it: `cmake -B build -S . -D CAS_STACK_DIR=/path/to/cas-bacnet-stack`.

## Run

```bash
# Linux / macOS
./build/BACnetExampleBEM

# Windows
.\build\Release\BACnetExampleBEM.exe
```

Expected output:

```
BACnet B-EM (Elevator Monitor) Example - C++ v1.0.0
CAS BACnet Stack version: 6.0.21.0
Common helper (common/) version: 2.5.0
FYI: Listening for BACnet/IP on UDP port 47808 (Network Port 1).
TX 21 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1)
FYI: Device 389015 ("Rainbow") ready. Vendor ID 389. Read-only monitor - no WriteProperty of any kind is accepted. Press 'h' for help.
```

The `TX` line is the start-up I-Am the device broadcasts to announce itself -
this profile requires only DM-DDB-B (answer Who-Is), but this example sends
one anyway on start-up, exactly as every example in the series does. It goes
to the **local subnet broadcast** address (here `192.168.3.255`, computed from
the Network Port's interface), not the global `255.255.255.255`. As clients
talk to the device you'll see `RX ... bytes from ...` and `TX ... bytes to ...`
lines showing the traffic, and once every 30 seconds a
`Simulated Passenger_Alarm on Lift 1 (Mauve): ACTIVE / cleared` line.

The device listens on UDP **47808** (BACnet/IP). Allow that port through your
firewall. To use a different port, pass `--port` (see below).

> **A one-time red `Error:` line about a UUID at start-up is expected and is
> not your bug** - it is the stack's own debug logging: a BACnet/SC datalink
> this IP-only example never configures starts anyway and logs it once.
> [TUTORIAL.md](TUTORIAL.md#troubleshooting) explains it.

### Command-line options

| Option | Default | Meaning |
|--------|---------|---------|
| `--port <n>` | `47808` | UDP port to listen on (BACnet/IP). |
| `--deviceID <n>` | `389015` | The device's BACnet instance number (BACnet requires this to be configurable). |
| `--help`, `-h` | - | Show usage and exit. |
| `--version` | - | Print the example, stack, and `common/` helper versions, then exit. |

### Interactive commands

While the example runs, these keys are available:

| Key | Action |
|-----|--------|
| `h` | Show the version information and this command list. |
| `q` | Quit. |
| up arrow | Increase Analog Input 1 (`Bronze`) by 1.1 (also the F-COVM demonstration). |
| down arrow | Decrease Analog Input 1 (`Bronze`) by 1.1. |

There is no other key - this device has no writable property, and the Lift's
`Passenger_Alarm` alarm is simulated on a 30-second timer, not a command.

## Verify

With the
[CAS BACnet Explorer](https://store.chipkin.com/products/tools/cas-bacnet-explorer):

1. **Discover** - Who-Is -> I-Am from `389015` (vendor `389`).
2. **Object model** - ten objects including the Elevator Group "Maroon", Lift
   "Mauve", Escalator "Mint", Positive Integer Value "Turquoise" and
   Notification Class "Crimson". `Object_List` lists them all;
   `Protocol_Revision` is `24`.
3. **Confirm NO write is accepted anywhere** - WriteProperty to any property of
   any object, and WritePropertyMultiple, are both rejected as an unsupported
   service.
4. **F-COVM** - SubscribeCOVPropertyMultiple naming Analog Input 1 (Bronze)'s
   `Present_Value` AND `Status_Flags`; nudge the value (up/down keys) and
   confirm **one** notification carries both. Confirm a plain SubscribeCOV on
   Bronze also works.
5. **F-ELEVATOR** - ReadProperty every required property of Maroon/Mauve/Mint;
   confirm `Group_Members` on Maroon lists Mauve; confirm
   `Assigned_Landing_Calls`, `Registered_Car_Call` and `Landing_Door_Status` on
   Mauve each read back their one demonstration entry.
6. **Alarming** - wait up to 30 seconds; confirm Mauve's `Event_State`
   transitions and an EventNotification arrives when the simulated
   `Passenger_Alarm` goes active, and again when it clears. AcknowledgeAlarm
   and GetEventInformation both respond.
7. **Device management** - DeviceCommunicationControl `disable-initiation` /
   `enable` is accepted.

For a property-by-property review against the conformance statement, see
[TUTORIAL.md](TUTORIAL.md).


## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
The CAS BACnet Stack supports every standardized device profile in ASHRAE 135-2024 Annex L, and there is one example repository per profile. Pick the profile your device claims, then the language you build in. "Ask" means the example hasn't been built yet for that language - [contact Chipkin](https://www.chipkin.com/contact/) if you need one.

### Controllers (Annex L.4)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) | Ask | Ask | Ask | Ask |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) | Ask | Ask | Ask | Ask |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) | [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) | Ask | Ask | Ask |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) | Ask | Ask | Ask | Ask |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) | Ask | Ask | Ask | Ask |

### Life safety controllers (Annex L.5)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 🚧 | Ask | Ask | Ask | Ask |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) | Ask | Ask | Ask | Ask |

### Access control controllers (Annex L.6)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) | Ask | Ask | Ask | Ask |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) | Ask | Ask | Ask | Ask |

### Lighting controllers (Annex L.11)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) | Ask | Ask | Ask | Ask |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) | Ask | Ask | Ask | Ask |

### Elevator controllers (Annex L.13)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) | Ask | Ask | Ask | Ask |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) | Ask | Ask | Ask | Ask |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) | Ask | Ask | Ask | Ask |

### Authentication and authorization (Annex L.14)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) | Ask | Ask | Ask | Ask |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) | Ask | Ask | Ask | Ask |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) | Ask | Ask | Ask | Ask |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) | Ask | Ask | Ask | Ask |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) | Ask | Ask | Ask | Ask |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) | Ask | Ask | Ask | Ask |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) | Ask | Ask | Ask | Ask |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) | Ask | Ask | Ask | Ask |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | — | — | — | — |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12)

Client-side profiles.

| Profile | C++ | Node.js | C# | Rust | Python |
|---|---|---|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) | Ask | Ask | Ask | Ask |
| **B-OWS** Operator Workstation | planned | — | — | — | — |
| **B-AWS** Advanced Operator Workstation | planned | — | — | — | — |
| **B-XAWS** Extended Advanced Operator Workstation | planned | — | — | — | — |
| **B-LSAP** Life Safety Annunciator Panel | planned | — | — | — | — |
| **B-LSWS** Life Safety Workstation | planned | — | — | — | — |
| **B-ALSWS** Advanced Life Safety Workstation | planned | — | — | — | — |
| **B-ACSD** Access Control Security Display | planned | — | — | — | — |
| **B-ACWS** Access Control Workstation | planned | — | — | — | — |
| **B-AACWS** Advanced Access Control Workstation | planned | — | — | — | — |
| **B-LOD** Lighting Operator Display | planned | — | — | — | — |
| **B-ALWS** Advanced Lighting Workstation | planned | — | — | — | — |
| **B-LCS** Lighting Control Station | planned | — | — | — | — |
| **B-ALCS** Advanced Lighting Control Station | planned | — | — | — | — |
| **B-ED** Elevator Display | planned | — | — | — | — |
| **B-EWS** Elevator Workstation | planned | — | — | — | — |
| **B-AEWS** Advanced Elevator Workstation | planned | — | — | — | — |

🚧 = in progress. "Ask" = not yet built for that language; contact Chipkin if you need it. Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
<!-- PROFILE-TABLE:END -->

## References

- **ANSI/ASHRAE Standard 135** (BACnet) - the protocol standard. Object model
  (Clause 12), alarm and event services (Clause 13), services (Clause 15),
  BACnet/IP (Annex J), device profiles (Annex L). Purchase / preview via the
  [ASHRAE store](https://www.ashrae.org/technical-resources/standards-and-guidelines).
- **What is BACnet?** - Chipkin's introduction:
  <https://docs.chipkin.com/protocols/bacnet/>.
- **CAS BACnet Stack** - product page and documentation:
  <https://store.chipkin.com/services/stacks/bacnet-stack>.
- **CAS BACnet Explorer** - client for testing this device:
  <https://store.chipkin.com/products/tools/cas-bacnet-explorer>.
- **B-AAC example** (the alarming/notification-class pattern this reuses) -
  <https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP>.
- **Shared helper used by this example** - [`common/README.md`](common/README.md).

See also [TUTORIAL.md](TUTORIAL.md), [docs/PICS.md](docs/PICS.md),
[CHANGELOG.md](CHANGELOG.md), and [AGENTS.md](AGENTS.md).
