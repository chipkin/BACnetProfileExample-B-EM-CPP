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
> **This device is read-only.** It accepts no WriteProperty,
> WritePropertyMultiple, Schedule, ReinitializeDevice or TimeSynchronization of
> any kind - Section 4 below spells out exactly which services are (and are
> not) enabled.

## 1. Product description

| | |
|---|---|
| **Vendor Name** | Chipkin Automation Systems |
| **Vendor Identifier** | 389 |
| **Product Name** | CAS BACnet Stack Example - B-EM |
| **Product Model Number** | CAS BACnet Stack Example - B-EM |
| **Application Software Version** | 1.0.0 |
| **Firmware Revision** | 1.0.0 |
| **BACnet Protocol Version** | 1 |
| **BACnet Protocol Revision** | 24 |

**Product Description:** a read-only BACnet/IP monitor for an elevator
installation, built on the CAS BACnet Stack. It presents the series' three base
sensor objects plus an Elevator Group / Lift / Escalator object family, answers
ReadProperty and ReadPropertyMultiple for every required property of every
object, supports SubscribeCOV and SubscribeCOVPropertyMultiple, generates
intrinsic ChangeOfState event notifications on the Lift's `Passenger_Alarm`,
accepts AcknowledgeAlarm, answers GetEventInformation, and handles
DeviceCommunicationControl. It accepts **no** WriteProperty,
WritePropertyMultiple, Schedule, ReinitializeDevice or TimeSynchronization. It
is a tutorial for implementers of the B-EM profile.

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
command of any kind.

## 4. Application services supported

| Service | Initiate | Execute |
|---|:---:|:---:|
| ReadProperty | no | **yes** |
| ReadPropertyMultiple | no | **yes** |
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

An I-Am is sent in response to Who-Is (this profile does not require a B-EM to
initiate its own Who-Is on start-up, unlike DM-DDB-A). Event notifications for
the Lift's `Passenger_Alarm` are sent unconfirmed to the recipient seeded in
Notification Class 1 ("Crimson") - by default the local subnet broadcast.

**WriteProperty and WritePropertyMultiple are NOT enabled** -
`BACnetStack_SetServiceEnabled` is never called for either service, and there
is no `BACnetStack_RegisterCallbackSetProperty*` call anywhere in `main.cpp`.
A WriteProperty to any property of any object, including the Lift's
`Passenger_Alarm` and every elevator-family property, is rejected as an
unsupported service. The same is true of Schedule, ReinitializeDevice and
TimeSynchronization: none of their services are enabled. Any confirmed service
this table does not list "yes" for is rejected; that rejection is part of the
profile boundary, not a limitation to work around.

## 5. Segmentation capability

Segmentation is **not supported** in either direction
(`Segmentation_Supported` = `no-segmentation`). `Max_APDU_Length_Accepted` is
1476 octets, the BACnet/IP maximum.

## 6. Standard object types supported

No object is dynamically creatable or deletable, and **no property of any
object is writable** - this device has no `SetPropertyWritable` call anywhere.

| Object type | Instance | Object_Name | Optional properties supported |
|---|:---:|---|---|
| Device | 389015 | Rainbow | Description |
| Analog Input | 1 | Bronze | - |
| Binary Input | 1 | Emerald | - |
| Multi-State Input | 1 | Hot Pink | State_Text |
| Network Port | 1 | Vermilion | - |
| Elevator Group | 1 | Maroon | Group_Mode, LandingCalls |
| Lift | 1 | Mauve | Assigned_Landing_Calls, Registered_Car_Call, Landing_Door_Status |
| Escalator | 1 | Mint | Power_Mode, Escalator_Mode |
| Positive Integer Value | 1 | Turquoise | - |
| Notification Class | 1 | Crimson | - |

The device instance is configurable at run time with `--deviceID` (BACnet
requires the device instance to be configurable). Positive Integer Value 1
("Turquoise") is not one of the profile's named object types; it is the object
the Elevator Group's `Machine_Room_ID` references, and this application adds
it explicitly before the Elevator Group (see [TUTORIAL.md](../TUTORIAL.md)).

## 7. Data link layer options

**BACnet/IP (Annex J)**, UDP port 47808 (0xBAC0) by default, configurable at
run time with `--port`.

BBMD is not supported, Foreign Device registration is not supported, and
BACnet/SC, MS/TP, Ethernet (Annex H) and PTP are not supported.

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
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the stack's `docs/property-profile-reference.md`), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Device 389015 "Rainbow" - the device itself; the instance is configurable with --deviceID (default 389015). The stack rows are device-wide facts only the stack knows - the protocol version and revision it implements, the services and object types it was configured with, the live object list and address-binding table. The accepted rows are the stack's configured defaults for APDU limits, segmentation, system status and database revision; an application that answered them from its own constants could contradict the stack, so this example does not

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
| Segmentation_Supported | BACnetSegmentation | stack default, accepted (`BACnetSegmentation::noSegmentation`) | no |
| APDU_Timeout | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_APDU_TIMEOUT`) | no |
| Number_Of_APDU_Retries | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_NUMBER_OF_APDU_RETRIES`) | no |
| Device_Address_Binding | BACnetLIST of BACnetAddressBinding | stack | no |
| Database_Revision | Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

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
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

<!-- OBJECTS-PROPERTIES:END -->

## 12. References

- ANSI/ASHRAE Standard 135-2024, Annex A (PICS template), Annex K (BIBBs),
  Annex L (device profiles), Clause 12 (object types), Clause 13
  (alarm and event services).
- [README.md](../README.md) - what this example is and how to build it.
- [TUTORIAL.md](../TUTORIAL.md) - how to extend it, and how to keep this
  document honest when you do.
