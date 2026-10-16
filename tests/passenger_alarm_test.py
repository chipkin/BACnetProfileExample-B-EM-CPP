#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-EM Wire Test - the passenger alarm: reported, listed, acknowledged; and what a monitor refuses.

    python3 tests/passenger_alarm_test.py [--exe build/BACnetExampleBEM] [--port 48030]

ANSI/ASHRAE 135-2024 cl. 12.59: "Lift objects that support intrinsic reporting shall apply
the CHANGE_OF_STATE event algorithm on the Passenger_Alarm property. The pAlarmValues
parameter shall contain the value TRUE." L.13.3: a B-EM "supports presentation of its
elevator objects by another device, but is not required to support modifications" - this
one accepts none. L.13.3 Alarm and Event Management includes "Notifying other recipients
that the acknowledgment has been received".

The Peer is an operator workstation (wiretest.Workstation), Crimson's confirmed recipient
(test-only, BEM_TEST_EVENT_RECIPIENT_PORT). The test presses the car's alarm button through
the Simulation's control channel.

  1. Control: Passenger_Alarm FALSE, Event_State NORMAL, nothing in GetEventInformation.
  2. A passenger presses the alarm button: Passenger_Alarm TRUE, Event_State OFFNORMAL,
     Status_Flags IN_ALARM, and a ConfirmedEventNotification NORMAL -> OFFNORMAL, event type
     CHANGE_OF_STATE, priority 100, ack required, reaches the workstation.
  3. GetEventInformation lists Lift 1, OFFNORMAL, to-offnormal unacknowledged.
  4. The operator acknowledges (AE-ACK-B): SimpleACK, Acked_Transitions to-offnormal TRUE,
     and an ACK_NOTIFICATION for that transition reaches the recipients.
  5. The button is released: Event_State NORMAL and a to-normal notification arrives.
  6. Failure Modes: an AcknowledgeAlarm for the Escalator (which has no alarm) is refused,
     and so is every WriteProperty - Car_Position, Passenger_Alarm, Out_Of_Service - because a
     monitor accepts no modifications. Car_Position is unchanged afterwards.
