#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-EM Wire Test - Failure Mode: a car door that fails to close, reported as a lift fault.

    python3 tests/car_fault_test.py [--exe build/BACnetExampleBEM] [--port 48030]

ANSI/ASHRAE 135-2024 cl. 12.59.32 Fault_Signals: "DOOR_CLOSING_FAULT  A door is failing to
close"; "This property is the value of the pMonitoredList parameter for the object's fault
algorithm". Cl. 12.59: "For reliability-evaluation, the FAULT_LISTED fault algorithm can be
applied." Cl. 13.2.5.3: "For all transitions to, or from, the FAULT state, the corresponding
event notification shall use the Event Type CHANGE_OF_RELIABILITY."

The Peer is an operator workstation (wiretest.Workstation) that the example notifies,
confirmed (the test-only recipient, BEM_TEST_EVENT_RECIPIENT_PORT). The test blocks the car's
doorway through the Simulation's control channel; everything it checks arrives over BACnet.

  1. Control: the car stands at floor 3 with its door open, Fault_Signals empty, Reliability
     NO_FAULT_DETECTED, Event_State NORMAL, Status_Flags FAULT clear.
  2. Something blocks the doorway and a passenger presses 7. The closing door re-opens.
  3. After the car's door-close attempts: Fault_Signals = { DOOR_CLOSING_FAULT }, Reliability
     FAULTS_LISTED, Event_State FAULT, Status_Flags FAULT set.
  4. The workstation receives a ConfirmedEventNotification from Lift 1: NORMAL -> FAULT, event
     type CHANGE_OF_RELIABILITY, Notification Class 1's to-fault priority 50.
  5. GetEventInformation lists Lift 1 in FAULT.
  6. The faulted car serves no stops: over several floor-travel times it stays at floor 3 and
     Registered_Car_Call still lists 7.
  7. The doorway clears: the door closes, Fault_Signals empties, Reliability NO_FAULT_DETECTED,
     a FAULT -> NORMAL notification (CHANGE_OF_RELIABILITY) arrives, and the car goes on to 7.
