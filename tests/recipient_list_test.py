#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-EM Wire Test - commissioning the alarm recipients: Notification Class 1's Recipient_List.

    python3 tests/recipient_list_test.py [--exe build/BACnetExampleBEM] [--port 47832]

ANSI/ASHRAE 135-2024 cl. 12.21 (Recipient_List): "If the Recipient_List is not writable and the
device is not using local Notification Forwarder objects, then the Recipient_List shall have a
length of 1 with the Recipient set to a local broadcast, ... Process Identifier set to 0, ...".
This device's Recipient_List IS writable - the one property a B-EM accepts a write to - so a
commissioning tool chooses who hears the alarms, and the start-up entry (unconfirmed, to the
subnet's broadcast address) is only its default.

The Peer is an operator workstation (wiretest.Workstation). The example starts WITHOUT the
test-only recipient (BEM_TEST_EVENT_RECIPIENT_PORT), so the workstation hears an alarm only if
the Recipient_List it writes names it.

  1. Control: Recipient_List reads the start-up default, one entry.
  2. The workstation writes Recipient_List = the default + itself (confirmed, Process
     Identifier 2, all transitions): SimpleACK, and it reads back exactly as written.
  3. A passenger presses the alarm button: a ConfirmedEventNotification NORMAL -> OFFNORMAL
     from Lift 1 reaches the workstation; the button is released: OFFNORMAL -> NORMAL.
  4. The default is written back: SimpleACK, reads back as at start-up, and the next alarm
     reaches the workstation no more (control: the list decides who hears it).
  5. Failure Modes: a Recipient_List write with the wrong datatype is refused and changes
     nothing; WriteProperty to any other property is still refused (passenger_alarm_test.py).
"""
import sys

import em_setup as setup
from em_setup import LIFT
import wiretest
from wiretest import alarm, application_unsigned, record

OBJECT_TYPE_NOTIFICATION_CLASS = 15
CRIMSON = (OBJECT_TYPE_NOTIFICATION_CLASS, setup.NOTIFICATION_CLASS)   # Notification Class 1 "Crimson"
WORKSTATION_PROCESS_IDENTIFIER = 2
NOTIFICATION_WAIT = 12       # one APDU_Timeout retransmission (passenger_alarm_test.py)
QUIET_WAIT = 4


def read_list(workstation, args):
    return workstation.read_property(args.port, *CRIMSON, alarm.PROPERTY_RECIPIENT_LIST)


def write_list(workstation, args, value_octets):
    return workstation.write_property(args.port, *CRIMSON, alarm.PROPERTY_RECIPIENT_LIST, value_octets)


def acked(answer):
    return answer.answered and answer.frame.pdu_type == wiretest.PDU_SIMPLE_ACK


def refused(answer):
    return answer.answered and answer.frame.pdu_type in (wiretest.PDU_ERROR, wiretest.PDU_REJECT)


def main():
    args = setup.parser(__doc__).parse_args()
    workstation = setup.workstation()
    example = setup.start_monitor(args)
    hands = None
    try:
        hands = setup.control(args)

        # --- 1. Control: the start-up default ----------------------------------------------------
        default = read_list(workstation, args)
        default_octets = default.value_octets if default.ok else None
        record("Control: Recipient_List reads (the start-up default)", default_octets is not None, default.describe())
        record("Control: the default does not name the workstation",
               default_octets is not None and workstation.mac not in default_octets, default.describe())
        if default_octets is None:
            return wiretest.summary()

        # --- 2. Commissioning: the workstation adds itself ------------------------------------
        written = default_octets + alarm.destination(workstation.mac, WORKSTATION_PROCESS_IDENTIFIER, True)
        answer = write_list(workstation, args, written)
        record("WriteProperty Recipient_List (default + the workstation, confirmed) is accepted", acked(answer),
               answer.describe())
        readback = read_list(workstation, args)
        record("Recipient_List reads back exactly as written", readback.ok and readback.value_octets == written,
               readback.describe())

        # --- 3. The written list is the one used -------------------------------------------------
        mark = workstation.mark()
        hands.send("alarm on")
        notification = workstation.wait_for_notification(
            setup.from_lift(alarm.EVENT_STATE_OFFNORMAL, setup.EVENT_TYPE_CHANGE_OF_STATE), NOTIFICATION_WAIT, mark)
        record("ConfirmedEventNotification from Lift 1 (NORMAL -> OFFNORMAL) reaches the written recipient",
               notification is not None and notification.confirmed and
               notification.process_identifier == WORKSTATION_PROCESS_IDENTIFIER,
               notification.describe() if notification else
               f"got {[received.describe() for received in workstation.notifications_since(mark)]}")
        mark = workstation.mark()
        hands.send("alarm off")
        notification = workstation.wait_for_notification(
            setup.from_lift(alarm.EVENT_STATE_NORMAL, setup.EVENT_TYPE_CHANGE_OF_STATE), NOTIFICATION_WAIT, mark)
        record("ConfirmedEventNotification from Lift 1 (OFFNORMAL -> NORMAL) reaches the written recipient",
               notification is not None and notification.from_state == alarm.EVENT_STATE_OFFNORMAL,
               notification.describe() if notification else
               f"got {[received.describe() for received in workstation.notifications_since(mark)]}")

        # --- 4. Back to the default ------------------------------------------------------------
        answer = write_list(workstation, args, default_octets)
        record("WriteProperty Recipient_List (the default again) is accepted", acked(answer), answer.describe())
        readback = read_list(workstation, args)
        record("Recipient_List reads back as at start-up", readback.ok and readback.value_octets == default_octets,
               readback.describe())
        mark = workstation.mark()
        hands.send("alarm on")
        silence = workstation.wait_for_notification(lambda received: received.event_object == LIFT, QUIET_WAIT, mark)
        record("Control: the workstation, no longer on the list, hears no alarm", silence is None,
               silence.describe() if silence else f"nothing in {QUIET_WAIT} s")
        hands.send("alarm off")

        # --- 5. Failure Modes ----------------------------------------------------------------------
        answer = write_list(workstation, args, application_unsigned(5))
        record("WriteProperty Recipient_List with the wrong datatype (Unsigned) is refused", refused(answer),
               answer.describe())
        readback = read_list(workstation, args)
        record("Recipient_List unchanged by the refused write",
               readback.ok and readback.value_octets == default_octets, readback.describe())
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
