// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE. The CAS BACnet Stack itself is
// a separate, commercially licensed product and is not covered by CC0.
// =============================================================================
// BACnet Profile Example - B-EM (Elevator Monitor) - C++
//
// This example implements the BACnet "B-EM" (Elevator Monitor) device profile:
// a READ-ONLY view of an elevator installation (a Lift and an Escalator grouped
// under an Elevator Group) with COV and intrinsic alarming. It accepts NO writes
// of any kind - there is no WriteProperty, WritePropertyMultiple, Schedule,
// ReinitializeDevice or TimeSynchronization anywhere in this device, by design:
// a monitor only reports what the elevator installation is doing.
//
// A B-EM (ANSI/ASHRAE 135, Annex L.13) must support:
//
//     DS-RP-B, DS-RPM-B     - ReadProperty + ReadPropertyMultiple,
//     DS-COV-B              - SubscribeCOV (plain, whole-object),
//     DS-COVM-B             - SubscribeCOVPropertyMultiple (COV across several
//                              properties of one or more objects in one request),
//     AE-N-I-B               - generate intrinsic alarm/event notifications,
//     AE-ACK-B                - accept AcknowledgeAlarm,
//     AE-INFO-B                - answer GetEventInformation,
//     DM-DCC-B                - DeviceCommunicationControl,
//     DM-DDB-B, DM-DOB-B      - Who-Is/I-Am (answer only - a B-EM does not
//                              initiate discovery), Who-Has/I-Have.
//
// WHAT IS NOT IMPLEMENTED (see README.md "What this example does NOT do" +
// TODO.md): nothing required by B-EM is missing at this stack pin.
//
// This is the series' CANONICAL example for two shared feature blocks that later
// repos copy byte-for-byte:
//
//     F-COVM      - SubscribeCOVPropertyMultiple: BACnetStack_SetCOVMultipleSettings
//                   plus BACnetStack_SetPropertySubscribable on more than one
//                   property of the same object (here, Analog Input 1's
//                   Present_Value AND Status_Flags).
//     F-ELEVATOR  - the Elevator Group / Lift / Escalator object family:
//                   BACnetStack_AddElevatorGroupObject,
//                   BACnetStack_AddLiftOrEscalatorObject, and the four
//                   elevator-specific "list/sequence" callbacks the stack calls
//                   to page through a Lift's door-indexed and call-related
//                   properties (see section 2b below). B-EC (Wave 3) seeds from
//                   this repo and adds writes back for the lift's commandable
//                   pieces (landing-call control) - keep every comment here
//                   accurate for that reuse.
//
// The device keeps the three base read-only sensors (the series convention) and
// ADDS the elevator object family plus the Notification Class that routes the
// Lift's Passenger_Alarm. Each object has a colour name (the convention shared
// across this example series):
//
//     Device 389015              "Rainbow"      (instance configurable with --deviceID)
//     Analog Input  1            "Bronze"       (REAL, degrees Celsius; read-only)
//     Binary Input  1            "Emerald"      (active / inactive; read-only)
//     Multi-State Input 1        "Hot Pink"     (state 1..3; read-only)
//     Network Port 1             "Vermilion"    (the BACnet/IP port - required)
//     Elevator Group 1           "Maroon"       (groups Mauve; a group of Lifts)
//     Lift 1                     "Mauve"        (car position, doors, alarm; intrinsic alarming)
//     Escalator 1                "Mint"         (not part of the Elevator Group - escalators
//                                                aren't lifts, so it uses the "no reference" sentinel)
//     Positive Integer Value 1   "Turquoise"    (the Elevator Group's Machine_Room_ID target -
//                                                must be added BEFORE the Elevator Group, below)
//     Notification Class 1       "Crimson"      (routes Mauve's Passenger_Alarm events)
//
// ALARMING: Lift 1 ("Mauve") arms an intrinsic ChangeOfState (boolean) algorithm
// on its Passenger_Alarm property. Passenger_Alarm is not writable through this
// (or any) API - it reflects a physical alarm button in the car - so this example
// SIMULATES it: every 30 seconds of uptime it flips a simulated passenger-alarm
// condition and calls BACnetStack_UpdateValue, which lets the stack's own
// algorithm decide whether to fire an EventNotification to Notification Class 1
// ("Crimson")'s recipients. No client write, no interactive key, is needed to see
// it - the whole point of a MONITOR is that it reports what is happening, not
// what a client told it to do.
//
// COVM: Analog Input 1 ("Bronze") has both its Present_Value and Status_Flags
// marked subscribable, so a client's one SubscribeCOVPropertyMultiple request
// naming both properties gets ONE multi-property notification when Bronze's
// value changes (the up/down keys nudge it, exactly as in every other example).
//
// To be a conformant BACnet device (Protocol_Revision 24) each object must
// expose its full set of REQUIRED properties. Most are generated by the stack
// (Object_Identifier, Object_Type, Status_Flags, Object_List, Protocol_*). The
// elevator objects are unusual: the stack stores only a handful of their
// properties (see the comment above BACnetStack_AddElevatorGroupObject /
// BACnetStack_AddLiftOrEscalatorObject in CASBACnetStackDLL.h) - EVERYTHING else,
// including properties this device profile treats as required (Car_Position,
// Car_Moving_Direction, Car_Door_Status, Passenger_Alarm, Out_Of_Service,
// Fault_Signals on the Lift; Operation_Direction, Passenger_Alarm, Out_Of_Service
// on the Escalator), must come from this file's Get* callbacks. Skip one and the
// stack does NOT error loudly: Car_Position/Car_Moving_Direction/Car_Door_Status/
// Passenger_Alarm on a Lift (and Operation_Direction/Passenger_Alarm on an
// Escalator) are the one place the stack DOES refuse to invent a value
// (value-not-initialized) - but Out_Of_Service silently reports false and
// Object_Name silently reports "undefined" if you forget it, exactly the
// duplicate-name trap the B-AAC example's callback comments warn about. Walk
// every callback below when you add an object, and diff its required properties
// against a working one.
//
// Interactive keys (handled by the shared helper): h = help, q = quit,
// up/down = nudge Analog Input 1 by +/-1.1 (also the F-COVM demonstration: watch
// a COVPropertyMultiple subscription fire on Present_Value AND Status_Flags
// together). There is no key to fire the Lift's Passenger_Alarm alarm - it is
// simulated on a 30-second timer instead, precisely because this device accepts
// no commands of any kind. Command line: --port <n>, --deviceID <n>.
//
// All the UDP/stack plumbing lives in common/CASExampleHelper so this file can
// stay focused on the BACnet logic.
// =============================================================================

#include "CASExampleHelper.h"
#include "CASBACnetStackExampleConstants.h"
#include "CASBACnetStackAdapter.h" // the CAS BACnet Stack C API (BACnetStack_*); call
                                    // LoadBACnetFunctions() before any BACnetStack_* call -
                                    // see the top of main() below.

#include <stdio.h>
#include <string.h>
#include <string>
#include <time.h> // time() - the Passenger_Alarm simulation timer

#if defined(_WIN32)
#include <windows.h> // Sleep()
#else
#include <unistd.h>  // usleep()
#endif

using namespace CASBACnetStackExampleConstants;

// -----------------------------------------------------------------------------
// 1. Example + device configuration
// -----------------------------------------------------------------------------
static const char* APP_NAME = "BACnet B-EM (Elevator Monitor) Example - C++";
static const char* APP_VERSION = "1.0.1";

// The device instance. BACnet requires this to be configurable, so it defaults
// to 389015 and can be overridden on the command line with --deviceID. Keep it
// configurable in your product: it must be unique across the internetwork.
static uint32_t g_deviceInstance = 389015;

// ---- Device identity: CHANGE ALL OF THIS BEFORE YOU SHIP --------------------
// Everything in this block is read by clients and shown to the operator in every
// discovery tool on the network. Left as-is, your product will appear on a real
// site announcing itself as a Chipkin demo. None of it is cosmetic:
// Object_Name must be unique across the BACnet internetwork, and Model_Name /
// Vendor_Identifier are what a building operator uses to identify your device.
// -----------------------------------------------------------------------------