"""
import sys
import time

import em_setup as setup
from em_setup import DIRECTION_STOPPED, DOOR_CLOSING, DOOR_OPENED, DOOR_OPENING, LIFT
import wiretest
from wiretest import alarm, record

FAULT_PRIORITY = 50          # main.cpp NC_PRIORITY_TO_FAULT
NORMAL_PRIORITY = 200        # main.cpp NC_PRIORITY_TO_NORMAL

# A confirmed notification the workstation missed is sent again APDU_Timeout (10 s) later
# (main.cpp serves Number_Of_APDU_Retries 3), so wait long enough for one retransmission.
NOTIFICATION_WAIT = 12


def fault_state(workstation, args):
    flags = setup.lift_value(workstation, args.port, setup.PROPERTY_STATUS_FLAGS)
    return (setup.lift_values(workstation, args.port, setup.PROPERTY_FAULT_SIGNALS),
            setup.lift_value(workstation, args.port, setup.PROPERTY_RELIABILITY),
            setup.lift_value(workstation, args.port, setup.PROPERTY_EVENT_STATE),
            flags[setup.STATUS_FLAG_FAULT] if isinstance(flags, list) and len(flags) == 4 else flags)


def main():
    args = setup.parser(__doc__).parse_args()
    workstation = wiretest.Workstation()
    example = setup.start_monitor(args, recipient_port=workstation.port)
    hands = None
    try:
        hands = setup.control(args)

        # --- 1. Control: a healthy car, door open at 3 ------------------------------------
        hands.send("call 3")
        at_three = wiretest.wait_until(lambda: setup.car_snapshot(workstation, args.port) == (3, DIRECTION_STOPPED,
                                                                                             DOOR_OPENED), 6, 0.05)
        record("Control: the car stands at floor 3, door OPENED", at_three,
               f"read {setup.car_snapshot(workstation, args.port)}")
        healthy = fault_state(workstation, args)
        record("Control: Fault_Signals empty, Reliability NO_FAULT_DETECTED, Event_State NORMAL, no FAULT flag",
               healthy == ([], setup.RELIABILITY_NO_FAULT_DETECTED, alarm.EVENT_STATE_NORMAL, False),
               f"read (Fault_Signals, Reliability, Event_State, FAULT flag) {healthy}")
        record("Control: no event notification yet", workstation.notifications == [],
               f"{len(workstation.notifications)} received")

        # --- 2. The doorway is blocked; a passenger presses 7 -----------------------------
        mark = workstation.mark()
        hands.send("obstruct on")
        hands.send("call 7")
        doors = []
        for _ in range(int((setup.DWELL_MS + 3 * setup.DOOR_MS) / 50) + 20):
            snapshot = setup.car_snapshot(workstation, args.port)
            if snapshot is not None and (not doors or doors[-1] != snapshot[2]):
                doors.append(snapshot[2])
            if DOOR_CLOSING in doors and doors[-1] in (DOOR_OPENING, DOOR_OPENED) and doors.index(DOOR_CLOSING) < len(doors) - 1:
                break
            time.sleep(0.05)
        record("The closing door meets the obstruction and re-opens (CLOSING -> OPENING)",
               DOOR_CLOSING in doors and DOOR_OPENING in doors[doors.index(DOOR_CLOSING):], f"door states seen {doors}")

        # --- 3. The fault ------------------------------------------------------------------
        attempts_seconds = 3 * (setup.DWELL_MS + 2 * setup.DOOR_MS) / 1000 + 2
        faulted = wiretest.wait_until(lambda: fault_state(workstation, args)[0] == [setup.LIFT_FAULT_DOOR_CLOSING_FAULT],
                                      attempts_seconds, 0.1)
        record("Fault_Signals = { DOOR_CLOSING_FAULT }", faulted,
               f"read {setup.lift_values(workstation, args.port, setup.PROPERTY_FAULT_SIGNALS)}")
        state = fault_state(workstation, args)
        record("Reliability FAULTS_LISTED, Event_State FAULT, Status_Flags FAULT set",
               state[1:] == (setup.RELIABILITY_FAULTS_LISTED, alarm.EVENT_STATE_FAULT, True), f"read {state}")

        # --- 4. The workstation is told ---------------------------------------------------
        notification = workstation.wait_for_notification(
            setup.from_lift(alarm.EVENT_STATE_FAULT, setup.EVENT_TYPE_CHANGE_OF_RELIABILITY), NOTIFICATION_WAIT, mark)
        record("ConfirmedEventNotification from Lift 1: NORMAL -> FAULT, CHANGE_OF_RELIABILITY, priority 50",
               notification is not None and notification.confirmed and
               notification.from_state == alarm.EVENT_STATE_NORMAL and notification.priority == FAULT_PRIORITY and
               notification.notification_class == setup.NOTIFICATION_CLASS,
               notification.describe() if notification else
               f"got {[received.describe() for received in workstation.notifications_since(mark)]}")

        # --- 5. GetEventInformation -----------------------------------------------------------
        summaries, answer = setup.get_event_information(workstation, args)
        entry = setup.summary_for(summaries, LIFT)
        record("GetEventInformation lists Lift 1 in FAULT",
               entry is not None and entry.get("event_state") == alarm.EVENT_STATE_FAULT,
               f"summary {entry}" if summaries is not None else answer.describe())

        # --- 6. A faulted car goes nowhere ------------------------------------------------
        time.sleep(4 * setup.TRAVEL_MS / 1000)
        snapshot = setup.car_snapshot(workstation, args.port)
        record("Faulted: after 4 floor-travel times the car is still at floor 3, STOPPED",
               snapshot is not None and snapshot[:2] == (3, DIRECTION_STOPPED), f"read {snapshot}")
        record("Faulted: the car call to 7 is still registered, not served",
               setup.registered_car_calls(workstation, args.port) == [7],
               f"read {setup.registered_car_calls(workstation, args.port)}")

        # --- 7. The doorway clears ------------------------------------------------------------
        mark = workstation.mark()
        hands.send("obstruct off")
        cleared = wiretest.wait_until(lambda: fault_state(workstation, args)[:3] == (
            [], setup.RELIABILITY_NO_FAULT_DETECTED, alarm.EVENT_STATE_NORMAL), (setup.DWELL_MS + 2 * setup.DOOR_MS) / 1000 + 2, 0.1)
        record("Doorway clear, door closed: Fault_Signals empty, Reliability NO_FAULT_DETECTED, Event_State NORMAL",
               cleared, f"read {fault_state(workstation, args)}")
        notification = workstation.wait_for_notification(
            setup.from_lift(alarm.EVENT_STATE_NORMAL, setup.EVENT_TYPE_CHANGE_OF_RELIABILITY), NOTIFICATION_WAIT, mark)
        record("ConfirmedEventNotification from Lift 1: FAULT -> NORMAL, CHANGE_OF_RELIABILITY",
               notification is not None and notification.from_state == alarm.EVENT_STATE_FAULT and
               notification.priority == NORMAL_PRIORITY,
               notification.describe() if notification else
               f"got {[received.describe() for received in workstation.notifications_since(mark)]}")
        arrived = wiretest.wait_until(lambda: (setup.car_snapshot(workstation, args.port) or (None,))[0] == 7,
                                      6 * setup.TRAVEL_MS / 1000 + 2, 0.1)
        record("Back in service, the car goes on to floor 7", arrived,
               f"read {setup.car_snapshot(workstation, args.port)}")
    finally:
        if hands is not None:
            hands.close()
        workstation.close()
        example.stop()
        if not all(wiretest.results.RESULTS):
            example.keep_log()
    return wiretest.summary()


if __name__ == "__main__":
    sys.exit(main())
