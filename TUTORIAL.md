# Tutorial - extending and reviewing the B-EM example

[README.md](README.md) says what this example *is*. This document is the
*how*: how to extend it into your own device, what each object type needs you
to serve, who serves which property, how to review the result for
conformance, and what goes wrong when you get it subtly right.

Read this once before you start changing `main.cpp`. The most expensive
mistake in this example is silent, and the section it lives in is
[Adding an object](#adding-an-object---read-this-first).

- [Extending the example](#extending-the-example)
- [Read-only means read-only](#read-only-means-read-only)
- [Adding an object - read this first](#adding-an-object---read-this-first)
- [What each object type needs you to serve](#what-each-object-type-needs-you-to-serve)
- [Who serves what: Lift 1 "Mauve"](#who-serves-what-lift-1-mauve)
- [Reviewing your device](#reviewing-your-device)
- [Troubleshooting](#troubleshooting)

## Extending the example

The example is intentionally as small as a full B-EM can be, so it is easy to
change.

**Change a sensor's or an elevator object's value or name** - edit the
constants / callbacks in `main.cpp` (e.g. `g_analogInput1Value`, `LIFT_CAR_POSITION`,
or the colour-name strings inside `GetPropertyCharString`).

**Change the device identity before you ship** - vendor ID, vendor name, model
name, description, firmware revision, device name and the
`DeviceCommunicationControl` password are all in the
`CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of `main.cpp`, with a
per-field comment saying what to change it to. That block is the authoritative
checklist; it is in the source rather than here so it cannot be skipped by
someone who only reads the code.

## Read-only means read-only

If you copy this file as the seed for a *writable* elevator profile (as B-EC
does), the single most important thing to preserve is what is **absent**:

- There is no `BACnetStack_RegisterCallbackSetProperty*` call anywhere in
  `main.cpp`.
- `SERVICE_WRITE_PROPERTY` / `SERVICE_WRITE_PROPERTY_MULTIPLE` are never
  passed to `BACnetStack_SetServiceEnabled`.
- `BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl` - the
  fifth F-ELEVATOR callback, the one that would accept a client's landing-call
  *command* - is never registered.

Adding writes back to a seeded copy is exactly the set of things you add, not
things you remove. Leaving a callback unregistered (rather than registering
one that rejects everything) is itself part of the "no write" guarantee: the
stack has no receiver for that command, so the property is never made
writable to begin with.

## Adding an object - read this first

Adding an object is the easiest place to ship a silent non-conformance.

> **Why "it scanned OK" is not evidence of anything.**
> Most of the `GetProperty*` callbacks match on **both** object type *and*
> instance, so a new instance falls through every one of them. Falling
> through does **not** reliably produce an error - the stack errors only for
> the handful of properties it refuses to invent. On this device that
> shortlist is `Present_Value`, `Number_Of_States`, `Relinquish_Default`,
> `Local_Date`, `Local_Time`, a Network Port's `APDU_Length`, and - specific to
> the elevator family - the Lift's `Car_Position`, `Car_Moving_Direction`,
> `Car_Door_Status` and `Passenger_Alarm`, and the Escalator's
> `Operation_Direction` and `Passenger_Alarm` (all `value-not-initialized` if
> unserved). For **everything else** a declined read is **silently
> substituted** with a default:
>
> | Property | If you forget to serve it | Loud? |
> |---|---|:--:|
> | `Present_Value` (input objects) | Error (`read-access-denied`) | yes |
> | `Car_Position` / `Car_Moving_Direction` / `Car_Door_Status` / `Passenger_Alarm` (Lift) | Error (`value-not-initialized`) | yes |
> | `Operation_Direction` / `Passenger_Alarm` (Escalator) | Error (`value-not-initialized`) | yes |
> | `Object_Name` | reads back as the string **`"undefined"`** | **no** |
> | `Units` | reads back as **`no-units` (95)** | **no** |
> | `Out_Of_Service` (Lift/Escalator too) | served on type alone in several places - works by accident | n/a |
>
> It is worse than "wrong value": the object's `Property_List` **still
> advertises** the property. So the object actively claims to have it, and
> then answers with a default. Nothing on the wire says you forgot anything -
> a half-added object looks **healthy**, not broken.
>
> **The `errorCode` out-parameter only helps where you name it.** Every
> `GetProperty*` callback ends with `uint32_t* errorCode`, preset to success
> and read only when you return `false`. `main.cpp` sets it in exactly one
> place - `State_Text` with an out-of-range array index - because naming an
> error on the general catch-all would break the properties the stack is
> *supposed* to fabricate (the Device's `Max_APDU_Length_Accepted`,
> `APDU_Timeout`, and so on).

**The F-ELEVATOR array-length trap.** Every one of the Lift's per-door
`BACnetARRAY` properties (`Car_Door_Status`, and the optional
`Assigned_Landing_Calls` / `Registered_Car_Call` / `Landing_Door_Status`) is
asked for its **length first**, via `GetPropertyUnsignedInteger` with
`useArrayIndex && propertyArrayIndex == 0`, before the stack pages the
elements through the matching per-property callback. This is undocumented in
the stack's own doc comments for these properties; skipping it produces
`BuildArrayProperty "failed to get the size of the array"` server-side and an
`Abort(other)` on the wire - found by testing against a running instance, not
by reading the header.

When you add an object: add its constant, call `BACnetStack_AddObject` (or
`AddLiftOrEscalatorObject` / `AddElevatorGroupObject`), serve every required
property across all the Get callbacks (including the array-length case
above if it has one), then **read back every required property and diff it
against a working object of the same type**. That diff is the only thing that
catches a missed step, because the failure mode is silent.

## What each object type needs you to serve

The application must serve every REQUIRED property the stack does not
generate. It differs per type - this is the checklist:

| Object type | You must serve | Plus |
|---|---|---|
| Analog Input | `Present_Value` (Real), `Object_Name`, `Units` | `Out_Of_Service` |
| Binary Input | `Present_Value` (Enumerated), `Object_Name` | `Polarity`, `Out_Of_Service` |
| Multi-State Input | `Present_Value` (Unsigned), `Object_Name` | `Number_Of_States`, `Out_Of_Service` |
| Network Port | `Object_Name`, `Network_Type`, `Protocol_Level`, `Changes_Pending`, IP addressing (octet strings) | `Out_Of_Service` |
| Elevator Group | `Object_Name` | - (Machine_Room_ID / Group_ID / Group_Members are genuinely stored and served by `AddElevatorGroupObject` itself, not a callback) |
| Lift | `Object_Name`, `Car_Position`, `Car_Moving_Direction`, `Car_Door_Status` (per-door array + its length), `Passenger_Alarm`, `Fault_Signals` (list callback) | `Out_Of_Service` |
| Escalator | `Object_Name`, `Operation_Direction`, `Passenger_Alarm` | `Out_Of_Service` |
| Positive Integer Value | `Present_Value`, `Object_Name` | - |
| Notification Class | `Object_Name` | - (Priority / Ack_Required / Recipient_List are genuinely stored and served by `AddNotificationClassObject` / `AddRecipientToNotificationClass`) |

## Who serves what: Lift 1 "Mauve"

The single most common question when reading this file is "who answers this
property?" Lift 1 ("Mauve") is the most instructive object in the device,
because unlike the base sensors, almost nothing about an elevator object is
generated by the stack:

| Property | Served by | How |
|---|---|---|
| `Object_Identifier` | **stack** | generated from the object you added |
| `Object_Type` | **stack** | generated |
| `Status_Flags` | **stack** | generated |
| `Elevator_Group` / `Group_ID` / `Installation_ID` | **stack**, stored | set by the arguments to `BACnetStack_AddLiftOrEscalatorObject` |
| `Car_Position` | **you** | `GetPropertyUnsignedInteger` - required, no stack default |
| `Car_Moving_Direction` | **you** | `GetPropertyEnumerated` - required, no stack default |
| `Car_Door_Status` | **you** | `GetPropertyEnumerated` with `useArrayIndex`, plus its length via `GetPropertyUnsignedInteger` (index 0) |
| `Passenger_Alarm` | **you** | `GetPropertyBool` - the alarm source; simulated on a 30-second timer, never written |
| `Out_Of_Service` | **you** | `GetPropertyBool` |
| `Fault_Signals` | **you** | `GetListOfEnumerations` - reports an empty list (no active faults) |
| `Assigned_Landing_Calls` *(optional, enabled)* | **you** | `GetSequenceLiftAssignedLandingCall` - one demonstration entry |
| `Registered_Car_Call` *(optional, enabled)* | **you** | `GetSequenceLiftRegisteredCarCall` - one demonstration entry |
| `Landing_Door_Status` *(optional, enabled)* | **you** | `GetSequenceLiftLandingDoorStatus` - one demonstration entry |

Every object, not just this one, is in [docs/PICS.md](docs/PICS.md).

Going further (writable landing-call control, a writable elevator profile)
means implementing a different profile - see B-EC in the series table in
[README.md](README.md), and [Read-only means read-only](#read-only-means-read-only)
above for exactly what stays absent if you seed a copy of this repository.

## Reviewing your device

After you have changed anything, review it against the conformance statement
rather than against "it looked fine in the explorer":

1. Regenerate [docs/PICS.md](docs/PICS.md) after editing `docs/objects.json`
   (see [Keeping the PICS honest](#keeping-the-pics-honest) below). A ⚠ row is
   a required property nothing serves.
2. Read **every** property listed for **every** object with a BACnet client,
   and compare the value against the PICS. `"undefined"`, `no-units`,
   `value-not-initialized` and `0` are the shapes a missed callback takes.
3. Diff a new object of a type against the existing one of that type. Anything
   that differs and shouldn't is a callback that matched on instance.
4. Confirm every service this profile does **not** implement is still
   rejected - for B-EM, WriteProperty and WritePropertyMultiple to **any**
   object, including the elevator family and Analog Input 1's `Present_Value`.
5. Exercise F-COVM: a SubscribeCOVPropertyMultiple naming Analog Input 1's
   `Present_Value` AND `Status_Flags` in one request; nudge the value with the
   up/down keys and confirm **one** notification carries both.
6. Exercise F-ELEVATOR: confirm `Group_Members` on Maroon lists Mauve, and
   that `Assigned_Landing_Calls`, `Registered_Car_Call` and
   `Landing_Door_Status` on Mauve each read back their one demonstration
   entry.
7. Wait up to 30 seconds and confirm Mauve's `Event_State` transitions and an
   EventNotification arrives when the simulated `Passenger_Alarm` goes active,
   and again when it clears; confirm AcknowledgeAlarm and
   GetEventInformation both respond.

### Keeping the PICS honest

`docs/PICS.md` is partly generated. `docs/objects.json` describes each object
and who serves which property; the series tool regenerates the object tables
from it plus the stack's own `docs/property-profile-reference.md` at the
pinned commit:

```bash
python tools/gen-objects-properties.py BACnetProfileExample-B-EM-CPP            # rewrite
python tools/gen-objects-properties.py BACnetProfileExample-B-EM-CPP --check    # fail if stale
```

(That tool lives in the example-series repository, not in this one. If you
only have this repository, edit the generated block by hand and keep it
matching the callbacks in `main.cpp`.)

When you add an object or a property to `main.cpp`, update
`docs/objects.json` in the same change and regenerate. The `app` list is what
the callbacks serve; `accepted` is for a required property you deliberately
leave to the stack's default (or, for the elevator-family objects, one that
the stack genuinely stores via its own host-configuration API rather than a
`GetProperty*` callback), and each one needs a justification. Anything
required, not in `app` and not in `accepted`, comes out as a ⚠ row - that is a
defect, not a feature.

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| On start-up the app prints a red *"UUID has not been set. A UUID must be set for the BACnetSC device to start."* line | **Expected - this is not your bug.** The stack starts a BACnet/SC datalink these IP-only examples never configure; it logs this once and does not spam. |
| CMake error: *"CAS BACnet Stack adapter not found under: ..."* | Submodules not initialized. Run `git submodule update --init --recursive` (or pass `-D CAS_STACK_DIR=...`). |
| `CASBACnetStackDLL.h: No such file or directory` | Same - submodules not checked out. |
| Windows: *"No CMAKE_CXX_COMPILER could be found"* | Install Visual Studio with the "Desktop development with C++" workload, then re-run from a fresh terminal. |
| First build seems stuck for minutes | Normal - it's compiling ~600 stack files. Only the first build is slow. |
| App prints *"Failed to bind UDP port 47808"* | Another BACnet program is already using 47808. Stop it, or run with `--port <n>`. |
| Client sends Who-Is but sees no I-Am | Firewall is blocking UDP 47808, or the client and device are on different subnets (Who-Is is a broadcast). Allow the port; test on the same subnet first. |
| Replies show an unexpected device instance or vendor | Another BACnet device is already answering on this host/port. On Linux/macOS two processes can share the port and both reply; on Windows the example asks for `SO_EXCLUSIVEADDRUSE` (`common/SimpleUDP.cpp`) so this shows up as a bind failure instead. Stop the other device, or use `--port`. |
| A read of an elevator-family property (`Car_Position`, `Passenger_Alarm`, ...) comes back `value-not-initialized` | Expected if you have not served it - this is the one part of the elevator family that fails loudly instead of substituting a default. See [Adding an object](#adding-an-object---read-this-first). |
| `BuildArrayProperty "failed to get the size of the array"` / `Abort(other)` reading a Lift array property | You added a per-door array property without serving its length via `GetPropertyUnsignedInteger(useArrayIndex, propertyArrayIndex=0)`. See the F-ELEVATOR array-length trap above. |
| `AddElevatorGroupObject` fails with *"Create the Positive Integer Value object before adding the Elevator Group object"* | Expected - despite what the stack's own doc comment implies, it does not create the Machine_Room_ID's Positive Integer Value object automatically. Add it first, as `main.cpp` does for Positive Integer Value 1 ("Turquoise"). |
| SubscribeCOVPropertyMultiple client shows no obvious way to test | Some open-source client libraries don't implement the SubscribeCOVPropertyMultiple APDU. Use the [CAS BACnet Explorer](https://store.chipkin.com/products/tools/cas-bacnet-explorer), which does. |