// Your BACnet Vendor Identifier. 389 = Chipkin Automation Systems; change this
// to YOUR company's vendor ID before shipping a product. Vendor IDs are assigned
// by ASHRAE - request one (free) at https://bacnet.org/assigned-vendor-ids/.
// Update VENDOR_NAME below to match.
static const uint32_t VENDOR_IDENTIFIER = 389;

// The Device object's Object_Name.
//
// THIS IS THE ONE THAT WILL BITE YOU. Object_Name must be unique across the
// whole BACnet internetwork, and here it is a COMPILE-TIME constant. The
// device instance is runtime-configurable via --deviceID (see g_deviceInstance
// above), so it is easy to ship two units, configure their instances
// correctly, and still have BOTH announce Object_Name "Rainbow" - a spec
// violation, and a hard BTL failure. In a real product Object_Name must be
// per-unit configurable too: derive it from a serial number, DIP switches, a
// config file, or add a --deviceName argument.
static const char* DEVICE_NAME = "Rainbow";

// The Device object's Description. Change it to what YOUR device actually is;
// this string describes this tutorial.
static const char* DEVICE_DESCRIPTION =
    "Chipkin CAS BACnet Stack example - B-EM (Elevator Monitor) profile. "
    "Read-only: DS-RP/RPM-B, DS-COV-B, DS-COVM-B, intrinsic alarming "
    "(AE-N-I-B / AE-ACK-B / AE-INFO-B), DM-DCC-B. Accepts no WriteProperty.";

// Device identity strings (read by clients, and used to populate I-Am).
//   VENDOR_NAME - your company name; it must match VENDOR_IDENTIFIER above.
//   MODEL_NAME  - your model designation. This is what a building operator
//                 reads to identify your device in a discovery tool.
static const char* VENDOR_NAME = "Chipkin Automation Systems";
static const char* MODEL_NAME = "CAS BACnet Stack Example - B-EM";

// DeviceCommunicationControl password. A management station may include a password
// with a DeviceCommunicationControl request; the device accepts the command only if
// it matches. Set to NULL/empty to accept any request (no password required). It
// crosses the wire in PLAINTEXT - a guard against accidents, not a security
// boundary. Change this to your device's secret before shipping.
static const char* DCC_PASSWORD = "";  // "" = no password required

// Application_Software_Version (12) is just APP_VERSION - one source of
// truth, so it can never drift from what --version/the startup banner
// prints (it used to be a separate hardcoded "1.0.0" constant that nobody
// updated).
//
// Firmware_Revision (44) is meant to name the underlying platform/stack,
// not this example's own version - built at runtime from the CAS BACnet
// Stack's own BACnetStack_GetAPIMajorVersion()/etc. (the same 4 calls
// common/CASExampleHelper.cpp's PrintVersion() already uses for the
// startup banner's "CAS BACnet Stack version: X.Y.Z.W" line), so it can
// never go stale either - see g_firmwareRevision below, populated once
// right after LoadBACnetFunctions() succeeds (those functions are what
// the version getters themselves are, so they must be loaded first).
static std::string g_firmwareRevision;

// The base sensor objects (all instance 1) and their colour names. Every example
// in the series carries these three plus the Network Port - see docs/colour-table.md.
static const uint32_t ANALOG_INPUT_INSTANCE = 1;       // "Bronze"
static const uint32_t BINARY_INPUT_INSTANCE = 1;       // "Emerald"
static const uint32_t MULTI_STATE_INPUT_INSTANCE = 1;  // "Hot Pink"
static const uint32_t MULTI_STATE_INPUT_NUMBER_OF_STATES = 3;

// The Network Port object - every BACnet device must have one. It represents
// the BACnet/IP port this device communicates on.
static const uint32_t NETWORK_PORT_INSTANCE = 1;       // "Vermilion"
static const uint32_t MAX_APDU_LENGTH = 1476;          // BACnet/IP APDU length

// BACnet/IP addressing the Network Port reports. The IP address and subnet mask
// are filled in at start-up from the host's primary interface; the gateway is
// left unset (0.0.0.0) for this example. The stack also uses IP_Address +
// BACnet_IP_UDP_Port to build the port's MAC_Address automatically.
static uint8_t g_ipAddress[4] = { 0, 0, 0, 0 };
static uint8_t g_ipSubnetMask[4] = { 0, 0, 0, 0 };
static uint8_t g_ipDefaultGateway[4] = { 0, 0, 0, 0 };
static uint16_t g_bacnetIpUdpPort = 47808;

// Analog Input 1's live present value (degrees Celsius). Starts at 21.5 and is
// nudged by the up/down arrow keys - this is also the F-COVM demonstration
// value: a SubscribeCOVPropertyMultiple naming Present_Value AND Status_Flags
// fires one multi-property notification when this changes.
static float g_analogInput1Value = 21.5f;

// -----------------------------------------------------------------------------
// F-ELEVATOR: object-type / property-identifier numbers not already in
// CASBACnetStackExampleConstants.h (verified against BACnetObjectType.h /
// BACnetPropertyIdentifier.h at the pin abd4cee1, 6.0.21).
// -----------------------------------------------------------------------------
static const uint16_t OBJECT_TYPE_ELEVATOR_GROUP = 57;
static const uint16_t OBJECT_TYPE_ESCALATOR = 58;
static const uint16_t OBJECT_TYPE_LIFT = 59;
static const uint16_t OBJECT_TYPE_POSITIVE_INTEGER_VALUE = 48;

static const uint32_t PROPERTY_IDENTIFIER_ASSIGNED_LANDING_CALLS = 447;
static const uint32_t PROPERTY_IDENTIFIER_CAR_DOOR_STATUS = 450;
static const uint32_t PROPERTY_IDENTIFIER_CAR_MOVING_DIRECTION = 457;
static const uint32_t PROPERTY_IDENTIFIER_CAR_POSITION = 458;
static const uint32_t PROPERTY_IDENTIFIER_ESCALATOR_MODE = 462;
static const uint32_t PROPERTY_IDENTIFIER_FAULT_SIGNALS = 463;
static const uint32_t PROPERTY_IDENTIFIER_GROUP_MODE = 467;
static const uint32_t PROPERTY_IDENTIFIER_LANDING_DOOR_STATUS = 472;
static const uint32_t PROPERTY_IDENTIFIER_OPERATION_DIRECTION = 477;
static const uint32_t PROPERTY_IDENTIFIER_PASSENGER_ALARM = 478;
static const uint32_t PROPERTY_IDENTIFIER_POWER_MODE = 479;
static const uint32_t PROPERTY_IDENTIFIER_REGISTERED_CAR_CALL = 480;

// BACnetLiftCarDirectionEnum (BACnetLiftCarDirection.h)
static const uint32_t LIFT_CAR_DIRECTION_UP = 3;
// BACnetDoorStatusEnum (BACnetDoorStatus.h) - shared with Access Door / the Lift's
// per-door Car_Door_Status and Landing_Door_Status arrays.
static const uint32_t DOOR_STATUS_CLOSED = 0;
// BACnetLiftGroupModeEnum (BACnetLiftGroupMode.h)
static const uint32_t LIFT_GROUP_MODE_NORMAL = 1;
// BACnetEscalatorOperationDirectionEnum (BACnetEscalatorOperationDirection.h)
static const uint32_t ESCALATOR_OPERATION_DIRECTION_UP_RATED_SPEED = 2;
// BACnetEscalatorModeEnum (BACnetEscalatorMode.h)
static const uint32_t ESCALATOR_MODE_UP = 2;

