// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE. The CAS BACnet Stack itself is
// a separate, commercially licensed product and is not covered by CC0.
// =============================================================================
// BACnet Profile Example - B-EM (Elevator Monitor) - C++
//
// This example implements the BACnet "B-EM" (Elevator Monitor) device profile:
// a READ-ONLY view of an elevator installation (a Lift and an Escalator grouped
// under an Elevator Group) with COV and intrinsic alarming. The Lift follows a
// SIMULATED car (simulation/ElevatorCar.cpp) as it moves floor to floor: this
// is the Profile Function, 135-2024 L.13.3 - "A B-EM device performs monitoring
// of an elevator control system." It accepts NO writes
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
//                   properties (see section 2b below). B-EC seeds from
//                   this repo and adds writes back for the lift's commandable
//                   pieces (landing-call control) - keep every comment here
//                   accurate for that reuse.
//
// The device keeps the three base read-only sensors (the series convention) and
// ADDS the elevator object family plus the Notification Class that routes the
// Lift's Passenger_Alarm. Each object has a colour name (the convention shared
// across this example series):
//
//     Device 389015              "Chipkin Example B-EM"      (instance configurable with --deviceID)
//     Analog Input  1            "Bronze"       (REAL, degrees Celsius; read-only)
//     Binary Input  1            "Emerald"      (active / inactive; read-only)
//     Multi-State Input 1        "Hot Pink"     (state 1..3; read-only)
//     Network Port 1             "Vermilion"    (the BACnet/IP port - required)
//     Elevator Group 1           "Maroon"       (groups Mauve; a group of Lifts)
//     Lift 1                     "Mauve"        (the SIMULATED car: position, direction, door;
//                                                CHANGE_OF_STATE on Passenger_Alarm,
//                                                FAULT_LISTED on Fault_Signals)
//     Escalator 1                "Mint"         (not part of the Elevator Group - escalators
//                                                aren't lifts, so it uses the "no reference" sentinel)
//     Positive Integer Value 1   "Turquoise"    (the Elevator Group's Machine_Room_ID target -
//                                                must be added BEFORE the Elevator Group, below)
//     Notification Class 1       "Crimson"      (routes Mauve's alarm and fault events)
//
// ALARMING: Lift 1 ("Mauve") arms the two algorithms cl. 12.59 names: an
// intrinsic CHANGE_OF_STATE (boolean) algorithm on Passenger_Alarm, and the
// FAULT_LISTED fault algorithm on Fault_Signals. Passenger_Alarm is not writable
// through this (or any) API - it reflects the alarm button in the car - so the
// SIMULATION presses it (the 's' key, or the test-only control channel); the
// simulated car raises DOOR_CLOSING_FAULT in Fault_Signals when its door cannot
// close. Each change is reported with BACnetStack_UpdateValue, which lets the
// stack's algorithms decide whether to notify Notification Class 1 ("Crimson")'s
// recipients. No client write is involved - a MONITOR reports what is
// happening, not what a client told it to do.
//
// COVM: Analog Input 1 ("Bronze") has both its Present_Value and Status_Flags
// marked subscribable, so a client's one SubscribeCOVPropertyMultiple request
// naming both properties gets ONE multi-property notification when Bronze's
// value changes (the up/down keys nudge it, exactly as in every other example).
//
// To be a conformant BACnet device (Protocol_Revision 30) each object must
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
// together), s = press/release the car's passenger alarm button (Simulation).
// Command line: --port <n>, --deviceID <n>. The Wire Tests use two TEST-ONLY
// environment variables, BEM_SIMULATION_CONTROL_PORT and
// BEM_TEST_EVENT_RECIPIENT_PORT (README "What is Simulation"); neither changes
// what this device does on the BACnet network.
//
// All the UDP/stack plumbing lives in common/CASExampleHelper so this file can
// stay focused on the BACnet logic.
// =============================================================================

#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // sscanf (%15s, bounded) / getenv in the test-only Simulation hooks: C4996 is noise
#endif

#include "CASExampleHelper.h"
#include "CASBACnetStackExampleConstants.h"
#include "CASBACnetStackAdapter.h" // the CAS BACnet Stack C API (BACnetStack_*); call
                                    // LoadBACnetFunctions() before any BACnetStack_* call -
                                    // see the top of main() below.