"""
import sys

import em_setup as setup
from em_setup import ESCALATOR, LIFT
import wiretest
from wiretest import alarm, application_boolean, application_unsigned, record

OFFNORMAL_PRIORITY = 100     # main.cpp NC_PRIORITY_TO_OFFNORMAL

# A confirmed notification the workstation missed is sent again APDU_Timeout (10 s) later
# (main.cpp serves Number_Of_APDU_Retries 3), so wait long enough for one retransmission.
NOTIFICATION_WAIT = 12


def alarm_state(workstation, args):
    flags = setup.lift_value(workstation, args.port, setup.PROPERTY_STATUS_FLAGS)
    return (setup.lift_value(workstation, args.port, setup.PROPERTY_PASSENGER_ALARM),
            setup.lift_value(workstation, args.port, setup.PROPERTY_EVENT_STATE),
            flags[setup.STATUS_FLAG_IN_ALARM] if isinstance(flags, list) and len(flags) == 4 else flags)


def refused(answer):
    return answer.answered and answer.frame.pdu_type in (wiretest.PDU_ERROR, wiretest.PDU_REJECT)


def main():
    args = setup.parser(__doc__).parse_args()
    workstation = wiretest.Workstation()
    example = setup.start_monitor(args, recipient_port=workstation.port)
    hands = None
    try:
        hands = setup.control(args)

        # --- 1. Control -------------------------------------------------------------------
        state = alarm_state(workstation, args)
        record("Control: Passenger_Alarm FALSE, Event_State NORMAL, IN_ALARM clear",
               state == (False, alarm.EVENT_STATE_NORMAL, False), f"read {state}")
        summaries, answer = setup.get_event_information(workstation, args)
        record("Control: GetEventInformation lists nothing", summaries == [],
               f"read {summaries}" if summaries is not None else answer.describe())

        # --- 2. The alarm button ------------------------------------------------------------
        mark = workstation.mark()
        hands.send("alarm on")
        notification = workstation.wait_for_notification(
            setup.from_lift(alarm.EVENT_STATE_OFFNORMAL, setup.EVENT_TYPE_CHANGE_OF_STATE), NOTIFICATION_WAIT, mark)
        state = alarm_state(workstation, args)
        record("Alarm pressed: Passenger_Alarm TRUE, Event_State OFFNORMAL, IN_ALARM set",
               state == (True, alarm.EVENT_STATE_OFFNORMAL, True), f"read {state}")
        record("ConfirmedEventNotification from Lift 1: NORMAL -> OFFNORMAL, CHANGE_OF_STATE, priority 100, "
               "ack required",
               notification is not None and notification.confirmed and
               notification.from_state == alarm.EVENT_STATE_NORMAL and notification.priority == OFFNORMAL_PRIORITY and
               notification.ack_required and notification.notify_type == alarm.NOTIFY_TYPE_ALARM,
               notification.describe() if notification else
               f"got {[received.describe() for received in workstation.notifications_since(mark)]}")

        # --- 3. GetEventInformation -------------------------------------------------------------
        summaries, answer = setup.get_event_information(workstation, args)
        entry = setup.summary_for(summaries, LIFT)
        record("GetEventInformation lists Lift 1: OFFNORMAL, to-offnormal unacknowledged",
               entry is not None and entry.get("event_state") == alarm.EVENT_STATE_OFFNORMAL and
               entry.get("acked_transitions", [None])[alarm.TO_OFFNORMAL] is False,
               f"summary {entry}" if summaries is not None else answer.describe())

        # --- 4. The operator acknowledges -------------------------------------------------------
        if notification is not None:
            answer = setup.acknowledge(workstation, args, LIFT, alarm.EVENT_STATE_OFFNORMAL, notification.time_stamp)
            record("AcknowledgeAlarm for Lift 1's to-offnormal is accepted",
                   answer.answered and answer.frame.pdu_type == wiretest.PDU_SIMPLE_ACK, answer.describe())
        else:
            record("AcknowledgeAlarm for Lift 1's to-offnormal is accepted", False, "no notification to acknowledge")
        acked = setup.lift_value(workstation, args.port, setup.PROPERTY_ACKED_TRANSITIONS)
        record("Acked_Transitions: to-offnormal acknowledged",
               isinstance(acked, list) and acked[alarm.TO_OFFNORMAL], f"read {acked}")
        acknowledgment = workstation.wait_for_notification(
            lambda received: received.event_object == LIFT and received.notify_type == alarm.NOTIFY_TYPE_ACK_NOTIFICATION,
            NOTIFICATION_WAIT, mark)
        record("The recipients are told: an ACK_NOTIFICATION for Lift 1's to-offnormal arrives",
               acknowledgment is not None and acknowledgment.to_state == alarm.EVENT_STATE_OFFNORMAL,
               acknowledgment.describe() if acknowledgment else
               f"got {[received.describe() for received in workstation.notifications_since(mark)]}")

        # --- 5. Released ----------------------------------------------------------------------------
        mark = workstation.mark()
        hands.send("alarm off")
        notification = workstation.wait_for_notification(
            setup.from_lift(alarm.EVENT_STATE_NORMAL, setup.EVENT_TYPE_CHANGE_OF_STATE), NOTIFICATION_WAIT, mark)
        state = alarm_state(workstation, args)
        record("Alarm released: Passenger_Alarm FALSE, Event_State NORMAL",
               state[:2] == (False, alarm.EVENT_STATE_NORMAL), f"read {state}")
        record("ConfirmedEventNotification from Lift 1: OFFNORMAL -> NORMAL",
               notification is not None and notification.from_state == alarm.EVENT_STATE_OFFNORMAL,
               notification.describe() if notification else
               f"got {[received.describe() for received in workstation.notifications_since(mark)]}")

        # --- 6. Failure Modes ------------------------------------------------------------------------
        if notification is not None:
            answer = setup.acknowledge(workstation, args, ESCALATOR, alarm.EVENT_STATE_NORMAL, notification.time_stamp)
            record("AcknowledgeAlarm for Escalator 1 (no alarm there) is refused", refused(answer), answer.describe())
        position = setup.lift_value(workstation, args.port, setup.PROPERTY_CAR_POSITION)
        for name, property_identifier, value in (
                ("Car_Position", setup.PROPERTY_CAR_POSITION, application_unsigned(6)),
                ("Passenger_Alarm", setup.PROPERTY_PASSENGER_ALARM, application_boolean(True)),
                ("Out_Of_Service", wiretest.PROPERTY_OUT_OF_SERVICE, application_boolean(True))):
            answer = setup.write(workstation, args.port, LIFT, property_identifier, value)
            record(f"Read-only monitor: WriteProperty Lift 1 {name} is refused", refused(answer), answer.describe())
        record("Car_Position unchanged by the refused write",
               setup.lift_value(workstation, args.port, setup.PROPERTY_CAR_POSITION) == position)
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