// --- F-ELEVATOR: Elevator Group 1 "Maroon", Lift 1 "Mauve", Escalator 1 "Mint" -
// Maroon groups Mauve (isGroupOfLifts = true); Mint is NOT part of an Elevator
// Group of Lifts, so it is added with the spec's "no reference" sentinel
// (4194303, NETWORK_PORT_REFERENCE_PORT_NONE reused - the same constant serves
// both "no reference" sentinels in the stack API).
static const uint32_t ELEVATOR_GROUP_INSTANCE = 1;      // "Maroon"
static const uint32_t LIFT_INSTANCE = 1;                // "Mauve"
static const uint32_t ESCALATOR_INSTANCE = 1;            // "Mint"
static const uint8_t ELEVATOR_GROUP_ID = 1;               // shared by Maroon and Mauve
static const uint8_t LIFT_INSTALLATION_ID = 1;
static const uint8_t ESCALATOR_GROUP_ID = 1;               // Mint is not grouped, but still carries an ID
static const uint8_t ESCALATOR_INSTALLATION_ID = 1;
// BACnetStack_AddElevatorGroupObject's machineRoomId names a Positive Integer
// Value object (instance 1, colour "Turquoise" per docs/colour-table.md) that
// THIS APPLICATION must add first - see the AddElevatorGroupObject call below
// for why its own doc comment's "the stack will create it" is not what happens.
static const uint32_t MACHINE_ROOM_ID_INSTANCE = 1;      // "Turquoise"
static const uint32_t MACHINE_ROOM_ID_VALUE = 100;        // an arbitrary plausible room id
static const uint32_t LIFT_NUMBER_OF_DOORS = 1;            // this Lift has one door (index 1)

// Simulated Lift state (a real device would read this from the elevator
// controller's own bus). Car_Position is a floor number (1-based); the car sits
// at floor 3, doors closed, not moving.
static const uint8_t LIFT_CAR_POSITION = 3;
static bool g_liftOutOfService = false;
// The alarm source: Passenger_Alarm is not writable through any API in this
// stack (a real installation drives it from hardware) - see the file header
// comment for how this example simulates it on a timer instead of a key/write.
static bool g_liftPassengerAlarm = false;

static bool g_escalatorOutOfService = false;
static bool g_escalatorPassengerAlarm = false;
static bool g_escalatorPowerMode = true; // powered on

// --- AE-N-I-B: Notification Class 1 "Crimson" routes Mauve's Passenger_Alarm --
static const uint32_t NOTIFICATION_CLASS_INSTANCE = 1;     // "Crimson"
// Notification priorities for the three transitions (lower = more urgent). The
// to-fault priority is supplied for completeness, but this example's
// ChangeOfState algorithm has no fault source, so to-fault is left disabled below.
static const uint8_t NC_PRIORITY_TO_OFFNORMAL = 100;
static const uint8_t NC_PRIORITY_TO_FAULT = 50;
static const uint8_t NC_PRIORITY_TO_NORMAL = 200;

// Where Mauve's alarms are sent. See B-AAC's main.cpp for the full rationale -
// this example seeds the ADDRESS form, defaulting to the LOCAL SUBNET BROADCAST
// with UNCONFIRMED notifications, so any BACnet client on the subnet sees the
// alarm without us knowing its address ahead of time. Unlike B-AAC (AE-CRL-B),
// this profile does NOT make Recipient_List writable - B-EM has no writable
// property anywhere - so the recipient seeded here is the only one, for the life
// of the process.
static const uint32_t RECIPIENT_PROCESS_IDENTIFIER = 1;
static const bool RECIPIENT_USE_BROADCAST = true;
static uint8_t RECIPIENT_IP[4] = { 0, 0, 0, 0 };  // used when not broadcasting

// How often (seconds of uptime) the simulated Passenger_Alarm condition flips.
// A real device would drive g_liftPassengerAlarm from an actual alarm input.
static const time_t PASSENGER_ALARM_SIMULATION_PERIOD_SECONDS = 30;

// -----------------------------------------------------------------------------
// 2. Property "get" callbacks
//
// The stack calls these when a client reads a property. For each data type the
// stack uses a separate callback. We return true (and fill *value) when we
// recognise the (object, property) pair, and false otherwise.
//
// THE errorCode OUT-PARAMETER. Every Get callback ends with uint32_t* errorCode.
// The stack PRESETS it to success (84) before the call, and reads it only if you
// return false. That gives a declining callback two distinct meanings:
//
//   1. return false and LEAVE errorCode ALONE  -> "I have no opinion on this
//      property." The stack falls back to its own handling (see below).
//   2. return false and SET *errorCode         -> "This read fails, with THIS
//      BACnet error." The client gets exactly that Error-PDU.
//
// WHAT false-WITHOUT-AN-ERROR-CODE ACTUALLY DOES. It does NOT reliably produce a
// BACnet error. The stack errors only for the handful of properties it refuses
// to invent - on this device that includes Car_Position, Car_Moving_Direction,
// Car_Door_Status and Passenger_Alarm on the Lift, and Operation_Direction and
// Passenger_Alarm on the Escalator (value-not-initialized), on top of the usual
// Present_Value / Number_Of_States / Relinquish_Default / Local_Date / Local_Time
// / a Network Port's APDU_Length. For EVERYTHING ELSE a false return means the
// stack SILENTLY SUBSTITUTES a default (Object_Name -> "undefined", Units ->
// no-units, otherwise a datatype zero-value) - load-bearing for the required
// properties this application does not serve, so it is not "fixed" here. Set
// *errorCode ONLY where this device knows the read is wrong; the one such case
// below is State_Text with an out-of-range array index.
//
// ADDING AN OBJECT? READ THIS FIRST. These callbacks are not uniformly strict -
// see the file header comment above for the duplicate-Object_Name trap. Walk
// every callback below when you add an instance, then read back every required
// property of the new object and diff it against a working one.
// -----------------------------------------------------------------------------

// REAL (floating point) - the Analog Input's Present_Value.
bool GetPropertyReal(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     float* value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode; // see "THE errorCode OUT-PARAMETER" above: every catch-all here declines without naming an error
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_ANALOG_INPUT &&
        objectInstance == ANALOG_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // ON REAL HARDWARE: return the live sensor reading here. Read it from a
        // cached variable that your hardware updates (as g_analogInput1Value is),
        // NOT directly from a slow/blocking device: this callback runs on the
        // BACnetStack_Tick() thread, so blocking it delays all BACnet processing.
        *value = g_analogInput1Value;
        return true;
    }
    return false;
}

// ENUMERATED - the Binary Input's Present_Value, the Analog Input's Units, and
// every elevator-family enumerated property (Car_Moving_Direction, Operation_Direction,
// Group_Mode, Escalator_Mode, Car_Door_Status).
bool GetPropertyEnumerated(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           uint32_t* value, const bool useArrayIndex,
                           const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_BINARY_INPUT &&
        objectInstance == BINARY_INPUT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
            *value = 1; // active
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POLARITY) {
            *value = POLARITY_NORMAL; // required property of a Binary Input
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_ANALOG_INPUT &&
        objectInstance == ANALOG_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_UNITS) {
        *value = ENGINEERING_UNITS_DEGREES_CELSIUS;
        return true;
    }
    if (objectType == OBJECT_TYPE_NETWORK_PORT &&
        objectInstance == NETWORK_PORT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_BACNET_IP_MODE) {
        *value = BACNET_IP_MODE_NORMAL; // not foreign-device, not BBMD
        return true;
    }

    // --- F-ELEVATOR ----------------------------------------------------------
    if (objectType == OBJECT_TYPE_ELEVATOR_GROUP && objectInstance == ELEVATOR_GROUP_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_GROUP_MODE) {
        // Enabled automatically because isGroupOfLifts=true was passed to
        // BACnetStack_AddElevatorGroupObject (required only in that case).
        *value = LIFT_GROUP_MODE_NORMAL;
        return true;
    }
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_CAR_MOVING_DIRECTION) {
            // Required, NO STACK DEFAULT - an unserved read fails value-not-initialized.
            *value = LIFT_CAR_DIRECTION_UP;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_CAR_DOOR_STATUS &&
            useArrayIndex && propertyArrayIndex >= 1 && propertyArrayIndex <= LIFT_NUMBER_OF_DOORS) {
            // Required, per-door array (this Lift has one door - see the
            // "THE DOOR COUNT IS NOT STORED BY THE STACK" note in
            // CASBACnetStackDLL.h: the application alone owns the door count,
            // by simply answering only door indices it recognises).
            *value = DOOR_STATUS_CLOSED;
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_ESCALATOR && objectInstance == ESCALATOR_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_OPERATION_DIRECTION) {
            // Required, NO STACK DEFAULT.
            *value = ESCALATOR_OPERATION_DIRECTION_UP_RATED_SPEED;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_ESCALATOR_MODE) {
            // Optional; enabled below with SetPropertyEnabled.
            *value = ESCALATOR_MODE_UP;
            return true;
        }
    }
    return false;
}