#include "ElevatorCar.h"            // SIMULATION: the lift car this device monitors
#include "SimulationControl.h"      // SIMULATION, TEST ONLY: the Wire Tests' hands on it

#include <stdio.h>
#include <string.h>
#include <string>
#include <chrono> // steady_clock - the car Simulation's clock
#include <stdlib.h> // getenv, strtol - the test-only event recipient

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
static const char* APP_VERSION = "1.2.0";

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
// correctly, and still have BOTH announce Object_Name "Chipkin Example B-EM" - a spec
// violation, and a hard BTL failure. In a real product Object_Name must be
// per-unit configurable too: derive it from a serial number, DIP switches, a
// config file, or add a --deviceName argument.
static const char* DEVICE_NAME = "Chipkin Example B-EM";

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
// BACnetPropertyIdentifier.h at the pin 64b2cfac5, 6.0.23).
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
static const uint32_t PROPERTY_IDENTIFIER_LANDING_CALLS = 470;
static const uint32_t PROPERTY_IDENTIFIER_LANDING_DOOR_STATUS = 472;
static const uint32_t PROPERTY_IDENTIFIER_OPERATION_DIRECTION = 477;
static const uint32_t PROPERTY_IDENTIFIER_PASSENGER_ALARM = 478;
static const uint32_t PROPERTY_IDENTIFIER_POWER_MODE = 479;
static const uint32_t PROPERTY_IDENTIFIER_REGISTERED_CAR_CALL = 480;

static const uint32_t PROPERTY_IDENTIFIER_RELIABILITY = 103;
static const uint32_t PROPERTY_IDENTIFIER_NUMBER_OF_APDU_RETRIES = 73;
static const uint32_t NUMBER_OF_APDU_RETRIES = 3; // see GetPropertyUnsignedInteger

// BACnetLiftCarDirectionEnum (BACnetLiftCarDirection.h)
static const uint32_t LIFT_CAR_DIRECTION_STOPPED = 2;
static const uint32_t LIFT_CAR_DIRECTION_UP = 3;
static const uint32_t LIFT_CAR_DIRECTION_DOWN = 4;
// BACnetDoorStatusEnum (BACnetDoorStatus.h) - shared with Access Door / the Lift's
// per-door Car_Door_Status and Landing_Door_Status arrays.
static const uint32_t DOOR_STATUS_CLOSED = 0;
static const uint32_t DOOR_STATUS_OPENED = 1;
static const uint32_t DOOR_STATUS_CLOSING = 6;
static const uint32_t DOOR_STATUS_OPENING = 7;
static const uint32_t DOOR_STATUS_SAFETY_LOCKED = 8;
// BACnetLiftFaultEnum (BACnetLiftFault.h) - the Fault_Signals values (cl. 12.59.32).
static const uint32_t LIFT_FAULT_DOOR_CLOSING_FAULT = 6;
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

// --- SIMULATION: the lift car Mauve monitors (simulation/ElevatorCar.h) -------
// A real B-EM reads all of this from the elevator controller over its own bus;
// here a simulated car stands in for it. It serves universal floors 1..8 one
// floor at a time at a set speed, opens and closes its door at each stop, and
// can be obstructed into a DOOR_CLOSING_FAULT. Simulated passengers keep it
// busy: whenever it has been idle for SIMULATED_TRAFFIC_IDLE_MILLISECONDS one
// of them presses the next button on SIMULATED_TRAFFIC_TOUR (a car call).
static const uint8_t LIFT_LOWEST_FLOOR = 1;
static const uint8_t LIFT_HIGHEST_FLOOR = 8;
static ElevatorSimulation::Car g_car(LIFT_LOWEST_FLOOR, LIFT_HIGHEST_FLOOR, LIFT_LOWEST_FLOOR);
static const uint8_t SIMULATED_TRAFFIC_TOUR[] = { 6, 2, 8, 4, 1 };
static const uint32_t SIMULATED_TRAFFIC_IDLE_MILLISECONDS = 3000;
static bool g_simulatedTraffic = true;
static size_t g_simulatedTrafficNext = 0;
static uint32_t g_carIdleMilliseconds = 0;

static bool g_liftOutOfService = false;
// The alarm source: Passenger_Alarm is not writable through any API in this
// stack (a real installation drives it from the car's alarm button). In this
// example the button is pressed with the 's' key or through the test-only
// simulation control channel ("alarm on" / "alarm off").
static bool g_liftPassengerAlarm = false;

