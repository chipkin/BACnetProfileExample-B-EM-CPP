# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""What B-EM's Wire Tests share: this elevator monitor's numbers, and starting it.

The setup, all on one host:

    Peer: monitoring client / operator workstation (127.0.0.1, ephemeral UDP port)
        |  BACnet/IP: ReadProperty(Multiple), SubscribeCOVPropertyMultiple,
        |  AcknowledgeAlarm, GetEventInformation; event notifications back to it
    BACnetExampleBEM   UDP --port           Lift 1 "Mauve" in Elevator Group 1 "Maroon"
        ^
        |  test-only control channel, 127.0.0.1 UDP --port + 1 (BEM_SIMULATION_CONTROL_PORT)
    the test, playing the physical world: the car's passengers, its alarm button, its doorway

The Peer plays a building operator's workstation watching the lifts. It learns everything
over BACnet. The control channel never touches BACnet: it is how the test presses buttons
inside the simulated car (README "What is Simulation").
"""
import argparse
import sys
import time
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY))

import wiretest  # noqa: E402  (the vendored harness at the repository root)
import property_multiple  # noqa: E402
from simulation_control import SimulationControl  # noqa: E402

# Match main.cpp.
DEVICE_INSTANCE = 389015
OBJECT_TYPE_ELEVATOR_GROUP = 57
OBJECT_TYPE_ESCALATOR = 58
OBJECT_TYPE_LIFT = 59
LIFT = (OBJECT_TYPE_LIFT, 1)                   # Lift 1 "Mauve"
ESCALATOR = (OBJECT_TYPE_ESCALATOR, 1)         # Escalator 1 "Mint"
ELEVATOR_GROUP = (OBJECT_TYPE_ELEVATOR_GROUP, 1)
LOWEST_FLOOR, HIGHEST_FLOOR = 1, 8
NOTIFICATION_CLASS = 1
TEST_RECIPIENT_PROCESS_IDENTIFIER = 2

# Lift properties (cl. 12.59; numbers from cl. 21 BACnetPropertyIdentifier).
PROPERTY_ACKED_TRANSITIONS = 0
PROPERTY_EVENT_STATE = 36
PROPERTY_RELIABILITY = 103
PROPERTY_STATUS_FLAGS = 111
PROPERTY_CAR_DOOR_STATUS = 450
PROPERTY_CAR_MOVING_DIRECTION = 457
PROPERTY_CAR_POSITION = 458
PROPERTY_FAULT_SIGNALS = 463
PROPERTY_LANDING_DOOR_STATUS = 472
PROPERTY_PASSENGER_ALARM = 478
PROPERTY_REGISTERED_CAR_CALL = 480

# BACnetLiftCarDirection, BACnetDoorStatus, BACnetLiftFault, BACnetReliability (cl. 21).
DIRECTION_STOPPED, DIRECTION_UP, DIRECTION_DOWN = 2, 3, 4
DOOR_CLOSED, DOOR_OPENED, DOOR_CLOSING, DOOR_OPENING, DOOR_SAFETY_LOCKED = 0, 1, 6, 7, 8
LIFT_FAULT_DOOR_CLOSING_FAULT = 6
RELIABILITY_NO_FAULT_DETECTED, RELIABILITY_FAULTS_LISTED = 0, 23
# BACnetEventType (cl. 21): the two algorithms cl. 12.59 names for a Lift.
EVENT_TYPE_CHANGE_OF_STATE = 1
EVENT_TYPE_CHANGE_OF_RELIABILITY = 19
STATUS_FLAG_IN_ALARM, STATUS_FLAG_FAULT = 0, 1

# The car's speed for the tests, milliseconds: per floor, per door move, doors held open.
# main.cpp's defaults (2000 / 1000 / 2000) are a demo for a person watching; a test sets these.
TRAVEL_MS, DOOR_MS, DWELL_MS = 500, 250, 600


def parser(description):
    arguments = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--exe", default=str(wiretest.default_exe(REPOSITORY, "BACnetExampleBEM")),
                           help="path to BACnetExampleBEM (default: build/BACnetExampleBEM)")
    arguments.add_argument("--port", type=int, default=48030,
                           help="the monitor's UDP port (default 48030); the simulation control channel is --port + 1")
    return arguments


def start_monitor(args, recipient_port=None, label="elevator monitor"):
    """Start the example with its simulation control channel on --port + 1 and, if given, the
    test event recipient (a Workstation's port) as Crimson's confirmed recipient."""
    environment = {"BEM_SIMULATION_CONTROL_PORT": str(args.port + 1)}
    if recipient_port is not None:
        environment["BEM_TEST_EVENT_RECIPIENT_PORT"] = str(recipient_port)
    return wiretest.start(args.exe, ["--port", args.port], label, environment)


def control(args):
    """The test's hands inside the simulated car; switches the simulated passengers off and
    speeds the car up, so every trip in a test is one the test asked for."""
    hands = SimulationControl(args.port + 1)
    hands.send("traffic off")
    hands.send(f"timing {TRAVEL_MS} {DOOR_MS} {DWELL_MS}")
    return hands


REQUEST_TIMEOUT = 0.5      # seconds; a loopback answer takes milliseconds
REQUEST_ATTEMPTS = 3


def ask(client, build_apdu, port):
    """Send the confirmed request `build_apdu(invoke_id)` makes with `client` (a wiretest.Client
    made with retries=0, or a Workstation); return the Answer. An unanswered request is sent again
    after REQUEST_TIMEOUT, with a visible [RETRY] line, up to REQUEST_ATTEMPTS times - as a BACnet
    client does. Windows loopback, on a loaded machine, was measured losing about one datagram in
    700 before the example saw it (699 sent, 698 received); a 2-3 s timeout then lets the car move
    on while the test waits, a short one does not."""
    answer = None
    for attempt in range(REQUEST_ATTEMPTS):
        if attempt:
            print(f"[RETRY] request unanswered after {REQUEST_TIMEOUT} s - sent again", flush=True)
        answer = client.request(build_apdu, port, timeout=REQUEST_TIMEOUT)
        if answer.answered:
            break
    return answer


def read(client, port, object_identifier, property_identifier, array_index=None):
    return ask(client, lambda invoke_id: wiretest.read_property_request(
        invoke_id, *object_identifier, property_identifier, array_index), port)


def write(client, port, object_identifier, property_identifier, value_octets, priority=None):
    return ask(client, lambda invoke_id: wiretest.write_property_request(
        invoke_id, *object_identifier, property_identifier, value_octets, priority), port)


CAR_PROPERTIES = (PROPERTY_CAR_POSITION, PROPERTY_CAR_MOVING_DIRECTION, PROPERTY_CAR_DOOR_STATUS)


def car_snapshot(client, port):
    """(Car_Position, Car_Moving_Direction, Car_Door_Status[1]) from ONE ReadPropertyMultiple,
    so the three are consistent; None if it was not answered."""
    answer = ask(client, lambda invoke_id: property_multiple.read_property_multiple_request(
        invoke_id, [(*LIFT, CAR_PROPERTIES)]), port)
    results = property_multiple.parse_read_property_multiple_ack(answer.frame.payload) if answer.answered else None
    if not results or LIFT not in results:
        return None
    lift = results[LIFT]
    door = lift.get(PROPERTY_CAR_DOOR_STATUS)
    door_values = wiretest.decoded_values(door) if isinstance(door, bytes) else []
    return (property_multiple.value_of(lift.get(PROPERTY_CAR_POSITION)),
            property_multiple.value_of(lift.get(PROPERTY_CAR_MOVING_DIRECTION)),
            door_values[0] if door_values else None)


def lift_value(client, port, property_identifier, array_index=None):
    """The decoded value of a Lift property, or 'no answer' / 'error' so a comparison fails readably."""
    answer = read(client, port, LIFT, property_identifier, array_index)
    if not answer.answered:
        return "no answer"
    if not answer.ok:
        return "error"
    return answer.value


def lift_values(client, port, property_identifier):
    """Every application value of a list- or array-valued Lift property, or 'error'."""
    answer = read(client, port, LIFT, property_identifier)
    return wiretest.decoded_values(answer.value_octets) if answer.ok else "error"


def describe(answer):
    return answer.describe()


# --- alarm and event services, from the operator workstation (a wiretest.Workstation) ------------

def get_event_information(workstation, args):
    """(summaries, Answer) from one GetEventInformation; summaries is None if it was not a ComplexACK."""
    answer = ask(workstation, lambda invoke_id: wiretest.alarm.get_event_information_request(invoke_id), args.port)
    if answer.frame is None or answer.frame.pdu_type != wiretest.PDU_COMPLEX_ACK:
        return None, answer
    summaries, _ = wiretest.alarm.parse_event_summaries(answer.frame.payload)
    return summaries, answer


def summary_for(summaries, object_identifier):
    for entry in summaries or []:
        if entry["object"] == object_identifier:
            return entry
    return None


def acknowledge(workstation, args, object_identifier, event_state, time_stamp, source="Operator Olive"):
    """AcknowledgeAlarm for `object_identifier`, the time of acknowledgment being now (host clock)."""
    now = time.localtime()
    time_of_acknowledgment = (bytes([now.tm_year - 1900, now.tm_mon, now.tm_mday, now.tm_wday + 1]),
                              bytes([now.tm_hour, now.tm_min, now.tm_sec, 0]))
    return ask(workstation, lambda invoke_id: wiretest.alarm.acknowledge_alarm_request(
        invoke_id, TEST_RECIPIENT_PROCESS_IDENTIFIER, *object_identifier, event_state, time_stamp, source,
        time_of_acknowledgment), args.port)


def from_lift(to_state, event_type=None):
    """A notification predicate: from Lift 1, into `to_state` (and of `event_type`, if given)."""
    def matches(notification):
        return (notification.event_object == LIFT and notification.to_state == to_state and
                (event_type is None or notification.event_type == event_type))
    return matches


def landing_door_status(client, port):
    """Landing_Door_Status[1] as {floor: door status} (BACnetLandingDoorStatus: a list of
    { floor-number [0] Unsigned8, door-status [1] BACnetDoorStatus }), or 'error'."""
    answer = read(client, port, LIFT, PROPERTY_LANDING_DOOR_STATUS, 1)
    if not answer.ok:
        return "error"
    pairs = wiretest.context_values(answer.value_octets)
    floors = [int.from_bytes(body, "big") for number, body in pairs if number == 0]
    statuses = [int.from_bytes(body, "big") for number, body in pairs if number == 1]
    return dict(zip(floors, statuses))


def registered_car_calls(client, port):
    """Registered_Car_Call[1] as a list of floors (BACnetLiftCarCallList: floor-numbers [0]
    SEQUENCE OF Unsigned8), or 'error'."""
    answer = read(client, port, LIFT, PROPERTY_REGISTERED_CAR_CALL, 1)
    if not answer.ok:
        return "error"
    return [value for value in wiretest.decoded_values(answer.value_octets)]
