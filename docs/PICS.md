# BACnet Protocol Implementation Conformance Statement (PICS)

For the **BACnet B-EM (Elevator Monitor) C++ example** -
see [README.md](../README.md).

> This is the PICS **for the example as shipped**. It describes a tutorial
> device announcing itself as a Chipkin demo, not a product. When you turn this
> example into your own device, this document is one of the things you rewrite:
> the vendor, model and version rows all come from the
> `CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of `main.cpp`. The
> example has **not** been submitted for BTL certification.
>
> **This device is read-only, but for one property.** It accepts no
> WritePropertyMultiple, Schedule, ReinitializeDevice or TimeSynchronization of
> any kind, and it executes WriteProperty for Notification Class 1's
> `Recipient_List` alone (commissioning the alarm recipients) - Section 4 below
> spells out exactly which services are (and are not) enabled.

## 1. Product description

| | |
|---|---|
| **Vendor Name** | Chipkin Automation Systems |
| **Vendor Identifier** | 389 |
| **Product Name** | CAS BACnet Stack Example - B-EM |
| **Product Model Number** | CAS BACnet Stack Example - B-EM |
| **Application Software Version** | 1.3.0 |
| **Firmware Revision** | 6.0.23.0 (the CAS BACnet Stack version) |
| **BACnet Protocol Version** | 1 |
| **BACnet Protocol Revision** | 30 |

**Product Description:** a read-only BACnet/IP monitor for an elevator
installation, built on the CAS BACnet Stack. It presents the series' three base
sensor objects plus an Elevator Group / Lift / Escalator object family; the Lift
follows a simulated lift car (README "What is Simulation") as it moves floor to
floor. It answers ReadProperty and ReadPropertyMultiple for every required
property of every object, supports SubscribeCOV and SubscribeCOVPropertyMultiple
(including the car's `Car_Position`, `Car_Moving_Direction` and
`Car_Door_Status`), generates intrinsic event notifications - CHANGE_OF_STATE on
the Lift's `Passenger_Alarm`, FAULT_LISTED on its `Fault_Signals` - accepts
AcknowledgeAlarm, answers GetEventInformation, and handles
DeviceCommunicationControl. It accepts WriteProperty only for Notification
Class 1's `Recipient_List`, and **no** WritePropertyMultiple, Schedule,
ReinitializeDevice or TimeSynchronization. It is a tutorial for implementers of the B-EM profile.

## 2. BACnet standardized device profile (Annex L)

**B-EM - BACnet Elevator Monitor** (Annex L.13).

This device claims exactly one profile. Because the B-EM requirements are a
superset of B-GENERAL's, a conformant B-EM device also satisfies **B-GENERAL**
(Annex L.8); that is subsumption, not a second claim.

## 3. BIBBs supported (Annex K)

| BIBB | Description |
|---|---|
| DS-RP-B | Data Sharing - ReadProperty - B |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B |
| DS-COV-B | Data Sharing - COV - B |
| DS-COVM-B | Data Sharing - COV-Property-Multiple - B |
| AE-N-I-B | Alarm and Event - Notification Internal - B |
| AE-ACK-B | Alarm and Event - ACK - B |
| AE-INFO-B | Alarm and Event - Information - B |
| DM-DCC-B | Device Management - Device Communication Control - B |
| DM-DDB-B | Device Management - Dynamic Device Binding - B |
| DM-DOB-B | Device Management - Dynamic Object Binding - B |

No other BIBBs are supported. **In particular this device does NOT support
DS-WP-B or DS-WP-A (WriteProperty), DS-WPM-B (WritePropertyMultiple),** any
scheduling (SCHED-*) or trending (T-*) BIBB, DM-RD-B (ReinitializeDevice), or
DM-TS-B / DM-UTC-B (TimeSynchronization). That is the whole point of a
**monitor**: it reports what the elevator installation is doing and accepts no
command of any kind. It executes WriteProperty for one property only, Notification
Class 1's `Recipient_List` (section 4); that is commissioning, not the DS-WP-B
BIBB, which the B-EM profile does not require and this device does not claim.

## 4. Application services supported

| Service | Initiate | Execute |
|---|:---:|:---:|
| ReadProperty | no | **yes** |
| ReadPropertyMultiple | no | **yes** |
| WriteProperty | no | **yes** (Notification Class 1 `Recipient_List` only) |
| SubscribeCOV | no | **yes** |
| SubscribeCOVPropertyMultiple | no | **yes** |
| Who-Is | no | **yes** |
| I-Am | **yes** | - |
| Who-Has | no | **yes** |
| I-Have | **yes** | - |
| DeviceCommunicationControl | no | **yes** |
| AcknowledgeAlarm | no | **yes** |
| GetEventInformation | no | **yes** |
| ConfirmedEventNotification / UnconfirmedEventNotification | **yes** | no |
| ConfirmedCOVNotification / UnconfirmedCOVNotification | **yes** | no |
| ConfirmedCOVNotificationMultiple / UnconfirmedCOVNotificationMultiple | **yes** | no |

COV notifications are sent to each SubscribeCOV / SubscribeCOVPropertyMultiple
subscriber, confirmed or unconfirmed as the subscriber asked (DS-COV-B,
DS-COVM-B: a SubscribeCOVPropertyMultiple subscriber gets the -Multiple form).
An I-Am is sent in response to Who-Is (this profile does not require a B-EM to
initiate its own Who-Is on start-up, unlike DM-DDB-A). Event notifications for
the Lift - its `Passenger_Alarm` (CHANGE_OF_STATE) and its `Fault_Signals`
(FAULT_LISTED, notified as CHANGE_OF_RELIABILITY) - are sent unconfirmed to the
recipient seeded in Notification Class 1 ("Crimson"): the local subnet
broadcast. That is the default: `Recipient_List` is writable, so a commissioning
tool replaces it with the workstations that should hear the alarms (clause 12.21
fixes the default only for a list that is not writable). A written list lasts
until the device restarts; like every other setting of this example, nothing is
saved to disk. The Wire Tests add a second, confirmed recipient through the
test-only `BEM_TEST_EVENT_RECIPIENT_PORT` environment variable (README "What is
Simulation"); `Number_Of_APDU_Retries` is 3, so a confirmed notification that
is not acknowledged is sent again after `APDU_Timeout` (10000 ms).

**WriteProperty is enabled for one property; WritePropertyMultiple is NOT
enabled.** The only property registered writable is Notification Class 1's
`Recipient_List` (`BACnetStack_SetPropertyWritable`; the stack decodes and
stores the list itself), and there is no `BACnetStack_RegisterCallbackSetProperty*`
call anywhere in `main.cpp`. A WriteProperty to any other property of any
object, including the Lift's `Passenger_Alarm` and every elevator-family
property, is refused (write-access-denied); WritePropertyMultiple is rejected
as an unsupported service. The same is true of Schedule, ReinitializeDevice and
TimeSynchronization: none of their services are enabled. Any confirmed service
this table does not list "yes" for is rejected; that rejection is part of the
profile boundary, not a limitation to work around.

## 5. Segmentation capability

Segmentation is **supported in both directions**
(`Segmentation_Supported` = `segmented-both`), by the stack: at `6.x` @
`69b6249f` the customer build compiles `STACK_OPTION_SEGMENTATION` in (stack
#2992), and this example keeps the stack's defaults - `Max_Segments_Accepted`
16 and `APDU_Segment_Timeout` 5000 ms. Table 12-13 footnote 1: "These
properties are required if, and shall be present only if, segmentation of any
kind is supported" - they are not rows in section 11 because its generator
lists only unconditionally required properties. `Max_APDU_Length_Accepted` is
1476 octets, the BACnet/IP maximum. (At the earlier pin `abd4cee1`, this
example answered `no-segmentation`.)

## 6. Standard object types supported

No object is dynamically creatable or deletable, and **no property of any
object is writable** - this device has no `SetPropertyWritable` call anywhere.

| Object type | Instance | Object_Name | Optional properties supported |
|---|:---:|---|---|
| Device | 389015 | Chipkin Example B-EM | Description |
| Analog Input | 1 | Bronze | - |
| Binary Input | 1 | Emerald | - |
| Multi-State Input | 1 | Hot Pink | State_Text |
| Network Port | 1 | Vermilion | - |
| Elevator Group | 1 | Maroon | Group_Mode, Landing_Calls (not Landing_Call_Control: cl. 12.58.10 requires it to be writable if present) |
| Lift | 1 | Mauve | Reliability (FAULT_LISTED), Assigned_Landing_Calls, Registered_Car_Call, Landing_Door_Status; intrinsic reporting (Event_State, Event_Enable, Notification_Class, ...) |
| Escalator | 1 | Mint | Power_Mode, Escalator_Mode |
| Positive Integer Value | 1 | Turquoise | - |
| Notification Class | 1 | Crimson | - |

The device instance is configurable at start-up with `--deviceID`, and the
Device's Object_Name with `--deviceName` (BACnet requires both to be
configurable, cl. 12.1.1.3 and 12.1.1.4). Changing either means restarting
the device; neither is writable over the network. The table shows the
defaults. Positive Integer Value 1
("Turquoise") is not one of the profile's named object types; it is the object
the Elevator Group's `Machine_Room_ID` references, and this application adds
it explicitly before the Elevator Group (see [TUTORIAL.md](../TUTORIAL.md)).

## 7. Data link layer options

**BACnet/IP (Annex J)**, UDP port 47808 (0xBAC0) by default, configurable at
run time with `--port`.

BBMD is not supported, Foreign Device registration is not supported, and
BACnet/SC, MS/TP, Ethernet (Annex H) and PTP are not supported.

The stack's Network Port object still lists its BACnet/IP properties in
`Property_List` - including `IP_DNS_Server`, `FD_BBMD_Address` and
`FD_Subscription_Lifetime` - and every one of them reads as a value
(`FD_BBMD_Address` is a Null host, port 0: no BBMD configured). The Wire Tests
read every listed property of every object.

## 8. Device address binding

Static device binding is **not supported**. The device only executes services
(ReadProperty, ReadPropertyMultiple, SubscribeCOV(-Multiple), Who-Is/Who-Has,
DeviceCommunicationControl, AcknowledgeAlarm, GetEventInformation) and never
initiates a confirmed request that would need a bound peer; its one
initiate-side service, event notification, is sent unconfirmed to the address
form seeded in Notification Class 1, not to a bound device.

## 9. Networking options

None. The device is not a router, not a BBMD, and does not register as a
foreign device.

## 10. Character sets supported

UTF-8 (ANSI X3.4). Supporting a character set does not imply the device can
handle data in all character sets.

## 11. Objects and properties

<!-- OBJECTS-PROPERTIES:BEGIN (generated by tools/gen-objects-properties.py from docs/objects.json - do not edit here) -->
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the CAS BACnet Stack's property profiles), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Device 389015 "Chipkin Example B-EM" - the device itself; the instance is configurable with --deviceID and the Object_Name with --deviceName (both at start-up; the heading shows the defaults). The stack rows are device-wide facts only the stack knows - the protocol version and revision it implements, the services and object types it was configured with, the live object list and address-binding table, and Database_Revision, which the stack starts at 0 and increments for each object added at start-up: it reads 10 (the Device and the nine objects main.cpp adds) and stays 10, because nothing creates, deletes or renames an object afterwards (the car moving, the alarm button and the door fault change values, not the object database). The accepted rows are the stack's configured defaults for APDU limits, segmentation and system status; an application that answered them from its own constants could contradict the stack, so this example does not. The exception is Number_Of_APDU_Retries, served as 3 (the cl. 12.11.29 suggested default): this device sends ConfirmedEventNotifications, and the stack default 0 never resends one a recipient missed. APDU_Timeout stays at the stack's 10000 ms, cl. 12.11.28's default for a device that does not let it be modified

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| System_Status | BACnetDeviceStatus | stack default, accepted (Generic Enumerated default: `0`) | no |
| Vendor_Name | CharacterString | app | no |
| Vendor_Identifier | Unsigned16 | app | no |
| Model_Name | CharacterString | app | no |
| Firmware_Revision | CharacterString | app | no |
| Application_Software_Version | CharacterString | app | no |
| Description *(optional, enabled)* | CharacterString | app | no |
| Protocol_Version | Unsigned | stack | no |
| Protocol_Revision | Unsigned | stack | no |
| Protocol_Services_Supported | BACnetServicesSupported | stack | no |
| Protocol_Object_Types_Supported | BACnetObjectTypesSupported | stack | no |
| Object_List | BACnetARRAY[N] of BACnetObjectIdentifier | stack | no |
| Max_APDU_Length_Accepted | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_MAX_APDU_LENGTH_ACCEPTED`) | no |
| Segmentation_Supported | BACnetSegmentation | stack default, accepted (`BACnetSegmentation::segmentedBoth` with `STACK_OPTION_SEGME) | no |
| APDU_Timeout | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_APDU_TIMEOUT`) | no |
| Number_Of_APDU_Retries | Unsigned | app | no |
| Device_Address_Binding | BACnetLIST of BACnetAddressBinding | stack | no |
| Database_Revision | Unsigned | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Analog Input 1 "Bronze" - REAL, degrees Celsius; starts at 21.5. F-COVM: Present_Value AND Status_Flags are both marked subscribable (SetPropertySubscribable), so a SubscribeCOVPropertyMultiple naming both fires one multi-property notification when the up/down keys nudge this value. COV_Increment 1.0 (required: the object supports COV reporting, cl. 12.2 footnote 2)

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
| COV_Increment *(optional, enabled)* | Real | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Elevator Group 1 "Maroon" - F-ELEVATOR (canonical). Machine_Room_ID, Group_ID and Group_Members are NOT stack DEFAULTS - they are genuinely stored and served by BACnetStack_AddElevatorGroupObject (Group_Members is also maintained by BACnetStack_AddLiftOrEscalatorObject as Lift 1/Mauve is added to this group). They are marked accepted only because the stack's generic per-type property profile does not know about this object-specific host-configuration API and so cannot credit them as stack-served. isGroupOfLifts=true (enables Group_Mode) and supportLandingCallStatus=false (it would also add Landing_Call_Control, which cl. 12.58.10 requires to be writable if present); Landing_Calls is enabled on its own with SetPropertyEnabled (see the Lift's note for how the four F-ELEVATOR list/sequence callbacks are exercised) but the stack's generic Elevator Group property profile does not mark either 'required', so neither generates a checked row here - Group_Mode is served by GetPropertyEnumerated (normal) and Landing_Calls by GetListElevatorGroupLandingCallStatus (no calls outstanding: the callback returns false on its first call, the empty-list answer) regardless

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Machine_Room_ID | BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Group_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Group_Members | BACnetARRAY[N] of BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Lift 1 "Mauve" - F-ELEVATOR (canonical). The Lift presents the car SIMULATION (simulation/ElevatorCar.cpp; README 'What is Simulation'): Car_Position and Car_Moving_Direction via GetPropertyUnsignedInteger/GetPropertyEnumerated, Car_Door_Status (one door, index 1) via GetPropertyEnumerated with useArrayIndex - SAFETY_LOCKED while the car moves, OPENING/OPENED/CLOSING/CLOSED at a stop - Passenger_Alarm and Out_Of_Service via GetPropertyBool, and Fault_Signals (the car's active BACnetLiftFaults: DOOR_CLOSING_FAULT when a door fails to close) via GetListOfEnumerations. Every change goes to the stack with BACnetStack_UpdateValue. The stack's Lift property profile lists those six required properties (with no default: the application must serve them), so the table below shows them; this example serves them because clause 12.59 requires them. Status_Flags, Elevator_Group, Group_ID and Installation_ID ARE in the generic table (stack-computed/stack-stored) and are accepted for the same reason as Maroon's. Event reporting, both algorithms clause 12.59 names: CHANGE_OF_STATE on Passenger_Alarm (pAlarmValues TRUE; the 's' key or the test-only control channel presses the button) and FAULT_LISTED on Fault_Signals, both to Notification Class 1 (Crimson). Reliability is enabled for the fault algorithm and computed by the stack from it (NO_FAULT_DETECTED / FAULTS_LISTED); the app does not serve it. Car_Position, Car_Moving_Direction and Car_Door_Status are SetPropertySubscribable (DS-COVM-B on the car). Optional Assigned_Landing_Calls (always empty: a B-EM's group takes no landing calls), Registered_Car_Call (the car's pending stops) and Landing_Door_Status (floors 1-8: the car's floor follows the car door, the others SAFETY_LOCKED) are enabled and served by the F-ELEVATOR sequence callbacks

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Elevator_Group | BACnetObjectIdentifier | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Group_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Installation_ID | Unsigned8 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Car_Position | Unsigned8 | app | no |
| Car_Moving_Direction | BACnetLiftCarDirection | app | no |
| Car_Door_Status | BACnetARRAY[N] of BACnetDoorStatus | app | no |
| Passenger_Alarm | Boolean | app | no |
| Reliability *(optional, enabled)* | BACnetReliability | stack | no |
| Out_Of_Service | Boolean | app | no |
| Fault_Signals | BACnetLIST of BACnetLiftFault | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Positive Integer Value 1 "Turquoise" - not one of the profile's named objects - added explicitly by this application, BEFORE Elevator Group 1 (Maroon), as the object Maroon's Machine_Room_ID references. BACnetStack_AddElevatorGroupObject's own doc comment reads as though the stack creates this object automatically if missing; verified against the running stack, it does not - AddElevatorGroupObject fails outright ('Create the Positive Integer Value object before adding the Elevator Group object') unless the application adds it first. Colour 'Turquoise' per docs/colour-table.md's global positive_integer_value mapping; Present_Value (required, no stack default) is served as an arbitrary plausible room id. Units defaults to no-units (accepted) since a machine-room identifier has none; Out_Of_Service is optional and left at the generic default

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Units | BACnetEngineeringUnits | stack default, accepted (`BACnetEngineeringUnits::noUnits`) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Notification Class 1 "Crimson" - AE-N-I-B / AE-ACK-B / AE-INFO-B. Priority, Ack_Required and Recipient_List are NOT stack DEFAULTS - they are genuinely populated by BACnetStack_AddNotificationClassObject (Priority, Ack_Required) and BACnetStack_AddRecipientToNotificationClass (Recipient_List) at start-up; marked accepted only because the stack's generic per-type property profile does not know about this object-specific host-configuration API. Recipient_List is registered writable (BACnetStack_SetPropertyWritable), the one writable property of this device, so a commissioning tool chooses who hears the alarms; the stack decodes and stores the write itself. The start-up entry (local subnet broadcast, unconfirmed) is only the default: clause 12.21 fixes the default only for a list that is not writable

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Priority | BACnetARRAY[3] of Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Ack_Required | BACnetEventTransitionBits | stack default, accepted (Generic BitString default: empty bitstring (zero bits - NOT ) | no |
| Recipient_List | BACnetLIST of BACnetDestination | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | yes |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

<!-- OBJECTS-PROPERTIES:END -->

## 12. References

- ANSI/ASHRAE Standard 135-2024, Annex A (PICS template), Annex K (BIBBs),
  Annex L (device profiles), Clause 12 (object types), Clause 13
  (alarm and event services).
- [README.md](../README.md) - what this example is and how to build it.
- [TUTORIAL.md](../TUTORIAL.md) - how to extend it, and how to keep this
  document honest when you do.