// UNSIGNED INTEGER - the Multi-State Input's Present_Value, the Device's
// Vendor_Identifier, the Lift's Car_Position (and its door-indexed lists), and
// the machine-room Positive Integer Value's Present_Value.
bool GetPropertyUnsignedInteger(const uint32_t deviceInstance, const uint16_t objectType,
                                const uint32_t objectInstance, const uint32_t propertyIdentifier,
                                uint32_t* value, const bool useArrayIndex,
                                const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT &&
        objectInstance == MULTI_STATE_INPUT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
            *value = 1; // state 1 (valid range is 1..Number_Of_States)
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_STATES) {
            *value = MULTI_STATE_INPUT_NUMBER_OF_STATES; // required property
            return true;
        }
        // State_Text is an array. The stack asks for its LENGTH here (array
        // index 0) before reading each element via GetPropertyCharString.
        if (propertyIdentifier == PROPERTY_IDENTIFIER_STATE_TEXT &&
            useArrayIndex && propertyArrayIndex == 0) {
            *value = MULTI_STATE_INPUT_NUMBER_OF_STATES;
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance &&
        propertyIdentifier == PROPERTY_IDENTIFIER_VENDOR_IDENTIFIER) {
        *value = VENDOR_IDENTIFIER;
        return true;
    }
    if (objectType == OBJECT_TYPE_NETWORK_PORT && objectInstance == NETWORK_PORT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_APDU_LENGTH) {
            *value = MAX_APDU_LENGTH;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_REFERENCE_PORT) {
            *value = NETWORK_PORT_REFERENCE_PORT_NONE;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_BACNET_IP_UDP_PORT) {
            *value = g_bacnetIpUdpPort;
            return true;
        }
    }

    // --- F-ELEVATOR ----------------------------------------------------------
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_CAR_POSITION) {
        // Required, NO STACK DEFAULT.
        *value = LIFT_CAR_POSITION;
        return true;
    }
    // Every one of the Lift's per-door BACnetARRAY properties (required
    // Car_Door_Status, optional Assigned_Landing_Calls/Registered_Car_Call/
    // Landing_Door_Status) is asked for its LENGTH here first (array index 0,
    // exactly like Multi-State Input 1's State_Text above) before the stack
    // reads/pages the individual elements - through GetPropertyEnumerated for
    // Car_Door_Status, or through the matching
    // GetSequenceLift*/GetListElevatorGroupLandingCallStatus callback for the
    // other three. Skipping this made every one of those reads fail with
    // BuildArrayProperty "failed to get the size of the array" - caught by
    // testing against a real client, not by reading the doc comment, which
    // does not mention this length-query step at all for these properties.
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE &&
        useArrayIndex && propertyArrayIndex == 0 &&
        (propertyIdentifier == PROPERTY_IDENTIFIER_CAR_DOOR_STATUS ||
         propertyIdentifier == PROPERTY_IDENTIFIER_ASSIGNED_LANDING_CALLS ||
         propertyIdentifier == PROPERTY_IDENTIFIER_REGISTERED_CAR_CALL ||
         propertyIdentifier == PROPERTY_IDENTIFIER_LANDING_DOOR_STATUS)) {
        *value = LIFT_NUMBER_OF_DOORS;
        return true;
    }
    // The Positive Integer Value the Elevator Group's Machine_Room_ID names
    // (added explicitly by this application - see the AddObject call right
    // before AddElevatorGroupObject below). Present_Value is required with no
    // stack default; must be served or the read fails.
    if (objectType == OBJECT_TYPE_POSITIVE_INTEGER_VALUE &&
        objectInstance == MACHINE_ROOM_ID_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        *value = MACHINE_ROOM_ID_VALUE;
        return true;
    }
    return false;
}

// BOOLEAN - Out_Of_Service is a required property of every input object, the
// Network Port, the Lift and the Escalator; Passenger_Alarm (required, no stack
// default) on the Lift and Escalator; Power_Mode (optional) on the Escalator.
bool GetPropertyBool(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     bool* value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }

    // --- F-ELEVATOR ----------------------------------------------------------
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PASSENGER_ALARM) {
            // Required, NO STACK DEFAULT. This is the alarm source (see the file
            // header comment) - simulated on a timer, never written.
            *value = g_liftPassengerAlarm;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_OUT_OF_SERVICE) {
            *value = g_liftOutOfService;
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_ESCALATOR && objectInstance == ESCALATOR_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PASSENGER_ALARM) {
            *value = g_escalatorPassengerAlarm;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_OUT_OF_SERVICE) {
            *value = g_escalatorOutOfService;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POWER_MODE) {
            // Optional; enabled below with SetPropertyEnabled.
            *value = g_escalatorPowerMode;
            return true;
        }
    }

    // Out_Of_Service is a required property of every input object and of the
    // Network Port too. This example never takes anything out of service: false.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_OUT_OF_SERVICE &&
        (objectType == OBJECT_TYPE_ANALOG_INPUT ||
         objectType == OBJECT_TYPE_BINARY_INPUT ||
         objectType == OBJECT_TYPE_MULTI_STATE_INPUT ||
         objectType == OBJECT_TYPE_NETWORK_PORT)) {
        *value = false;
        return true;
    }
    return false;
}

// OCTET STRING - the Network Port's BACnet/IP addressing. The stack cannot know
// the host's IP, so the application must supply IP_Address and IP_Subnet_Mask
// (and IP_Default_Gateway). Each is four octets.
bool GetPropertyOctetString(const uint32_t deviceInstance, const uint16_t objectType,
                            const uint32_t objectInstance, const uint32_t propertyIdentifier,
                            uint8_t* value, uint32_t* valueElementCount,
                            const uint32_t maxElementCount, const bool useArrayIndex,
                            const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)errorCode;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance ||
        objectType != OBJECT_TYPE_NETWORK_PORT ||
        objectInstance != NETWORK_PORT_INSTANCE ||
        maxElementCount < 4) {
        return false;
    }
    const uint8_t* source = NULL;
    switch (propertyIdentifier) {
        case PROPERTY_IDENTIFIER_IP_ADDRESS:         source = g_ipAddress; break;
        case PROPERTY_IDENTIFIER_IP_SUBNET_MASK:     source = g_ipSubnetMask; break;
        case PROPERTY_IDENTIFIER_IP_DEFAULT_GATEWAY: source = g_ipDefaultGateway; break;
        default: return false;
    }
    memcpy(value, source, 4);
    *valueElementCount = 4;
    return true;
}

// Small helper: copy a C string into the stack's character-string buffer and
// set the element count + encoding. Returns true (so callers can `return`).
static bool ReturnCharacterString(const char* text, char* value,
                                  uint32_t* valueElementCount,
                                  const uint32_t maxElementCount,
                                  uint8_t* encodingType) {
    uint32_t length = (uint32_t)strlen(text);
    if (length > maxElementCount) {
        // Truncate SILENTLY to fit the stack's buffer - see B-AAC's main.cpp for
        // the full explanation of when this matters (STACK_OPTION_TARGET_EMBEDDED).
        length = maxElementCount;
    }
    memcpy(value, text, length);
    *valueElementCount = length;
    *encodingType = CHARACTER_STRING_ENCODING_UTF8;
    return true;
}

