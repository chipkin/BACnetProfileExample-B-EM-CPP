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

Part of the CAS BACnet Stack **BACnet profile example series** - one repository
per BACnet device profile. This example claims **only** B-EM.

> **Versions:** this document describes **example v1.0.0**, built and verified
> against **CAS BACnet Stack 6.0.21 (`6.x` @ `abd4cee1`)**, linked as a static
> library, at **Protocol_Revision 24**, with the vendored `common/` helper at
> **v2.5.0**. Running the example prints all three - if what it prints disagrees
> with this line, trust the program and check `CHANGELOG.md`.

This example is also the series' **canonical source** for two shared feature
blocks later repos copy byte-for-byte: **F-COVM** (SubscribeCOVPropertyMultiple)
and **F-ELEVATOR** (the Elevator Group / Lift / Escalator object family).
[B-EC](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) (an Elevator
*Controller*, not just a monitor) seeds from this repository and adds writes
back for the lift's commandable pieces.

## What is a B-EM (Elevator Monitor) profile?

A **device profile** (ANSI/ASHRAE 135, Annex L) lists the capabilities a class
of device must support. A **B-EM** (Annex L.13) is a **monitor**: it reports
what an elevator installation (lifts, escalators, the group that coordinates
them) is doing, with COV and alarms, but issues and accepts no commands. (New
to BACnet? See Chipkin's
[What is BACnet?](https://docs.chipkin.com/protocols/bacnet/) guide.)

**What the profile requires** (and where this example stands):

| Requirement | BIBB | This example |
|---|---|:--:|
| ReadProperty | DS-RP-B | ✅ |
| ReadPropertyMultiple | DS-RPM-B | ✅ |
| SubscribeCOV | DS-COV-B | ✅ |
| SubscribeCOVPropertyMultiple | DS-COVM-B | ✅ (canonical - F-COVM) |
| Generate event notifications | AE-N-I-B | ✅ (intrinsic ChangeOfState on the Lift's Passenger_Alarm) |
| Accept AcknowledgeAlarm | AE-ACK-B | ✅ |
| Answer GetEventInformation | AE-INFO-B | ✅ |
| DeviceCommunicationControl | DM-DCC-B | ✅ |
| Who-Is/I-Am (answer), Who-Has/I-Have | DM-DDB-B, DM-DOB-B | ✅ |

No WriteProperty, WritePropertyMultiple, Schedule, ReinitializeDevice or
TimeSynchronization BIBB is in this profile, and none is implemented - a B-EM
is deliberately read-only.

## F-COVM: SubscribeCOVPropertyMultiple (the headline B-EM capability)

**Analog Input 1 "Bronze"** has both its `Present_Value` AND `Status_Flags`
marked subscribable (`BACnetStack_SetPropertySubscribable`), on top of the
device-wide `BACnetStack_SetCOVMultipleSettings` table-capacity call. A client's
**one** SubscribeCOVPropertyMultiple request naming both properties of Bronze
gets **one multi-property notification** whenever Bronze's value changes -
nudge it with the up/down keys and watch. A plain SubscribeCOV on Bronze still
works too (DS-COV-B): `SetPropertySubscribable` is not required for a standard
object type's `Present_Value`, only for a second property or COVProperty on any
property (see the doc comment on `BACnetStack_SetPropertySubscribable`).

## F-ELEVATOR: the elevator object family

**Elevator Group 1 "Maroon"** groups **Lift 1 "Mauve"** (`isGroupOfLifts =
true`); **Escalator 1 "Mint"** is not a Lift, so it is added with the spec's
"no reference" sentinel instead of belonging to Maroon. Adding Maroon also
requires **Positive Integer Value 1 "Turquoise"** to exist first - this example
adds it explicitly, before Maroon, as the object Maroon's `Machine_Room_ID`
names. (`BACnetStack_AddElevatorGroupObject`'s own doc comment reads as though
the stack creates it automatically; measured against the running stack, it
does not - see AGENTS.md.)

The stack stores only a handful of each elevator object's properties
(`Object_Identifier`, `Group_ID`, `Installation_ID`, `Group_Members`, ...);
everything else - including several properties this profile treats as
required - comes from four callbacks this example registers, each paging
through a list- or array-shaped property one element at a time:

| Callback | Serves | Demonstrated on |
|---|---|---|
| `GetListOfEnumerations` | `Fault_Signals` (Lift, required) | Mauve - reports no active faults |
| `GetListElevatorGroupLandingCallStatus` | `Landing_Calls` (Elevator Group, optional - enabled) | Maroon - reports no calls outstanding |
| `GetSequenceLiftAssignedLandingCall` | `Assigned_Landing_Calls` (Lift, optional - enabled) | Mauve - one call, floor 5, direction up |
| `GetSequenceLiftRegisteredCarCall` | `Registered_Car_Call` (Lift, optional - enabled) | Mauve - one call, floor 5 |
| `GetSequenceLiftLandingDoorStatus` | `Landing_Door_Status` (Lift, optional - enabled) | Mauve - floor 3 (the car's current position), door closed |

A **fifth** elevator callback exists in the stack -
`RegisterCallbackSetElevatorGroupLandingCallControl`, which would accept a
client's landing-call *command* - and this example **deliberately does not
register it**. That single omission is what keeps `Landing_Call_Control`
non-writable and is part of how this device guarantees it accepts no command
of any kind.

`docs/property-profile-reference.md`'s generated Lift table is **incomplete**
relative to the properties `CASBACnetStackDLL.h`'s own doc comment documents as
required (a documentation gap in the stack repository, not an API gap) - this
example serves `Car_Position`, `Car_Moving_Direction`, `Car_Door_Status`,
`Passenger_Alarm`, `Out_Of_Service` and `Fault_Signals` regardless, because
clause 12.59 genuinely requires them. See `docs/objects.json`'s notes for detail.

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

## What this example does NOT do

Nothing. Every BIBB B-EM requires is implemented against the pinned stack; no
`TODO.md` gap exists. (The generated Lift table's documentation gap described
under F-ELEVATOR above is not a functional gap - this example serves every
genuinely required property regardless.)

## Before you ship

This example is a tutorial, and it identifies itself as one. Everything in this
table is read by clients and shown to the operator in **every discovery tool on
the network**. Left as-is, your product appears on a real site announcing itself
as a Chipkin demo. None of it is cosmetic.

| Constant (`main.cpp`) | Ships as | Change it to |
|---|---|---|
| `VENDOR_IDENTIFIER` | `389` (Chipkin) | **Your** company's vendor ID. Assigned by ASHRAE, free: <https://bacnet.org/assigned-vendor-ids/> |
| `VENDOR_NAME` | `Chipkin Automation Systems` | Your company name - must match the vendor ID above. |
| `DEVICE_NAME` | `"Rainbow"` | Your device's `Object_Name`. **Must be unique across the BACnet internetwork.** |
| `MODEL_NAME` | `CAS BACnet Stack Example - B-EM` | Your model designation - what a building operator reads to identify your device. |
| `DEVICE_DESCRIPTION` | a description of *this example* | What your device actually is. |
| `FIRMWARE_REVISION` / `APPLICATION_SOFTWARE_VERSION` | `1.0.0` | Your real versions - wire them to your build. |
| `DCC_PASSWORD` | `""` (no password) | Set your device's secret, or leave empty to accept any DeviceCommunicationControl. It crosses the wire in **plaintext** - a guard against accidents, not a security boundary. |
| Device instance | `389015` (`--deviceID` overrides) | Must be unique on the internetwork. BACnet requires this to be configurable; keep it so. |

`main.cpp` marks this block with a `CHANGE ALL OF THIS BEFORE YOU SHIP` banner.

## Extending the example: read-only means read-only

If you copy this file as the seed for a *writable* elevator profile (as B-EC
does), the single most important thing to preserve is what is **absent**: there
is no `BACnetStack_RegisterCallbackSetProperty*` call anywhere in `main.cpp`,
`SERVICE_WRITE_PROPERTY`/`SERVICE_WRITE_PROPERTY_MULTIPLE` are never enabled,
and `RegisterCallbackSetElevatorGroupLandingCallControl` is never registered.
Adding writes back is exactly the set of things you add, not things you remove.

### Adding an object - read this first

Adding an object is the easiest place to ship a silent non-conformance. The
callbacks are **not uniformly strict**: a Get callback returning `false` does
**not** reliably produce an error for most properties - the stack **silently
substitutes a default** (`Object_Name` -> `"undefined"`, `Units` -> `no-units`,
otherwise a datatype zero) while `Property_List` still advertises the
property. The Lift/Escalator properties this stack refuses to default
(`Car_Position`, `Car_Moving_Direction`, `Car_Door_Status`, `Passenger_Alarm`,
`Operation_Direction`) are the exception - those fail loudly
(`value-not-initialized`) if unserved, which is actually the safer failure
mode; `Out_Of_Service` on the same objects is not, and silently reports
`false`.

So a half-added object looks **healthy** on a scan and is non-conformant. When
you add an instance: add its constant, `BACnetStack_AddObject` (or
`AddLiftOrEscalatorObject`/`AddElevatorGroupObject`) it, serve every required
property in the Get callbacks, then **read back every required property and
diff it against a working object**. "It scanned OK" is the failure mode, not
evidence against it. The block comment above the Get callbacks in `main.cpp`
("ADDING AN OBJECT? READ THIS FIRST") is the in-code version of this.

## Requires the CAS BACnet Stack (licensed product)

This example **builds against the CAS BACnet Stack, a commercial Chipkin product** -
not free or open source, no public/trial build. The stack is the **private** git
submodule `submodules/cas-bacnet-stack`; you can only fetch and build it with a CAS
BACnet Stack license. **To get the stack, contact Chipkin:**
<https://store.chipkin.com/services/stacks/bacnet-stack> or sales@chipkin.com. You
do not need a stack licence to *read* this example's own source: every file outside
submodules/ is CC0 public domain. The licence is what lets you *build* it.

## Build

This example links the CAS BACnet Stack as a prebuilt **STATIC** library. Build
the library once from the pinned submodule commit, then configure and build the
example against it:

```bash
git clone --recursive https://github.com/chipkin/BACnetProfileExample-B-EM-CPP.git
cd BACnetProfileExample-B-EM-CPP
git submodule update --init --recursive   # if not cloned with --recursive
tools/build-stack-static.sh BACnetProfileExample-B-EM-CPP   # from the series root; builds
                                                              # submodules/cas-bacnet-stack/bin/...
cmake -B build -S . -DCAS_BACNET_STACK_LINK=STATIC
cmake --build build --config Release
./build/BACnetExampleBEM            # Linux/macOS
.\build\Release\BACnetExampleBEM.exe   # Windows
```

> **The stack library build takes a few minutes** the first time - it compiles
> the entire CAS BACnet Stack (~600 source files) once, via the stack's own
> project files (`msbuild` on Windows, `make` on Linux). The example itself
> (`main.cpp` + `common/`) then builds in seconds against that library, and
> rebuilds after that are incremental.

Use `-D CAS_STACK_DIR=/path` to point at a stack elsewhere. Options: `--port <n>`
(default 47808), `--deviceID <n>` (default 389015), `--help` (show usage and exit),
`--version` (print the example, stack, and `common/` versions and exit).
Interactive keys: `h` help, `q` quit, up/down nudge Analog Input 1 (also the
F-COVM demonstration). There is no other key - this device has no writable
property and its alarm is simulated on a timer, not a command.

### Link mode

This example links the stack through the `CASBACnetStack::Adapter` CMake target
(`submodules/cas-bacnet-stack/adapters/cpp`) in **STATIC** mode -
`-DCAS_BACNET_STACK_LINK=STATIC` links the prebuilt
`CASBACnetStack_x64_Release.lib` / `libCASBACnetStack_x64_Release.a` built by
`tools/build-stack-static.sh` above. **Application code is identical
regardless of link mode** - `main.cpp` and `common/` call `BACnetStack_AddDevice(...)`
and friends by the exact export name. Every mode requires calling
`LoadBACnetFunctions()` once at the top of `main()` before any other
`BACnetStack_*` call, which runs a version handshake; if it fails,
`CASBACnetStackAdapter_LastError()` says why and the program exits with a
message rather than crashing.

The adapter also offers a **SOURCE** mode (compiles the stack's `source/*.cpp`
straight into the executable, no library build step) - this example is built
and published in **STATIC** mode only.

## Verify

With the [CAS BACnet Explorer](https://store.chipkin.com/products/tools/cas-bacnet-explorer)
(or any client, e.g. `bacpypes3`/`BAC0`):

1. **Discover** - Who-Is -> I-Am from `389015` (vendor `389`).
2. **Object model** - nine objects incl. Elevator Group "Maroon", Lift "Mauve",
   Escalator "Mint", Positive Integer Value "Turquoise" and Notification Class
   "Crimson". `Object_List` lists them all; `Protocol_Revision` = 24.
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

## What's in this repository

`main.cpp` (the example), `common/` (the vendored shared helper), and
`submodules/cas-bacnet-stack/` (the CAS BACnet Stack as a private git submodule,
compiled from source). Self-contained: clone with `--recursive` and build.

## Objects and properties

<!-- OBJECTS-PROPERTIES:BEGIN (generated by tools/gen-objects-properties.py from docs/objects.json - do not edit here) -->
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the stack's `docs/property-profile-reference.md`), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Analog Input 1 "Bronze" - REAL, degrees Celsius; starts at 21.5. F-COVM: Present_Value AND Status_Flags are both marked subscribable (SetPropertySubscribable), so a SubscribeCOVPropertyMultiple naming both fires one multi-property notification when the up/down keys nudge this value

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |

### Binary Input 1 "Emerald" - starts inactive

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |

### Multi-state Input 1 "Hot Pink" - state 1 of 3: On, Off, Auto

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| State_Text *(optional, enabled)* | BACnetARRAY[N] of CharacterString | app | no |

### Network Port 1 "Vermilion" - BACnet/IP; Network_Type and Protocol_Level are set from BACnetStack_AddNetworkPortObject()'s arguments (IPv4, BACnet Application) at start-up, not a GetProperty callback like the object's other app-served rows; Changes_Pending is likewise computed and answered natively by the stack's Network Port object. Reliability has no fault condition this example detects, so it is accepted at the generic default (normal)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Network_Type | BACnetNetworkType | app | no |
| Protocol_Level | BACnetProtocolLevel | app | no |
| Changes_Pending | Boolean | app | no |

### Elevator Group 1 "Maroon" - F-ELEVATOR (canonical). Machine_Room_ID, Group_ID and Group_Members are NOT stack DEFAULTS - they are genuinely stored and served by BACnetStack_AddElevatorGroupObject (Group_Members is also maintained by BACnetStack_AddLiftOrEscalatorObject as Lift 1/Mauve is added to this group). They are marked accepted only because property-profile-reference.md's generic per-type table does not know about this object-specific host-configuration API and so cannot credit them as stack-served. isGroupOfLifts=true and supportLandingCallStatus=true: Group_Mode and LandingCalls are therefore also enabled (see the Lift's note for how the four F-ELEVATOR list/sequence callbacks are exercised) but property-profile-reference.md's generic Elevator Group table does not mark either 'required', so neither generates a checked row here - Group_Mode is served by GetPropertyEnumerated (normal) and LandingCalls by GetListElevatorGroupLandingCallStatus (no calls outstanding) regardless

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Machine_Room_ID | BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Group_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Group_Members | BACnetARRAY[N] of BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |

### Lift 1 "Mauve" - F-ELEVATOR (canonical). property-profile-reference.md's generated Lift table is INCOMPLETE relative to CASBACnetStackDLL.h's own doc comment for BACnetStack_AddLiftOrEscalatorObject (a documentation gap in the stack repo, not a stack API gap): the markdown table omits Car_Position, Car_Moving_Direction, Car_Door_Status, Passenger_Alarm, Out_Of_Service and Fault_Signals entirely, even though the DLL header lists all six as REQUIRED Lift properties. This example serves every one of them regardless, because they are genuinely required by clause 12.59 - Car_Position and Car_Moving_Direction via GetPropertyUnsignedInteger/GetPropertyEnumerated, Car_Door_Status (one door, index 1) via GetPropertyEnumerated with useArrayIndex, Passenger_Alarm and Out_Of_Service via GetPropertyBool, Fault_Signals (empty - no active faults) via the GetListOfEnumerations callback. Status_Flags, Elevator_Group, Group_ID and Installation_ID ARE in the generic table (stack-computed/stack-stored) and are accepted for the same reason as Maroon's. Passenger_Alarm is the AE-N-I-B alarm source: an intrinsic ChangeOfState(bool) algorithm routed to Notification Class 1 (Crimson); it is not writable through any API in this stack (a real installation drives it from hardware), so this example simulates it on a 30-second timer rather than a key or a write - see the file header comment in main.cpp. Optional properties Assigned_Landing_Calls, Registered_Car_Call and Landing_Door_Status are enabled and each exercises one of the four F-ELEVATOR list/sequence callbacks with one demonstration entry

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Elevator_Group | BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Group_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Installation_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |

### Escalator 1 "Mint" - F-ELEVATOR. Not a member of Maroon (a group of Lifts, isGroupOfLifts=true) - added with the spec's 'no reference' sentinel (4194303) for Elevator_Group, as clause 12.60.6 allows. Required Operation_Direction (no stack default), Passenger_Alarm and Out_Of_Service are served by the app; the optional Power_Mode and Escalator_Mode are enabled and served too, for a fuller demonstration. Status_Flags/Elevator_Group/Group_ID/Installation_ID are stack-stored, same reasoning as Mauve's

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Elevator_Group | BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Group_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Installation_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Power_Mode *(optional, enabled)* | Boolean | stack default (Generic Boolean default: `false`) | no |
| Operation_Direction | BACnetEscalatorOperationDirection | app | no |
| Escalator_Mode *(optional, enabled)* | BACnetEscalatorMode | stack default (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Passenger_Alarm | Boolean | app | no |

### Positive Integer Value 1 "Turquoise" - not one of the profile's named objects - added explicitly by this application, BEFORE Elevator Group 1 (Maroon), as the object Maroon's Machine_Room_ID references. BACnetStack_AddElevatorGroupObject's own doc comment reads as though the stack creates this object automatically if missing; verified against the running stack, it does not - AddElevatorGroupObject fails outright ('Create the Positive Integer Value object before adding the Elevator Group object') unless the application adds it first. Colour 'Turquoise' per docs/colour-table.md's global positive_integer_value mapping; Present_Value (required, no stack default) is served as an arbitrary plausible room id. Units defaults to no-units (accepted) since a machine-room identifier has none; Out_Of_Service is optional and left at the generic default

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Units | BACnetEngineeringUnits | stack default, accepted (`BACnetEngineeringUnits::noUnits`) | no |

### Notification Class 1 "Crimson" - AE-N-I-B / AE-ACK-B / AE-INFO-B. Priority, Ack_Required and Recipient_List are NOT stack DEFAULTS - they are genuinely populated by BACnetStack_AddNotificationClassObject (Priority, Ack_Required) and BACnetStack_AddRecipientToNotificationClass (Recipient_List) at start-up; marked accepted only because property-profile-reference.md's generic per-type table does not know about this object-specific host-configuration API. Unlike B-AAC (AE-CRL-B), Recipient_List is NOT made writable here - this profile has no AE-CRL-B requirement and B-EM accepts no WriteProperty of any kind

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Priority | BACnetARRAY[3] of Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Ack_Required | BACnetEventTransitionBits | stack default, accepted (Generic BitString default: empty bitstring (zero bits - NOT ) | no |
| Recipient_List | BACnetLIST of BACnetDestination | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |

<!-- OBJECTS-PROPERTIES:END -->

## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
The CAS BACnet Stack supports every standardized device profile in ASHRAE 135-2024 Annex L. One example repository per profile shows how. ✅ = the required BIBB (service) is supported by the CAS BACnet Stack; the **Example** column is the state of that profile's tutorial repository.

### Controllers (Annex L.4)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) ✅ | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) ✅ · [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-CRL-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) 📝 | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-RPM-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-CRL-B · ✅ SCHED-E-B · ✅ T-VMT-I-B · ✅ T-ATR-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Life safety controllers (Annex L.5)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ AE-LS-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ AE-LS-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |

### Access control controllers (Annex L.6)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-ACUC-B · ✅ DS-ACSC-B · ✅ AE-AC-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) 📝 | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-RPM-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-A · ✅ DS-COV-B · ✅ DS-ACAD-A · ☐ DS-ACCDI-A · ✅ DS-ACUC-B · ✅ DS-ACSC-B · ✅ AE-AC-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Lighting controllers (Annex L.11)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-LO-B / DS-BLO-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) 📝 | ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WG-E-B · ✅ DS-ALO-A · ✅ SCHED-E-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |

### Elevator controllers (Annex L.13)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-OCD-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Authentication and authorization (Annex L.14)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) 📝 | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ AA-AS-B |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ NM-BBMDC-B |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-ACAD-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) 📝 | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-COV-B · ✅ DS-ACCDI-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) 📝 | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-A · ✅ DM-DOB-B · ✅ DM-LM-B · ✅ NM-RC-B |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) 📝 | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ GW-EO-B / GW-VN-B |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) 📝 | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DAB-B |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) 📝 | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ NM-SCH-B |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12) — client-side profiles

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) ✅ | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-V-A · ✅ DS-M-A · ✅ AE-N-A · ✅ AE-VN-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-OWS** Operator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-V-A · ✅ DS-M-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-VM-A · ✅ AE-VN-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-MTS-A |
| **B-AWS** Advanced Operator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-AV-A · ✅ DS-AM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-AVM-A · ✅ AE-AVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ DM-DDA-A · ✅ NM-CC-A · ✅ AR-AVM-A |
| **B-XAWS** Extended Advanced Operator Workstation | planned | ✅ union of B-AWS + B-AACWS + B-ALWS + B-AEWS |
| **B-LSAP** Life Safety Annunciator Panel | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LSV-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-LSVN-A |
| **B-LSWS** Life Safety Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LSV-A · ✅ DS-LSM-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-LSVM-A · ✅ AE-LSAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-ALSWS** Advanced Life Safety Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LSAV-A · ✅ DS-LSAM-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-LSAVM-A · ✅ AE-LSAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ AR-AVM-A |
| **B-ACSD** Access Control Security Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACV-A · ✅ DS-ACM-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-MTS-A |
| **B-ACWS** Access Control Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACAV-A · ✅ DS-ACM-A · ✅ DS-ACUC-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACVM-A · ✅ AE-ACAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-AACWS** Advanced Access Control Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACAV-A · ✅ DS-ACAM-A · ✅ DS-ACUC-A · ✅ DS-ACSC-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACAVM-A · ✅ AE-ACAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ AR-AVM-A |
| **B-LOD** Lighting Operator Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LV-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ALWS** Advanced Lighting Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LAV-A · ✅ DS-LAM-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-AVM-A · ✅ AE-AVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-LCS** Lighting Control Station | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LO-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-ALCS** Advanced Lighting Control Station | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ SCHED-E-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-ED** Elevator Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-EV-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-EVN-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-EWS** Elevator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-COVM-A · ✅ DS-EV-A · ✅ DS-EM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-EVM-A · ✅ AE-EAVN-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A |
| **B-AEWS** Advanced Elevator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-COVM-A · ✅ DS-EAV-A · ✅ DS-EAM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-EAVM-A · ✅ AE-EAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |

Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
<!-- PROFILE-TABLE:END -->

## Footprint

Release-build sizes and start-up timing, from the latest tagged release's CI
run (`metrics-windows.json` / `metrics-linux.json`), both built with
`CAS_BACNET_STACK_LINK=STATIC`:

<!-- METRICS -->
| Platform | Binary | Size | SHA-256 (prefix) | Start-up to `ready` | Stack commit | Link mode | Compiler |
|---|---|---|---|---|---|---|---|
| Windows x64 (windows-2022) | `BACnetExampleBEM.exe` | 3,400,192 bytes (~3.2 MiB) | `0b073f7d60177582` | 112 ms | `abd4cee1` | STATIC | Visual Studio 17 2022 |
| Linux x64 (ubuntu-latest) | `BACnetExampleBEM` | 44,752 bytes (~44 KiB) | `e823d863a0255ea7` | 110 ms | `abd4cee1` | STATIC | `/usr/bin/c++` |

From release [v1.0.0](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP/releases/tag/v1.0.0) (`metrics-windows.json` / `metrics-linux.json`).

## References

- **ANSI/ASHRAE 135** - object model (Clause 12), alarming/events (Clause 13),
  services (Clause 16), device profiles (Annex L).
- **CAS BACnet Stack** - <https://store.chipkin.com/services/stacks/bacnet-stack>.
- **B-AAC example** (the alarming/notification-class pattern this reuses) -
  <https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP>.
- **[CHANGELOG.md](CHANGELOG.md)**, **[AGENTS.md](AGENTS.md)**,
  **[`common/README.md`](common/README.md)**.

## Use this in your own project

Self-contained: clone (with the submodule) and build, then copy what you need. The
example source is **CC0-1.0** (public domain). The CAS BACnet Stack is a separate,
commercially licensed product not covered by CC0.