static bool g_escalatorOutOfService = false;
static bool g_escalatorPassengerAlarm = false;
static bool g_escalatorPowerMode = true; // powered on

// --- AE-N-I-B: Notification Class 1 "Crimson" routes Mauve's Passenger_Alarm --
static const uint32_t NOTIFICATION_CLASS_INSTANCE = 1;     // "Crimson"
// Notification priorities for the three transitions (lower = more urgent).
// To-fault is Mauve's FAULT_LISTED fault algorithm on Fault_Signals (cl. 12.59:
// "For reliability-evaluation, the FAULT_LISTED fault algorithm can be applied").
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

// TEST ONLY: when this environment variable holds a UDP port, Crimson gets a
// SECOND recipient, 127.0.0.1:<port>, sent CONFIRMED notifications - the Wire
// Tests' operator workstation (tests/). A real B-EM's recipients are set at
// commissioning; this one has no writable Recipient_List (no writes at all).
static const char* TEST_EVENT_RECIPIENT_PORT_VARIABLE = "BEM_TEST_EVENT_RECIPIENT_PORT";
static const uint32_t TEST_RECIPIENT_PROCESS_IDENTIFIER = 2;
// TEST ONLY: the simulation control channel (simulation/SimulationControl.h).
static const char* SIMULATION_CONTROL_PORT_VARIABLE = "BEM_SIMULATION_CONTROL_PORT";

// The simulated car's state, in BACnet's terms (cl. 12.59.15 Car_Moving_Direction,
// 12.59.17 Car_Door_Status). A car standing at a floor with its door shut is
// CLOSED ("fully closed but not yet locked"); the door locks before the car
// moves, so a moving car always reports SAFETY_LOCKED.
static uint32_t CarMovingDirection() {
    switch (g_car.MovingDirection()) {
        case ElevatorSimulation::Direction::Up:   return LIFT_CAR_DIRECTION_UP;
        case ElevatorSimulation::Direction::Down: return LIFT_CAR_DIRECTION_DOWN;
        default:                                  return LIFT_CAR_DIRECTION_STOPPED;
    }
}

static uint32_t CarDoorStatus() {
    switch (g_car.DoorState()) {
        case ElevatorSimulation::Door::Locked:  return DOOR_STATUS_SAFETY_LOCKED;
        case ElevatorSimulation::Door::Opening: return DOOR_STATUS_OPENING;
        case ElevatorSimulation::Door::Opened:  return DOOR_STATUS_OPENED;
        case ElevatorSimulation::Door::Closing: return DOOR_STATUS_CLOSING;
        default:                                return DOOR_STATUS_CLOSED;
    }
}