// CHARACTER STRING - Object_Name for each object, and the device Description.
bool GetPropertyCharString(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           char* value, uint32_t* valueElementCount,
                           const uint32_t maxElementCount, uint8_t* encodingType,
                           const bool useArrayIndex, const uint32_t propertyArrayIndex,
                           uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }

    // State_Text (optional) - one label per state of the Multi-State Input. It is
    // a BACnet array, so the stack asks for one element at a time by index
    // (1..Number_Of_States). Present_Value 1 -> "On", 2 -> "Off", 3 -> "Auto".
    if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT &&
        objectInstance == MULTI_STATE_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_STATE_TEXT && useArrayIndex) {
        static const char* const stateText[] = { "On", "Off", "Auto" };
        if (propertyArrayIndex >= 1 && propertyArrayIndex <= MULTI_STATE_INPUT_NUMBER_OF_STATES) {
            return ReturnCharacterString(stateText[propertyArrayIndex - 1], value,
                                         valueElementCount, maxElementCount, encodingType);
        }
        // The one place in this file where naming an error is clearly right: the
        // client asked for State_Text[n] and this object has no element n.
        *errorCode = ERROR_CODE_INVALID_ARRAY_INDEX;
        return false;
    }

    // Object_Name - the colour name for each object.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_OBJECT_NAME) {
        if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance) {
            return ReturnCharacterString(DEVICE_NAME, value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ANALOG_INPUT && objectInstance == ANALOG_INPUT_INSTANCE) {
            return ReturnCharacterString("Bronze", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_BINARY_INPUT && objectInstance == BINARY_INPUT_INSTANCE) {
            return ReturnCharacterString("Emerald", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT && objectInstance == MULTI_STATE_INPUT_INSTANCE) {
            return ReturnCharacterString("Hot Pink", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_NETWORK_PORT && objectInstance == NETWORK_PORT_INSTANCE) {
            return ReturnCharacterString("Vermilion", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ELEVATOR_GROUP && objectInstance == ELEVATOR_GROUP_INSTANCE) {
            return ReturnCharacterString("Maroon", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE) {
            return ReturnCharacterString("Mauve", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ESCALATOR && objectInstance == ESCALATOR_INSTANCE) {
            return ReturnCharacterString("Mint", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_POSITIVE_INTEGER_VALUE && objectInstance == MACHINE_ROOM_ID_INSTANCE) {
            return ReturnCharacterString("Turquoise", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_NOTIFICATION_CLASS && objectInstance == NOTIFICATION_CLASS_INSTANCE) {
            return ReturnCharacterString("Crimson", value, valueElementCount, maxElementCount, encodingType);
        }
    }

    // The remaining strings are all on the Device object - its identity, read
    // by clients and used to populate the device's I-Am / object list.
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance) {
        switch (propertyIdentifier) {
            case PROPERTY_IDENTIFIER_DESCRIPTION:
                return ReturnCharacterString(DEVICE_DESCRIPTION, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_VENDOR_NAME:
                return ReturnCharacterString(VENDOR_NAME, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_MODEL_NAME:
                return ReturnCharacterString(MODEL_NAME, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_FIRMWARE_REVISION:
                return ReturnCharacterString(g_firmwareRevision.c_str(), value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_APPLICATION_SOFTWARE_VERSION:
                return ReturnCharacterString(APP_VERSION, value, valueElementCount, maxElementCount, encodingType);
            default:
                break;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// 2b. F-ELEVATOR list/sequence callbacks
//
// These are the four elevator-specific callbacks the stack calls to page
// through a property that is a LIST or a per-door ARRAY rather than a plain
// typed value - one call per element, walking rangeOption/offset/more until the
// application says there is nothing left. A monitor with nothing outstanding
// answers the first call with *more = false (an empty list is a completely
// legitimate, honest answer - it means "no calls right now").
//
// This device is READ-ONLY, so the FIFTH elevator callback,
// BACnetStack_RegisterCallbackSetElevatorGroupLandingCallControl - the one that
// would accept a client's landing-call command - is DELIBERATELY NOT registered.
// Leaving it unregistered (rather than registering a callback that rejects
// everything) is itself part of the "no write" guarantee: the stack has no
// registered receiver for that command, so LandingCallControl is never made
// writable to begin with. B-EC (Wave 3), which seeds from this repo, is where
// that callback gets wired up.
// -----------------------------------------------------------------------------

// FaultSignals (required on the Lift) - a list of active fault enumerations.
// A healthy Lift reports an empty list; this example never simulates a fault.
bool GetListOfEnumerations(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           const uint8_t rangeOption, const uint32_t rangeIndexOrSequenceNumber,
                           const bool rangeInPositiveDirection, uint32_t* enumeration, bool* more) {
    (void)rangeOption;
    (void)rangeIndexOrSequenceNumber;
    (void)rangeInPositiveDirection;
    (void)enumeration;
    if (deviceInstance != g_deviceInstance) {
        *more = false;
        return false;
    }
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_FAULT_SIGNALS) {
        *more = false; // no active faults
        return true;
    }
    *more = false;
    return false;
}

// LandingCalls (optional; enabled on Maroon because supportLandingCallStatus was
// passed to AddElevatorGroupObject) - the group's current landing-call queue.
// This monitor has none outstanding.
bool GetListElevatorGroupLandingCallStatus(const uint32_t deviceInstance, const uint32_t elevatorGroupInstance,
                                           const uint32_t propertyIdentifier, const uint8_t rangeOption,
                                           const uint32_t rangeIndexOrSequence, uint8_t* floorNumber,
                                           uint8_t* commandChoice, uint32_t* bacnetLiftCarDirection,
                                           uint8_t* destination, bool* useFloorText, char* floorText,
                                           uint32_t* floorTextLength, const uint32_t floorTextMaxLength,
                                           bool* more) {
    (void)propertyIdentifier;
    (void)rangeOption;
    (void)rangeIndexOrSequence;
    (void)floorNumber;
    (void)commandChoice;
    (void)bacnetLiftCarDirection;
    (void)destination;
    (void)useFloorText;
    (void)floorText;
    (void)floorTextLength;
    (void)floorTextMaxLength;
    if (deviceInstance != g_deviceInstance || elevatorGroupInstance != ELEVATOR_GROUP_INSTANCE) {
        *more = false;
        return false;
    }
    *more = false; // no landing calls outstanding right now
    return true;
}

// AssignedLandingCalls (optional Lift property, enabled below) - the calls this
// car has been assigned to answer. Report one, for demonstration: the car is
// assigned to floor 5, direction up.
bool GetSequenceLiftAssignedLandingCall(const uint32_t deviceInstance, const uint32_t objectInstance,
                                        const uint32_t arrayIndexForDoor, const uint32_t offset,
                                        uint8_t* floorNumber, uint32_t* direction, bool* more) {
    if (deviceInstance != g_deviceInstance || objectInstance != LIFT_INSTANCE ||
        arrayIndexForDoor != 1 /* this Lift's one door */) {
        *more = false;
        return false;
    }
    if (offset == 0) {
        *floorNumber = 5;
        *direction = LIFT_CAR_DIRECTION_UP;
        *more = false; // exactly one assigned call
        return true;
    }
    *more = false;
    return false;
}

// RegisteredCarCall (optional Lift property, enabled below) - the floor(s) a
// passenger has pressed inside the car. Report one, for demonstration: floor 5.
bool GetSequenceLiftRegisteredCarCall(const uint32_t deviceInstance, const uint32_t objectInstance,
                                      const uint32_t arrayIndexForDoor, const uint32_t offset,
                                      uint8_t* floorNumber, bool* more) {
    if (deviceInstance != g_deviceInstance || objectInstance != LIFT_INSTANCE ||
        arrayIndexForDoor != 1) {
        *more = false;
        return false;
    }
    if (offset == 0) {
        *floorNumber = 5;
        *more = false; // exactly one registered car call
        return true;
    }
    *more = false;
    return false;
}

// LandingDoorStatus (optional Lift property, enabled below) - the door status at
// each floor the car serves. Report one, for demonstration: floor 3 (where the
// car currently sits), door closed.
bool GetSequenceLiftLandingDoorStatus(const uint32_t deviceInstance, const uint32_t objectInstance,
                                      const uint32_t arrayIndexForDoor, const uint32_t offset,
                                      uint8_t* floorNumber, uint32_t* doorStatus, bool* more) {
    if (deviceInstance != g_deviceInstance || objectInstance != LIFT_INSTANCE ||
        arrayIndexForDoor != 1) {
        *more = false;
        return false;
    }
    if (offset == 0) {
        *floorNumber = LIFT_CAR_POSITION;
        *doorStatus = DOOR_STATUS_CLOSED;
        *more = false; // exactly one landing-door status entry reported
        return true;
    }
    *more = false;
    return false;
}

// -----------------------------------------------------------------------------
// 2c. DeviceCommunicationControl callback (DM-DCC-B)
//
// A management station sends DeviceCommunicationControl to tell a device to stop
// or resume communicating. This is NOT a write to any BACnet object property -
// it is a device-management service every profile above B-ASC must support, and
// it does not conflict with "this device accepts no WriteProperty". The stack
// runs the actual enable/disable state machine; this callback validates the
// optional password and logs. See B-AAC's main.cpp for the full explanation of
// the errorCode contract on this callback (it has NO fallback - every false
// return here MUST set *errorCode) and the Protocol_Revision >= 20 deprecation
// of plain "disable".
// -----------------------------------------------------------------------------
bool DeviceCommunicationControl(const uint32_t deviceInstance, const uint8_t enableDisable,
                                const char* password, const uint8_t passwordLength,
                                const bool useTimeDuration, const uint16_t timeDuration,
                                uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return false;
    }

    const size_t requiredLength = strlen(DCC_PASSWORD);
    if (requiredLength > 0) {
        bool matches = (password != NULL) && (passwordLength == requiredLength);
        if (matches) {
            for (size_t i = 0; i < requiredLength; ++i) {
                if (password[i] != DCC_PASSWORD[i]) {
                    matches = false;
                    break;
                }
            }
        }
        if (!matches) {
            printf("DeviceCommunicationControl: REJECTED (password failure)\n");
            *errorCode = ERROR_CODE_PASSWORD_FAILURE;
            return false;
        }
    }

    const char* action = (enableDisable == DCC_ENABLE) ? "enable (resume communication)" :
                         (enableDisable == DCC_DISABLE) ? "disable (1) - DEPRECATED, the stack will reject this" :
                         (enableDisable == DCC_DISABLE_INITIATION) ? "disable-initiation (keep responding)" :
                         "unknown";
    if (useTimeDuration) {
        printf("DeviceCommunicationControl: %s for %u minute(s)\n", action, timeDuration);
    } else {
        printf("DeviceCommunicationControl: %s (indefinitely)\n", action);
    }
    return true;
}

// AcknowledgeAlarm (AE-ACK-B). An operator acknowledges the Lift's Passenger_Alarm
// alarm. The stack tracks the acknowledged state; this callback lets the
// application react (and accept or reject). See B-AAC's main.cpp for why the
// trailing parameters are left unnamed.
bool AcknowledgeAlarm(const uint32_t deviceInstance, const uint32_t /*acknowledgingProcessIdentifier*/,
                      const uint16_t eventObjectType, const uint32_t eventObjectInstance,
                      const uint16_t /*eventStateAcknowledged*/, const uint8_t /*eventTimeStampYear*/,
                      const uint8_t /*eventTimeStampMonth*/, const uint8_t /*eventTimeStampDay*/,
                      const uint8_t /*eventTimeStampWeekday*/, const uint8_t /*eventTimeStampHour*/,
                      const uint8_t /*eventTimeStampMinute*/, const uint8_t /*eventTimeStampSecond*/,
                      const uint8_t /*eventTimeStampHundrethSecond*/, const char* /*acknowledgementSource*/,
                      const uint32_t /*acknowledgementSourceLength*/, const uint8_t /*acknowledgementSourceEncoding*/,
                      const bool /*timeOfAcknowledgementIsTime*/, const bool /*timeOfAcknowledgementIsSequenceNumber*/,
                      const bool /*timeOfAcknowledgementIsDateTime*/, const uint8_t /*timeOfAcknowledgementYear*/,
                      const uint8_t /*timeOfAcknowledgementMonth*/, const uint8_t /*timeOfAcknowledgementDay*/,
                      const uint8_t /*timeOfAcknowledgementWeekday*/, const uint8_t /*timeOfAcknowledgementHour*/,
                      const uint8_t /*timeOfAcknowledgementMinute*/, const uint8_t /*timeOfAcknowledgementSecond*/,
                      const uint8_t /*timeOfAcknowledgementHundrethSecond*/,
                      const uint16_t /*timeOfAcknowledgementSequenceNumber*/, uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Only Lift 1 "Mauve"'s Passenger_Alarm has an alarm in this example. Reject
    // an ack for anything else.
    if (eventObjectType != OBJECT_TYPE_LIFT || eventObjectInstance != LIFT_INSTANCE) {
        printf("AcknowledgeAlarm: rejected for object (type %u, instance %u) - no such alarm\n",
               eventObjectType, eventObjectInstance);
        *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    printf("AcknowledgeAlarm: Lift 1 (Mauve) Passenger_Alarm acknowledged\n");
    return true; // accept the acknowledgement
}

// Build the 6-octet BACnet/IP connection string for the LOCAL SUBNET BROADCAST:
// the directed broadcast address (IP | ~mask) followed by the UDP port in network
// byte order. (When the mask is 0.0.0.0 - the helper's fallback - this collapses to
// the limited broadcast 255.255.255.255.)
static void LocalBroadcastConnString(uint8_t out[6]) {
    out[0] = (uint8_t)(g_ipAddress[0] | ~g_ipSubnetMask[0]);
    out[1] = (uint8_t)(g_ipAddress[1] | ~g_ipSubnetMask[1]);
    out[2] = (uint8_t)(g_ipAddress[2] | ~g_ipSubnetMask[2]);
    out[3] = (uint8_t)(g_ipAddress[3] | ~g_ipSubnetMask[3]);
    out[4] = (uint8_t)(g_bacnetIpUdpPort >> 8);
    out[5] = (uint8_t)(g_bacnetIpUdpPort & 0xFF);
}

// -----------------------------------------------------------------------------
// 3. main()
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Show printf output immediately, even when stdout is piped to a file.
    setvbuf(stdout, NULL, _IONBF, 0);

    // --- Load the CAS BACnet Stack -------------------------------------------
    if (!LoadBACnetFunctions()) {
        fprintf(stderr, "Error: failed to load the CAS BACnet Stack: %s\n",
                CASBACnetStackAdapter_LastError());
        return 1;
    }

    // g_firmwareRevision (Device object property 44) - see its own doc
    // comment above for why this is the STACK's version, not this example's
    // own (that's Application_Software_Version/APP_VERSION instead). Must
    // happen after LoadBACnetFunctions() (these getters ARE some of the
    // functions it loads) and before the Device object is ever readable.
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                 BACnetStack_GetAPIMajorVersion(), BACnetStack_GetAPIMinorVersion(),
                 BACnetStack_GetAPIPatchVersion(), BACnetStack_GetAPIBuildVersion());
        g_firmwareRevision = buf;
    }

    // --- Command line + version --------------------------------------------
    if (CASExampleHelper::HandleHelpAndVersionArgs(argc, argv, APP_NAME, APP_VERSION)) {
        return 0;
    }
    const uint16_t port = CASExampleHelper::ParsePortArg(argc, argv, 47808);
    g_deviceInstance = CASExampleHelper::ParseDeviceIdArg(argc, argv, g_deviceInstance);
    CASExampleHelper::PrintVersion(APP_NAME, APP_VERSION);

    // --- Bind the BACnet/IP socket -----------------------------------------
    if (!CASExampleHelper::SetupUDP(port)) {
        return 1;
    }

    // Capture the BACnet/IP addressing the Network Port object will report.
    g_bacnetIpUdpPort = port;
    if (!CASExampleHelper::GetLocalIPv4(g_ipAddress, g_ipSubnetMask)) {
        printf("FYI: could not read a local IPv4 address; Network Port IP_Address "
               "will report 0.0.0.0.\n");
    }

    // --- Register callbacks -------------------------------------------------
    CASExampleHelper::SetNetworkPortInstance(NETWORK_PORT_INSTANCE);
    CASExampleHelper::RegisterCommonCallbacks();
    BACnetStack_RegisterCallbackGetPropertyReal(GetPropertyReal);
    BACnetStack_RegisterCallbackGetPropertyEnumerated(GetPropertyEnumerated);
    BACnetStack_RegisterCallbackGetPropertyUnsignedInteger(GetPropertyUnsignedInteger);
    BACnetStack_RegisterCallbackGetPropertyCharacterString(GetPropertyCharString);
    BACnetStack_RegisterCallbackGetPropertyBool(GetPropertyBool);
    BACnetStack_RegisterCallbackGetPropertyOctetString(GetPropertyOctetString);
    // F-ELEVATOR: the list/sequence callbacks (see the section 2b comment for why
    // the fifth, write-side elevator callback is deliberately NOT registered).
    BACnetStack_RegisterCallbackGetListOfEnumerations(GetListOfEnumerations);
    BACnetStack_RegisterCallbackGetListElevatorGroupLandingCallStatus(GetListElevatorGroupLandingCallStatus);
    BACnetStack_RegisterCallbackGetSequenceLiftAssignedLandingCall(GetSequenceLiftAssignedLandingCall);
    BACnetStack_RegisterCallbackGetSequenceLiftRegisteredCarCall(GetSequenceLiftRegisteredCarCall);
    BACnetStack_RegisterCallbackGetSequenceLiftLandingDoorStatus(GetSequenceLiftLandingDoorStatus);
    // Device-management + alarming callbacks this profile needs. NOTE: there is
    // NO BACnetStack_RegisterCallbackSetProperty* call anywhere in this file -
    // this device has no writable property, so none of the Set callbacks are
    // ever registered.
    BACnetStack_RegisterCallbackDeviceCommunicationControl(DeviceCommunicationControl); // DM-DCC-B
    BACnetStack_RegisterCallbackAcknowledgeAlarm(AcknowledgeAlarm);                     // AE-ACK-B

    // --- Create the device --------------------------------------------------
    if (!BACnetStack_AddDevice(g_deviceInstance)) {
        printf("Error: Failed to add the Device %u.\n", g_deviceInstance);
        return 1;
    }

    // Enable the services a B-EM must execute. We set each one explicitly so the
    // profile requirements are obvious. There is deliberately NO WriteProperty /
    // WritePropertyMultiple / ReinitializeDevice / TimeSynchronization here - a
    // B-EM monitor never accepts any of those.
    const struct { uint32_t service; const char* name; } services[] = {
        { SERVICE_READ_PROPERTY,                 "ReadProperty (DS-RP-B)" },
        { SERVICE_READ_PROPERTY_MULTIPLE,        "ReadPropertyMultiple (DS-RPM-B)" },
        { SERVICE_DEVICE_COMMUNICATION_CONTROL,  "DeviceCommunicationControl (DM-DCC-B)" },
        { SERVICE_ACKNOWLEDGE_ALARM,             "AcknowledgeAlarm (AE-ACK-B)" },
        { SERVICE_GET_EVENT_INFORMATION,         "GetEventInformation (AE-INFO-B)" },
        { SERVICE_CONFIRMED_EVENT_NOTIFICATION,  "ConfirmedEventNotification (AE-N-I-B)" },
        { SERVICE_UNCONFIRMED_EVENT_NOTIFICATION,"UnconfirmedEventNotification (AE-N-I-B)" },
    };
    for (size_t i = 0; i < sizeof(services) / sizeof(services[0]); ++i) {
        if (!BACnetStack_SetServiceEnabled(g_deviceInstance, services[i].service, true)) {
            printf("Error: Failed to enable the %s service.\n", services[i].name);
            return 1;
        }
    }
    // DS-COV-B / DS-COVM-B: SubscribeCOV and SubscribeCOVPropertyMultiple. The
    // service enum has no separate SubscribeCOVPropertyMultiple constant in
    // CASBACnetStackExampleConstants.h (it mirrors only the handful the other
    // examples need), so it is named here with the verified bit number - see
    // BACnetServicesSupported.h: subscribeCovPropertyMultiple = 41.
    static const uint32_t SERVICE_SUBSCRIBE_COV = 5;
    static const uint32_t SERVICE_SUBSCRIBE_COV_PROPERTY_MULTIPLE = 41;
    if (!BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_SUBSCRIBE_COV, true)) {
        printf("Error: Failed to enable the SubscribeCOV (DS-COV-B) service.\n");
        return 1;
    }
    if (!BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_SUBSCRIBE_COV_PROPERTY_MULTIPLE, true)) {
        printf("Error: Failed to enable the SubscribeCOVPropertyMultiple (DS-COVM-B) service.\n");
        return 1;
    }

    // Discovery: Who-Is/I-Am (DM-DDB-B) and Who-Has/I-Have (DM-DOB-B). A B-EM
    // only ANSWERS - it does not itself send Who-Is on start-up (that would be
    // DM-DDB-A, which this profile does not require).
    if (!BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_WHO_IS, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_I_AM, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_WHO_HAS, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_I_HAVE, true)) {
        printf("Error: Failed to enable the discovery services (Who-Is/I-Am, Who-Has/I-Have).\n");
        return 1;
    }

    // --- Add the read-only base sensor objects (series convention) ----------
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT, ANALOG_INPUT_INSTANCE)) {
        printf("Error: Failed to add Analog Input %u (Bronze).\n", ANALOG_INPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_BINARY_INPUT, BINARY_INPUT_INSTANCE)) {
        printf("Error: Failed to add Binary Input %u (Emerald).\n", BINARY_INPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_INPUT, MULTI_STATE_INPUT_INSTANCE)) {
        printf("Error: Failed to add Multi-State Input %u (Hot Pink).\n", MULTI_STATE_INPUT_INSTANCE);
        return 1;
    }

    // --- Add the Network Port object ----------------------------------------
    if (!BACnetStack_AddNetworkPortObject(
            g_deviceInstance, NETWORK_PORT_INSTANCE,
            NETWORK_PORT_NETWORK_TYPE_IPV4,
            NETWORK_PORT_PROTOCOL_LEVEL_BACNET_APPLICATION,
            0,  // networkNumber: not configured
            NETWORK_NUMBER_QUALITY_UNKNOWN,
            NETWORK_PORT_REFERENCE_PORT_NONE)) {
        printf("Error: Failed to add Network Port 1 (Vermilion).\n");
        return 1;
    }

    // --- Enable the OPTIONAL properties we choose to expose ------------------
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_DEVICE,
                                        g_deviceInstance, PROPERTY_IDENTIFIER_DESCRIPTION, true)) {
        printf("Error: Failed to enable Description on the Device object.\n");
        return 1;
    }
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_INPUT,
                                        MULTI_STATE_INPUT_INSTANCE, PROPERTY_IDENTIFIER_STATE_TEXT, true)) {
        printf("Error: Failed to enable State_Text on Multi-State Input 1 (Hot Pink).\n");
        return 1;
    }

    // --- F-COVM: make Analog Input 1 (Bronze) subscribable on TWO properties -
    // A plain SubscribeCOV on a standard object type (Analog Input included)
    // already works without this; SetPropertySubscribable is what lets a
    // SubscribeCOVPropertyMultiple request name Status_Flags too, so one
    // multi-property notification carries both.
    if (!BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                             ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE, true) ||
        !BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                             ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_STATUS_FLAGS, true)) {
        printf("Error: Failed to make Analog Input 1 (Bronze) Present_Value/Status_Flags subscribable.\n");
        return 1;
    }
    // Table capacities. 0 = unbounded is the same default the stack already
    // enforces (28800 s max lifetime); called explicitly so the profile's
    // intent is visible in the code, matching the series convention.
    if (!BACnetStack_SetCOVSettings(g_deviceInstance, 0, 28800) ||
        !BACnetStack_SetCOVMultipleSettings(g_deviceInstance, 0)) {
        printf("Error: Failed to configure COV / COV-Multiple subscription settings.\n");
        return 1;
    }

    // --- F-ELEVATOR: Elevator Group 1 (Maroon), Lift 1 (Mauve), Escalator 1 (Mint)
    // Maroon groups Mauve (a group of Lifts); supportLandingCallStatus=true turns
    // on the (read-only, in this device) LandingCalls property.
    //
    // Positive Integer Value 1 (Turquoise) - the object Maroon's Machine_Room_ID
    // will name - MUST be added first. CASBACnetStackDLL.h's doc comment on
    // BACnetStack_AddElevatorGroupObject reads as though the stack creates it
    // automatically ("If the Positive Integer Value Object does not exist, this
    // function will create it"); verified against the running stack, it does
    // NOT - AddElevatorGroupObject fails outright with "Create the Positive
    // Integer Value object before adding the Elevator Group object" otherwise.
    // Trust the measured behaviour, not the comment.
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_POSITIVE_INTEGER_VALUE, MACHINE_ROOM_ID_INSTANCE)) {
        printf("Error: Failed to add Positive Integer Value 1 (Turquoise).\n");
        return 1;
    }
    if (!BACnetStack_AddElevatorGroupObject(g_deviceInstance, ELEVATOR_GROUP_INSTANCE,
                                            MACHINE_ROOM_ID_INSTANCE, ELEVATOR_GROUP_ID,
                                            true /*isGroupOfLifts*/, true /*supportLandingCallStatus*/)) {
        printf("Error: Failed to add Elevator Group 1 (Maroon).\n");
        return 1;
    }
    if (!BACnetStack_AddLiftOrEscalatorObject(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                              ELEVATOR_GROUP_INSTANCE, ELEVATOR_GROUP_ID, LIFT_INSTALLATION_ID)) {
        printf("Error: Failed to add Lift 1 (Mauve).\n");
        return 1;
    }
    // Mint is an Escalator, not a Lift - it cannot belong to Maroon (a group of
    // Lifts), so it is added with the "no reference" sentinel.
    if (!BACnetStack_AddLiftOrEscalatorObject(g_deviceInstance, OBJECT_TYPE_ESCALATOR, ESCALATOR_INSTANCE,
                                              NETWORK_PORT_REFERENCE_PORT_NONE, ESCALATOR_GROUP_ID,
                                              ESCALATOR_INSTALLATION_ID)) {
        printf("Error: Failed to add Escalator 1 (Mint).\n");
        return 1;
    }

    // Optional Lift properties this example demonstrates (each exercises one of
    // the four F-ELEVATOR list/sequence callbacks registered above).
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                        PROPERTY_IDENTIFIER_ASSIGNED_LANDING_CALLS, true) ||
        !BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                        PROPERTY_IDENTIFIER_REGISTERED_CAR_CALL, true) ||
        !BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                        PROPERTY_IDENTIFIER_LANDING_DOOR_STATUS, true)) {
        printf("Error: Failed to enable Lift 1 (Mauve)'s optional properties.\n");
        return 1;
    }
    // Optional Escalator properties this example demonstrates.
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_ESCALATOR, ESCALATOR_INSTANCE,
                                        PROPERTY_IDENTIFIER_POWER_MODE, true) ||
        !BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_ESCALATOR, ESCALATOR_INSTANCE,
                                        PROPERTY_IDENTIFIER_ESCALATOR_MODE, true)) {
        printf("Error: Failed to enable Escalator 1 (Mint)'s optional properties.\n");
        return 1;
    }

    // --- AE-N-I-B / AE-ACK-B / AE-INFO-B: Notification Class 1 (Crimson) -----
    if (!BACnetStack_AddNotificationClassObject(
            g_deviceInstance, NOTIFICATION_CLASS_INSTANCE,
            NC_PRIORITY_TO_OFFNORMAL, NC_PRIORITY_TO_FAULT, NC_PRIORITY_TO_NORMAL,
            true /*toOffNormalAckRequired*/, false /*toFaultAck*/, true /*toNormalAck*/)) {
        printf("Error: Failed to add Notification Class 1 (Crimson).\n");
        return 1;
    }
    uint8_t recipientMac[6];
    if (RECIPIENT_USE_BROADCAST) {
        LocalBroadcastConnString(recipientMac);
    } else {
        memcpy(recipientMac, RECIPIENT_IP, 4);
        recipientMac[4] = (uint8_t)(g_bacnetIpUdpPort >> 8);
        recipientMac[5] = (uint8_t)(g_bacnetIpUdpPort & 0xFF);
    }
    const uint8_t validDaysAll = 0x7F;
    if (!BACnetStack_AddRecipientToNotificationClass(
            g_deviceInstance, NOTIFICATION_CLASS_INSTANCE,
            validDaysAll,
            0, 0, 0, 0,         // from 00:00:00.00
            23, 59, 59, 99,     // to   23:59:59.99
            RECIPIENT_PROCESS_IDENTIFIER,
            false,              // issue UNCONFIRMED notifications (works to a broadcast)
            true, true, true,   // notify on to-offnormal, to-fault, to-normal
            false, 0,           // NOT using the device choice
            true,               // use the ADDRESS choice
            0,                  // network number 0 = this local network
            recipientMac, sizeof(recipientMac))) {
        printf("Error: could not seed the Notification Class recipient (Crimson).\n");
        return 1;
    }
    // Note: unlike B-AAC (AE-CRL-B), Recipient_List is NOT made writable here -
    // B-EM has no writable property anywhere.

    // Arm an intrinsic ChangeOfState (boolean) algorithm on Lift 1 (Mauve)'s
    // Passenger_Alarm: alarmValue=true means "Passenger_Alarm == true is the
    // OFFNORMAL condition". See the file header comment for how this is
    // simulated (a 30-second timer, never a write).
    if (!BACnetStack_SetAlarmsAndEventsForObjectEnabled(
            g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
            NOTIFICATION_CLASS_INSTANCE, NOTIFY_TYPE_ALARM,
            true /*enableToOffNormal*/, false /*enableToFault*/, true /*enableToNormal*/,
            true /*enableEventDetection*/,
            true /*enabled - 6.x dropped this argument's default, so pass it explicitly*/)) {
        printf("Error: could not enable alarms on Lift 1 (Mauve).\n");
        return 1;
    }
    if (!BACnetStack_SetIntrinsicChangeOfStateAlgorithmBool(
            g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
            true /*alarmValue: Passenger_Alarm == true is OFFNORMAL*/,
            0 /*timeDelay*/, false, 0, true /*enable*/)) {
        printf("Error: could not arm the ChangeOfState algorithm on Mauve's Passenger_Alarm.\n");
        return 1;
    }

    // Who-Is is answered automatically once WHO_IS/I_AM are enabled above. A
    // B-EM does not initiate its own Who-Is (no DM-DDB-A) - but it still
    // announces itself once at start-up, exactly as every example in the
    // series does.
    CASExampleHelper::SendIAm(g_deviceInstance);

    printf("FYI: Device %u (\"%s\") ready. Vendor ID %u. Read-only monitor - no "
           "WriteProperty of any kind is accepted. Press 'h' for help.\n",
           g_deviceInstance, DEVICE_NAME, VENDOR_IDENTIFIER);

    // --- Run the stack ------------------------------------------------------
    time_t lastAlarmToggle = time(NULL);
    bool running = true;
    while (running) {
        BACnetStack_Tick();

        // --- AE-N-I-B demo: flip the simulated Passenger_Alarm every 30 s ----
        // No write, no key - a monitor reports what the (simulated) elevator
        // hardware is doing, on its own schedule.
        time_t now = time(NULL);
        if (now - lastAlarmToggle >= PASSENGER_ALARM_SIMULATION_PERIOD_SECONDS) {
            g_liftPassengerAlarm = !g_liftPassengerAlarm;
            lastAlarmToggle = now;
            BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                    PROPERTY_IDENTIFIER_PASSENGER_ALARM);
            printf("Simulated Passenger_Alarm on Lift 1 (Mauve): %s\n",
                   g_liftPassengerAlarm ? "ACTIVE" : "cleared");
        }

        switch (CASExampleHelper::PollKey()) {
            case CASExampleHelper::KeyCommand::Help:
                CASExampleHelper::PrintHelp(APP_NAME, APP_VERSION);
                break;
            case CASExampleHelper::KeyCommand::Quit:
                running = false;
                break;
            case CASExampleHelper::KeyCommand::ArrowUp:
                g_analogInput1Value += 1.1f;
                BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                        ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE);
                printf("Analog Input 1 (Bronze) = %.1f C\n", g_analogInput1Value);
                break;
            case CASExampleHelper::KeyCommand::ArrowDown:
                g_analogInput1Value -= 1.1f;
                BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT,
                                        ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE);
                printf("Analog Input 1 (Bronze) = %.1f C\n", g_analogInput1Value);
                break;
            case CASExampleHelper::KeyCommand::DemoAdvance:
            case CASExampleHelper::KeyCommand::None:
            default:
                break;
        }

#if defined(_WIN32)
        Sleep(1); // 1 ms - be a good citizen, don't spin the CPU
#else
        usleep(1000);
#endif
    }

    CASExampleHelper::RestoreInput();
    CASExampleHelper::ShutdownUDP();
    return 0;
}
