#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-EM Wire Test - the Profile Function: a monitoring workstation watches the lift car move.

    python3 tests/car_monitor_test.py [--exe build/BACnetExampleBEM] [--port 48030]

ANSI/ASHRAE 135-2024 L.13.3: the B-EM "performs monitoring of an elevator control system".
The Peer is a Python operator workstation. It learns everything over BACnet: one
SubscribeCOVPropertyMultiple (DS-COVM-B) on the car, and ReadPropertyMultiple snapshots
(DS-RPM-B) of Car_Position (cl. 12.59.14), Car_Moving_Direction (12.59.15) and
Car_Door_Status (12.59.17). The test plays the passengers through the Simulation's control
channel: it presses floor buttons inside the car, nothing else.

  1. Control: nobody has pressed anything - the car stands at floor 1, STOPPED, door CLOSED,
     and stays there. The COV subscription is accepted and its first notification says floor 1.
  2. A passenger presses 5. Registered_Car_Call (12.59.13) lists 5.
  3. The trip up, as the COV subscriber hears it: Car_Position 1, 2, 3, 4, 5 - one floor at a
     time - and the door SAFETY_LOCKED -> OPENING -> OPENED -> CLOSING -> CLOSED; direction UP
     then STOPPED. Every RPM snapshot taken while the car moves is UP with the door
     SAFETY_LOCKED: one RPM is one consistent answer, so the car never moves with its door open.
     The trip takes at least four floor-travel times (the car's set speed) and arrives.
  4. At 5, doors open: Landing_Door_Status (12.59.33) shows floor 5's landing door OPENED and
     every other floor's SAFETY_LOCKED, and the car call is gone from Registered_Car_Call.
  5. A passenger presses 2: the COV subscriber hears Car_Position 5, 4, 3, 2 and direction DOWN.

The ordered checks come from the COV notifications: the operating system queues them for
the subscriber's socket, so a test process the machine stalls for a moment still sees every
one, in order. A polled read can miss a floor; a notification cannot be skipped that way.
"""
import sys
import time

import em_setup as setup  # first: it puts the vendored wiretest/ on sys.path
from em_setup import (DIRECTION_DOWN, DIRECTION_STOPPED, DIRECTION_UP, DOOR_CLOSED, DOOR_CLOSING, DOOR_OPENED,
                      DOOR_OPENING, DOOR_SAFETY_LOCKED, LIFT)
import wiretest
from wiretest import record

COV_PROCESS_IDENTIFIER = 7


class Subscriber:
    """The workstation's COV subscription on the car, and the changes it has heard, in order."""

    def __init__(self):
        self.client = setup.client()
        self.heard = []          # [(property identifier, value)], in arrival order

    def subscribe(self, port):
        return self.client.request(lambda invoke_id: wiretest.services.subscribe_cov_property_multiple_request(
            invoke_id, COV_PROCESS_IDENTIFIER, False, 600, [(*LIFT, list(setup.CAR_PROPERTIES))]), port)

    def listen(self, seconds):
        """Take in every notification that has arrived (and any in the next `seconds`);
        returns the (property, value) changes they carried, in order."""
        self.client.frames = []
        self.client.collect(seconds)
        changes = []
        for frame in self.client.frames:
            if frame.ok and wiretest.services.is_cov_notification_multiple(frame):
                notification = wiretest.services.CovNotificationMultiple(frame)
                if (notification.process_identifier, notification.initiating_device) != (
                        COV_PROCESS_IDENTIFIER, (wiretest.OBJECT_TYPE_DEVICE, setup.DEVICE_INSTANCE)):
                    changes.append(("stranger", notification.process_identifier))
                    continue
                for monitored, property_identifier, _, octets in notification.values:
                    if monitored == LIFT:
                        changes.append((property_identifier, wiretest.decode_application(octets)))
        self.heard.extend(changes)
        return changes

    def close(self):
        self.client.close()


def values(changes, property_identifier):
    """The values of one property in `changes`, consecutive repeats collapsed."""
    collapsed = []
    for identifier, value in changes:
        if identifier == property_identifier and (not collapsed or collapsed[-1] != value):
            collapsed.append(value)
    return collapsed


def watch(client, port, until, seconds):
    """RPM snapshots of the car until `until(snapshot)` or `seconds` pass; [(position, direction, door)]."""
    seen = []
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        snapshot = setup.car_snapshot(client, port)
        if snapshot is not None:
            seen.append(snapshot)
            if until(snapshot):
                break
        time.sleep(0.05)
    return seen


def trip(name, workstation, subscriber, hands, args, start, finish, direction):
    """Press `finish` in the car standing at `start`; check the trip."""
    pressed = time.monotonic()
    hands.send(f"call {finish}")
    calls = setup.registered_car_calls(workstation, args.port)
    record(f"{name}: a passenger presses {finish} - Registered_Car_Call lists {finish}", calls == [finish],
           f"read {calls}")
    floors = abs(finish - start)
    seen = watch(workstation, args.port, lambda snapshot: snapshot[:2] == (finish, DIRECTION_STOPPED),
                 3 * floors * setup.TRAVEL_MS / 1000 + 4)
    took = time.monotonic() - pressed
    arrived = bool(seen) and seen[-1][:2] == (finish, DIRECTION_STOPPED)
    record(f"{name}: the car arrives at {finish}, STOPPED", arrived, f"last snapshot {seen[-1] if seen else None}")
    record(f"{name}: the trip takes at least {floors} floor-travel times ({floors * setup.TRAVEL_MS} ms) - "
           f"the car's set speed", took >= floors * setup.TRAVEL_MS / 1000, f"took {took * 1000:.0f} ms")
    moving = [snapshot for snapshot in seen if snapshot[1] != DIRECTION_STOPPED]
    record(f"{name}: every RPM snapshot of the moving car is {'UP' if direction == DIRECTION_UP else 'DOWN'} "
           "with the door SAFETY_LOCKED",
           bool(moving) and all(snapshot[1] == direction and snapshot[2] == DOOR_SAFETY_LOCKED for snapshot in moving),
           f"{len(moving)} moving snapshot(s): {sorted(set(moving))}")


def main():
    args = setup.parser(__doc__).parse_args()
    example = setup.start_monitor(args)
    workstation = setup.client()      # retries a lost request, visibly
    subscriber = Subscriber()
    hands = None
    try:
        hands = setup.control(args)

        # --- 1. Control: nothing pressed, nothing moves -----------------------------------
        first = setup.car_snapshot(workstation, args.port)
        record("Control: the car stands at floor 1, STOPPED, door CLOSED",
               first == (1, DIRECTION_STOPPED, DOOR_CLOSED), f"read (position, direction, door) {first}")
        answer = subscriber.subscribe(args.port)
        record("SubscribeCOVPropertyMultiple on the car (Car_Position, Car_Moving_Direction, Car_Door_Status) "
               "is accepted", answer.answered and answer.frame.pdu_type == wiretest.PDU_SIMPLE_ACK, answer.describe())
        time.sleep(4 * setup.TRAVEL_MS / 1000)
        initial = subscriber.listen(0.2)
        record("The subscription's first notification: floor 1, STOPPED, door CLOSED; then nothing (no call, no move)",
               initial == [(setup.PROPERTY_CAR_POSITION, 1), (setup.PROPERTY_CAR_MOVING_DIRECTION, DIRECTION_STOPPED),
                           (setup.PROPERTY_CAR_DOOR_STATUS, DOOR_CLOSED)], f"heard {initial}")
        still = setup.car_snapshot(workstation, args.port)
        record("Control: with no call the car is still at floor 1", still == first, f"read {still}")
        record("Control: Registered_Car_Call is empty", setup.registered_car_calls(workstation, args.port) == [])

        # --- 2 and 3. A passenger presses 5: the trip up --------------------------------------
        trip("Up 1 -> 5", workstation, subscriber, hands, args, 1, 5, DIRECTION_UP)

        # --- 4. At floor 5, doors open ------------------------------------------------------
        watch(workstation, args.port, lambda snapshot: snapshot[2] == DOOR_OPENED, 2)
        landing = setup.landing_door_status(workstation, args.port)
        record("Doors open at 5: Landing_Door_Status shows floor 5 OPENED, the other floors SAFETY_LOCKED",
               isinstance(landing, dict) and landing.get(5) == DOOR_OPENED and
               sorted(landing) == list(range(setup.LOWEST_FLOOR, setup.HIGHEST_FLOOR + 1)) and
               all(status == DOOR_SAFETY_LOCKED for floor, status in landing.items() if floor != 5),
               f"read {landing}")
        record("Arrived: the car call to 5 is cleared from Registered_Car_Call",
               setup.registered_car_calls(workstation, args.port) == [],
               f"read {setup.registered_car_calls(workstation, args.port)}")
        watch(workstation, args.port, lambda snapshot: snapshot[2] == DOOR_CLOSED,
              (setup.DWELL_MS + 2 * setup.DOOR_MS) / 1000 + 3)
        up = subscriber.listen(0.3)
        record("COV, up: Car_Position 2, 3, 4, 5 - one floor at a time",
               values(up, setup.PROPERTY_CAR_POSITION) == [2, 3, 4, 5], f"heard {values(up, setup.PROPERTY_CAR_POSITION)}")
        record("COV, up: Car_Moving_Direction UP, then STOPPED at 5",
               values(up, setup.PROPERTY_CAR_MOVING_DIRECTION) == [DIRECTION_UP, DIRECTION_STOPPED],
               f"heard {values(up, setup.PROPERTY_CAR_MOVING_DIRECTION)}")
        record("COV, up: the door SAFETY_LOCKED -> OPENING -> OPENED -> CLOSING -> CLOSED",
               values(up, setup.PROPERTY_CAR_DOOR_STATUS) == [DOOR_SAFETY_LOCKED, DOOR_OPENING, DOOR_OPENED,
                                                               DOOR_CLOSING, DOOR_CLOSED],
               f"heard {values(up, setup.PROPERTY_CAR_DOOR_STATUS)}")
        record("COV: every notification was for this subscriber, from Device 389015",
               not any(identifier == "stranger" for identifier, _ in subscriber.heard))

        # --- 5. Down to 2 -----------------------------------------------------------------------
        trip("Down 5 -> 2", workstation, subscriber, hands, args, 5, 2, DIRECTION_DOWN)
        down = subscriber.listen(0.3)
        record("COV, down: Car_Position 4, 3, 2 - one floor at a time",
               values(down, setup.PROPERTY_CAR_POSITION) == [4, 3, 2],
               f"heard {values(down, setup.PROPERTY_CAR_POSITION)}")
        record("COV, down: Car_Moving_Direction DOWN, then STOPPED at 2",
               values(down, setup.PROPERTY_CAR_MOVING_DIRECTION) == [DIRECTION_DOWN, DIRECTION_STOPPED],
               f"heard {values(down, setup.PROPERTY_CAR_MOVING_DIRECTION)}")
    finally:
        if hands is not None:
            hands.close()
        workstation.close()
        subscriber.close()
        example.stop()
        if not all(wiretest.results.RESULTS):
            example.keep_log()
    return wiretest.summary()


if __name__ == "__main__":
    sys.exit(main())