static uint32_t LiftFaultSignal(ElevatorSimulation::Fault fault) {
    switch (fault) {
        case ElevatorSimulation::Fault::DoorClosing:
        default:
            return LIFT_FAULT_DOOR_CLOSING_FAULT;
    }
}

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
            *value = CarMovingDirection();
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_CAR_DOOR_STATUS &&
            useArrayIndex && propertyArrayIndex >= 1 && propertyArrayIndex <= LIFT_NUMBER_OF_DOORS) {
            // Required, per-door array (this Lift has one door - see the
            // "THE DOOR COUNT IS NOT STORED BY THE STACK" note in
            // CASBACnetStackDLL.h: the application alone owns the door count,
            // by simply answering only door indices it recognises).
            *value = CarDoorStatus();
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
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance &&
        propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_APDU_RETRIES) {
        // The stack retries a confirmed request (a ConfirmedEventNotification) this
        // many times, APDU_Timeout apart. Served by nobody it reads 0 - 135-2024
        // 12.11.29: "If this device does not perform retries, then this property
        // shall be set to zero" - and a recipient that missed one datagram never gets
        // the alarm. 3 is the clause's suggested default. (APDU_Timeout is left to the
        // stack: 10000 ms, cl. 12.11.28's default for a device that does not let it be
        // modified.)
        *value = NUMBER_OF_APDU_RETRIES;
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
        // Required, NO STACK DEFAULT. The universal floor number of the car (cl. 12.59.14).
        *value = g_car.Position();
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
// Landing_Call_Control itself is not present on Maroon at all (see
// AddElevatorGroupObject below: cl. 12.58.10 requires it to be writable when
// present), so no client can address a landing-call command to this device.
// B-EC, which seeds from this repo, is where that property and its callback
// get wired up.
// -----------------------------------------------------------------------------

// FaultSignals (required on the Lift) - the car's active faults (cl. 12.59.32),
// one BACnetLiftFault per call. It is also the pMonitoredList of Mauve's
// FAULT_LISTED fault algorithm: when it is not empty the stack takes Reliability
// to FAULTS_LISTED and Event_State to FAULT, and sends the to-fault notification.
// The stack pages with rangeIndexOrSequenceNumber as a 0-based counter (see
// BACnetStack_RegisterCallbackGetListOfEnumerations). An EMPTY list is answered
// with true and the enumeration left untouched: the stack's sentinel skips it.
bool GetListOfEnumerations(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           const uint8_t rangeOption, const uint32_t rangeIndexOrSequenceNumber,
                           const bool rangeInPositiveDirection, uint32_t* enumeration, bool* more) {
    (void)rangeOption;
    (void)rangeInPositiveDirection;
    if (deviceInstance != g_deviceInstance) {
        *more = false;
        return false;
    }
    if (objectType == OBJECT_TYPE_LIFT && objectInstance == LIFT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_FAULT_SIGNALS) {
        const std::vector<ElevatorSimulation::Fault>& faults = g_car.Faults();
        if (rangeIndexOrSequenceNumber < faults.size()) {
            *enumeration = LiftFaultSignal(faults[rangeIndexOrSequenceNumber]);
        }
        *more = rangeIndexOrSequenceNumber + 1 < faults.size();
        return true;
    }
    *more = false;
    return false;
}

// LandingCalls (optional; enabled on Maroon with SetPropertyEnabled) - the
// group's current landing-call queue. This monitor has none outstanding. The
// callback's contract (BACnetCallbackInterface.h): return true only when it
// supplied an element; return false on the first call (rangeIndexOrSequence 0)
// for an EMPTY list. Returning true without an element is read as a malformed
// element and the read fails with READ_ACCESS_DENIED.
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
    *more = false;
    return false; // no landing calls outstanding right now: an empty list
}

// AssignedLandingCalls (optional Lift property, enabled below) - the landing
// calls this car has been assigned to answer (cl. 12.59.11). A B-EM's group
// takes no landing calls (Landing_Call_Control is absent - see section 2b), so
// the list is always empty: answered like an empty Landing_Calls, false at offset 0.
bool GetSequenceLiftAssignedLandingCall(const uint32_t deviceInstance, const uint32_t objectInstance,
                                        const uint32_t arrayIndexForDoor, const uint32_t offset,
                                        uint8_t* floorNumber, uint32_t* direction, bool* more) {
    (void)deviceInstance;
    (void)objectInstance;
    (void)arrayIndexForDoor;
    (void)offset;
    (void)floorNumber;
    (void)direction;
    *more = false;
    return false;
}

// RegisteredCarCall (optional Lift property, enabled below) - the floors
// passengers have pressed inside the car and the car has not reached yet
// (cl. 12.59.13): the simulated car's pending stops, lowest first. A stop leaves
// the list the moment the car stops at that floor.
bool GetSequenceLiftRegisteredCarCall(const uint32_t deviceInstance, const uint32_t objectInstance,
                                      const uint32_t arrayIndexForDoor, const uint32_t offset,
                                      uint8_t* floorNumber, bool* more) {
    const std::vector<uint8_t>& stops = g_car.PendingStops();
    if (deviceInstance != g_deviceInstance || objectInstance != LIFT_INSTANCE ||
        arrayIndexForDoor != 1 /* this Lift's one door */ || offset >= stops.size()) {
        *more = false;
        return false; // no element at this offset (false at offset 0 = an empty list)
    }
    *floorNumber = stops[offset];
    *more = offset + 1 < stops.size();
    return true;
}

// LandingDoorStatus (optional Lift property, enabled below) - the landing doors
// on the floors this car serves (cl. 12.59.33). Landing doors are driven by the
// car door (cl. 12.59: "the status of a lift car door also reflects the status
// of the corresponding landing door at a particular floor"), so the landing door
// at the car's floor follows the car door and every other one is SAFETY_LOCKED.
bool GetSequenceLiftLandingDoorStatus(const uint32_t deviceInstance, const uint32_t objectInstance,
                                      const uint32_t arrayIndexForDoor, const uint32_t offset,
                                      uint8_t* floorNumber, uint32_t* doorStatus, bool* more) {
    const uint32_t floorCount = (uint32_t)(LIFT_HIGHEST_FLOOR - LIFT_LOWEST_FLOOR + 1);
    if (deviceInstance != g_deviceInstance || objectInstance != LIFT_INSTANCE ||
        arrayIndexForDoor != 1 || offset >= floorCount) {
        *more = false;
        return false;
    }
    *floorNumber = (uint8_t)(LIFT_LOWEST_FLOOR + offset);
    *doorStatus = (*floorNumber == g_car.Position()) ? CarDoorStatus() : DOOR_STATUS_SAFETY_LOCKED;
    *more = offset + 1 < floorCount;
    return true;
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
// 2d. SIMULATION: running the car, and telling the stack what changed
//
// BACnetStack_UpdateValue is what makes COV and the event/fault algorithms see
// a value this application serves - without it the car could move and no
// subscriber or alarm recipient would ever hear of it.
// -----------------------------------------------------------------------------
static void ReportCarChanges(uint32_t changes) {
    if (changes & ElevatorSimulation::CHANGE_POSITION) {
        BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE, PROPERTY_IDENTIFIER_CAR_POSITION);
    }
    if (changes & ElevatorSimulation::CHANGE_DIRECTION) {
        BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE, PROPERTY_IDENTIFIER_CAR_MOVING_DIRECTION);
    }
    if (changes & ElevatorSimulation::CHANGE_DOOR) {
        BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE, PROPERTY_IDENTIFIER_CAR_DOOR_STATUS);
    }
    if (changes & ElevatorSimulation::CHANGE_FAULTS) {
        // Fault_Signals is the FAULT_LISTED algorithm's monitored list: this is the
        // call that takes Mauve to FAULT (and back to NORMAL) and notifies Crimson.
        BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE, PROPERTY_IDENTIFIER_FAULT_SIGNALS);
        printf("SIMULATION: Lift 1 (Mauve) Fault_Signals %s\n",
               g_car.Faulted() ? "= { DOOR_CLOSING_FAULT } - a door is failing to close" : "cleared");
    }
    if (changes & (ElevatorSimulation::CHANGE_POSITION | ElevatorSimulation::CHANGE_DIRECTION)) {
        printf("SIMULATION: car at floor %u, %s\n", g_car.Position(),
               ElevatorSimulation::DirectionName(g_car.MovingDirection()));
    }
}

static void SetPassengerAlarm(bool active) {
    if (g_liftPassengerAlarm == active) {
        return;
    }
    g_liftPassengerAlarm = active;
    // Passenger_Alarm is the CHANGE_OF_STATE algorithm's pMonitoredValue (cl. 12.59.24).
    BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE, PROPERTY_IDENTIFIER_PASSENGER_ALARM);
    printf("SIMULATION: Lift 1 (Mauve) passenger alarm button %s\n", active ? "PRESSED" : "released");
}

// Simulated passengers: once the car has stood idle for a while, someone gets in
// and presses the next floor on the tour.
static void RunSimulatedTraffic(uint32_t elapsedMilliseconds) {
    const bool idle = g_car.PendingStops().empty() && g_car.DoorState() == ElevatorSimulation::Door::Closed;
    g_carIdleMilliseconds = idle ? g_carIdleMilliseconds + elapsedMilliseconds : 0;
    if (!g_simulatedTraffic || g_carIdleMilliseconds < SIMULATED_TRAFFIC_IDLE_MILLISECONDS) {
        return;
    }
    const uint8_t floor = SIMULATED_TRAFFIC_TOUR[g_simulatedTrafficNext];
    g_simulatedTrafficNext = (g_simulatedTrafficNext + 1) % (sizeof(SIMULATED_TRAFFIC_TOUR) / sizeof(SIMULATED_TRAFFIC_TOUR[0]));
    g_carIdleMilliseconds = 0;
    if (g_car.RequestStop(floor)) {
        printf("SIMULATION: a passenger presses %u in the car\n", floor);
    }
}

// The test-only control channel's commands (simulation/SimulationControl.h):
//     alarm on|off                       press / release the car's passenger alarm button
//     obstruct on|off                    block / clear the car's doorway
//     call <floor>                       a passenger presses <floor> in the car
//     traffic on|off                     the simulated passengers' tour
//     timing <travel> <door> <dwell>     milliseconds per floor / per door move / doors held open
//     state                              -> "floor <n> direction <d> door <d> stops <a,b> faults <n>"
// Each answers "ok" (or the state), or "error <reason>".
static void RunSimulationCommand(const char* command, char* reply, size_t replyLength) {
    char verb[16] = { 0 };
    char argument[16] = { 0 };
    unsigned travel = 0;
    unsigned door = 0;
    unsigned dwell = 0;
    const int fieldCount = sscanf(command, "%15s %15s", verb, argument);
    const bool on = strcmp(argument, "on") == 0;
    const bool onOrOff = on || strcmp(argument, "off") == 0;
    if (fieldCount == 2 && strcmp(verb, "alarm") == 0 && onOrOff) {
        SetPassengerAlarm(on);
    } else if (fieldCount == 2 && strcmp(verb, "obstruct") == 0 && onOrOff) {
        g_car.SetObstructed(on);
        printf("SIMULATION: something %s the car doorway\n", on ? "is blocking" : "no longer blocks");
    } else if (fieldCount == 2 && strcmp(verb, "traffic") == 0 && onOrOff) {
        g_simulatedTraffic = on;
    } else if (fieldCount == 2 && strcmp(verb, "call") == 0) {
        const long floor = strtol(argument, NULL, 10);
        if (floor < 0 || floor > 255 || !g_car.RequestStop((uint8_t)floor)) {
            snprintf(reply, replyLength, "error floor %ld not served", floor);
            return;
        }
    } else if (strcmp(verb, "timing") == 0 &&
               sscanf(command, "%15s %u %u %u", verb, &travel, &door, &dwell) == 4 &&
               travel > 0 && door > 0 && dwell > 0) {
        ElevatorSimulation::Timing timing = g_car.CurrentTiming();
        timing.floorTravelMilliseconds = travel;
        timing.doorMoveMilliseconds = door;
        timing.dwellMilliseconds = dwell;
        g_car.SetTiming(timing);
    } else if (fieldCount == 1 && strcmp(verb, "state") == 0) {
        std::string stops;
        for (uint8_t stop : g_car.PendingStops()) {
            stops += (stops.empty() ? "" : ",") + std::to_string(stop);
        }
        snprintf(reply, replyLength, "floor %u direction %s door %s stops %s faults %u",
                 g_car.Position(), ElevatorSimulation::DirectionName(g_car.MovingDirection()),
                 ElevatorSimulation::DoorName(g_car.DoorState()), stops.empty() ? "-" : stops.c_str(),
                 (unsigned)g_car.Faults().size());
        return;
    } else {
        snprintf(reply, replyLength, "error unknown command");
        return;
    }
    snprintf(reply, replyLength, "ok");
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
    // Maroon groups Mauve (a group of Lifts). supportLandingCallStatus is FALSE:
    // it would enable Landing_Calls AND Landing_Call_Control, and 135-2024
    // cl. 12.58.10 says of Landing_Call_Control "If it is present, it shall be
    // writable" - a read-only monitor must not have it. Landing_Calls alone (the
    // group's current call queue, read-only) is enabled after the Lift below.
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
                                            true /*isGroupOfLifts*/, false /*supportLandingCallStatus*/)) {
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

    // Optional Elevator Group property: Landing_Calls only (see above).
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_ELEVATOR_GROUP, ELEVATOR_GROUP_INSTANCE,
                                        PROPERTY_IDENTIFIER_LANDING_CALLS, true)) {
        printf("Error: Failed to enable Elevator Group 1 (Maroon)'s Landing_Calls property.\n");
        return 1;
    }

    // Optional Lift properties this example demonstrates (each exercises one of
    // the four F-ELEVATOR list/sequence callbacks registered above), plus
    // Reliability, which the FAULT_LISTED fault algorithm below drives
    // (cl. 12.59.29: "If a fault algorithm is applied, then this property shall be
    // the pCurrentReliability parameter for the object's fault algorithm").
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                        PROPERTY_IDENTIFIER_RELIABILITY, true) ||
        !BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
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

    // TEST ONLY: a second, CONFIRMED recipient for the Wire Tests' operator
    // workstation - see TEST_EVENT_RECIPIENT_PORT_VARIABLE above. Off unless set.
    const char* testRecipientPortText = getenv(TEST_EVENT_RECIPIENT_PORT_VARIABLE);
    if (testRecipientPortText != NULL && testRecipientPortText[0] != '\0') {
        const long testRecipientPort = strtol(testRecipientPortText, NULL, 10);
        const uint8_t testRecipientMac[6] = { 127, 0, 0, 1, (uint8_t)(testRecipientPort >> 8),
                                              (uint8_t)(testRecipientPort & 0xFF) };
        if (testRecipientPort <= 0 || testRecipientPort > 65535 ||
            !BACnetStack_AddRecipientToNotificationClass(
                g_deviceInstance, NOTIFICATION_CLASS_INSTANCE, validDaysAll, 0, 0, 0, 0, 23, 59, 59, 99,
                TEST_RECIPIENT_PROCESS_IDENTIFIER, true /*confirmed*/, true, true, true,
                false, 0, true, 0, testRecipientMac, sizeof(testRecipientMac))) {
            printf("Error: %s=%s - could not add the test event recipient.\n",
                   TEST_EVENT_RECIPIENT_PORT_VARIABLE, testRecipientPortText);
            return 1;
        }
        printf("TEST: Crimson also notifies 127.0.0.1:%ld, confirmed (%s).\n", testRecipientPort,
               TEST_EVENT_RECIPIENT_PORT_VARIABLE);
    }

    // Lift 1 (Mauve)'s event reporting, both halves 135-2024 cl. 12.59 names:
    //   "Lift objects that support intrinsic reporting shall apply the
    //    CHANGE_OF_STATE event algorithm on the Passenger_Alarm property. The
    //    pAlarmValues parameter shall contain the value TRUE."
    //   "For reliability-evaluation, the FAULT_LISTED fault algorithm can be
    //    applied." - its pMonitoredList is Fault_Signals (cl. 12.59.32).
    // So a passenger pressing the alarm button is a to-offnormal ALARM, and a car
    // fault (the simulated door that fails to close) is a to-fault. Both go to
    // Notification Class 1 (Crimson)'s recipients.
    if (!BACnetStack_SetAlarmsAndEventsForObjectEnabled(
            g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
            NOTIFICATION_CLASS_INSTANCE, NOTIFY_TYPE_ALARM,
            true /*enableToOffNormal*/, true /*enableToFault*/, true /*enableToNormal*/,
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
    if (!BACnetStack_SetFaultListedAlgorithm(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE, true /*enable*/)) {
        printf("Error: could not arm the FAULT_LISTED algorithm on Mauve's Fault_Signals.\n");
        return 1;
    }

    // DS-COVM-B on the car: a monitoring client can subscribe (with
    // SubscribeCOVPropertyMultiple) to where the car is, where it is going and
    // what its door is doing, and hear every change as the Simulation reports it.
    if (!BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                             PROPERTY_IDENTIFIER_CAR_POSITION, true) ||
        !BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                             PROPERTY_IDENTIFIER_CAR_MOVING_DIRECTION, true) ||
        !BACnetStack_SetPropertySubscribable(g_deviceInstance, OBJECT_TYPE_LIFT, LIFT_INSTANCE,
                                             PROPERTY_IDENTIFIER_CAR_DOOR_STATUS, true)) {
        printf("Error: Failed to make Lift 1 (Mauve)'s car properties subscribable.\n");
        return 1;
    }

    if (!SimulationControl::Start(SIMULATION_CONTROL_PORT_VARIABLE)) {
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
    std::chrono::steady_clock::time_point lastCarTick = std::chrono::steady_clock::now();
    bool running = true;
    while (running) {
        BACnetStack_Tick();

        // --- SIMULATION: the car moves; the stack hears of every change ------
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const uint32_t elapsedMilliseconds = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastCarTick).count();
        if (elapsedMilliseconds >= 10) {
            lastCarTick = now;
            ReportCarChanges(g_car.Tick(elapsedMilliseconds));
            RunSimulatedTraffic(elapsedMilliseconds);
        }
        SimulationControl::Service(RunSimulationCommand);

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
                // 's': press (or release) the car's passenger alarm button.
                SetPassengerAlarm(!g_liftPassengerAlarm);
                break;
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

    SimulationControl::Stop();
    CASExampleHelper::RestoreInput();
    CASExampleHelper::ShutdownUDP();
    return 0;
}
