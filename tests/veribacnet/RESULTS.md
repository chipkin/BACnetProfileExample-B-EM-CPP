# VeriBACnet results

The latest VeriBACnet run of this folder's project, summarised by hand from
that run's `report.json`. The run's own Results Folder (reports, packet
captures, wire logs) is not published: it holds the testing machine's network
addresses.

| | |
|---|---|
| Date | 2026-10-27 |
| Example | BACnet B-EM (Elevator Monitor) Example - C++ v1.2.0 (`common/` 3.5.0), CAS BACnet Stack 6.x `69b6249f` |
| Profile | B-EM (BACnet Elevator Monitor) |
| Transport | BACnet/IP, UDP port 47832 |
| VeriBACnet | 0.20.0 (its test device uses CAS BACnet Stack 6.0.23.0) |
| Test plan | 171 BTL tests (ANSI/ASHRAE 135.1-2023) selected for the B-EM profile and the options this example claims |
| Result | **71 PASS · 0 FAIL · 0 ERROR · 100 SKIP** (two runs, identical row for row) |

**Judged BTL-clean on BACnet/IP, with a star**: no failure or error is the
example's own fault, every other failure is tracked, and every skipped test has
a stated reason. The star means some of the functions the example exists for
were not measured by VeriBACnet in this run (below). See "What BTL-clean means"
in [README.md](README.md). This is not BTL certification.

**Measured coverage:** 71 of the 171 selected tests ran (71 passed, none failed).
Of the 100 skipped, 35 wait on a test-tool or test-script fix, or on scenario
data the tool cannot yet work out for itself (tracked), 36 need an operator,
and 29 do not apply to this device (it executes no WritePropertyMultiple,
TimeSynchronization or ReinitializeDevice, cannot create, delete or rename
objects over BACnet, has no Event Enrollment object, has only one
event-reporting object, and refuses the deprecated DeviceCommunicationControl
DISABLE option, as its Protocol_Revision requires).

**Measured now:** ReadProperty and ReadPropertyMultiple (all 24 tests passed),
COV subscriptions on Analog Input 1 (DS-COV-B: confirmed and unconfirmed
notifications, updating a subscription, 8-hour lifetimes and the
Active_COV_Subscriptions property all passed), COV-multiple subscriptions
(DS-COVM-B: 9 tests passed), Who-Is and Who-Has (24 of 26 passed; the other 2
need an operator), GetEventInformation, the Read Support and EPICS consistency
tests, and the Multi-state Input's State_Text.

**Not measured yet:** event notification (AE-N-I-B), alarm acknowledgement
(AE-ACK-B) and DeviceCommunicationControl disable-initiation (DM-DCC-B). The
alarm source is the lift car's passenger alarm button, so those tests need an
operator to press it (or to obstruct the door); the run was unattended. The
other AcknowledgeAlarm tests compare against values only a lab setup can
supply, and the disable-initiation tests need a value the test tool can change
and watch, which a read-only monitor does not offer. All of it is tracked. This
example's own Wire Tests cover these functions: the lift car followed over COV
and ReadPropertyMultiple, the passenger alarm (notification,
GetEventInformation, AcknowledgeAlarm), the door fault (to-fault and to-normal
notifications) and the alarm recipients written to Notification Class 1.

This run replaces the 2026-10-26 run (an earlier VeriBACnet build), which
measured 73 of 171 with three failures. The three failures are gone: the test
tool now stops the two COV threshold tests at an operator step instead of
failing a read-only input, and skips the WriteProperty test when the only
writable property is a list it cannot vary. Two tests fewer ran (the two COV
tests that became operator steps), and one more passed (9.42.1.2).

Earlier changes, since the 2026-10-24 run (CAS BACnet Stack 6.x `29b83de6`,
77 of 142 measured):

- the example moved to CAS BACnet Stack 6.x `69b6249f`;
- Analog Input 1 gained its COV_Increment (1.0), which BACnet requires on an
  Analog Input that supports COV reporting;
- Notification Class 1's Recipient_List became writable, so the alarm
  recipients can be set at commissioning. BACnet requires a fixed default only
  for a read-only list. It is the one property this monitor accepts a write
  to; every other write is still refused;
- the project now declares the Test Plan options the example claims, so 29
  more tests were selected;
- the test tool now takes its COV object from the device, so the COV tests
  that failed before (they looked for an Analog Value) pass on Analog Input 1.
  The AcknowledgeAlarm and disable-initiation tests that failed on the same
  kind of script assumption now stop at an operator step instead.

## Tests that did not pass

None. No test failed or errored in either run.

## Skipped tests, by reason

| Tests | Why |
|---|---|
| 9.1.1.8-.11, 9.1.1.15, 9.1.2.x (6) and 7.3.1.11.1 AcknowledgeAlarm | They compare against expected values (process identifiers, priorities, time stamps) that the tool cannot work out from the device and that a live device cannot fix in advance. Tracked. |
| 9.42.1.3-.11 and 9.42.2.6, .7 SubscribeCOVPropertyMultiple (11) | Need scenario data the tool does not work out for a device it did not build (the values a notification will carry, subscriptions captured from a reference device, an array property, a small subscription capacity). Tracked. |
| 7.3.2.21.3.4, .5, .6 Notification Class recipients, 7.3.1.10.1 | Need scenario data the tool does not yet accept for a device that reports through intrinsic reporting. Tracked. |
| 8.2.1, 8.2.2, 8.3.1, 8.3.2 COV | Reach Analog Input 1 (COV_Increment read, subscription accepted, first notification received) and then need the input's value or status to change, which is an operator action on a read-only input. |
| btl-auto 9.8.2 / 9.8.3 | The scripts are written for the reference device's Analog Value object, which this device does not have. Tracked. |
| 9.22.1.5 WriteProperty | The only writable property is the Notification Class Recipient_List, a list the test tool cannot vary. Tracked. |
| 9.1.1.1-.6, 9.1.1.14, 7.3.1.19.x, 7.3.1.20.x, 7.3.1.22.x, 7.3.2.35.x, 7.3.2.36.x, 8.4.17.10, 8.5.17.10, 8.4.17.12.x, 8.5.17.12.x, 7.2.8 | Need an operator to raise an alarm or a fault on the lift car; the run was unattended. |
| 9.10.1.4, .5, .7, .11 (COV) | Look for a writable REAL point of a COV-reporting type to change while the test watches; this monitor has none, since its values change by themselves or at the console. Tracked. |
| 9.24.1.6, .8, .9, .10 (DCC disable-initiation) | Need a value to change while the test watches. This monitor's values change by themselves or at the console, not by a write, so an operator has to make the change. |
| 7.3.2.10.9 / 7.3.2.10.10 Device Object_Name / Object_Identifier configurable, 9.32.1.12 / 9.32.1.13 Who-Has after a change | Need an operator: the name and instance are configured by restarting the device with `--deviceName` / `--deviceID`, which an unattended run cannot do. |
| 7.3.2.10.3 / .4 / .5 / .6 Database_Revision after a create, delete or rename | The device cannot create, delete or rename objects over BACnet; the clause skips the tests then. |
| 7.3.2.21.3.1, .2, .7, 9.10.1.9, 9.23.2.20 | Need TimeSynchronization, ReinitializeDevice or WritePropertyMultiple, which this monitor does not execute, by design. |
| 7.3.1.15, 9.8.2-.5, 8.4.2 / 8.5.2 forms (10) | Need a writable Multi-state object, an Event Enrollment object, or another kind of event-reporting object; this device has none. |
| 7.3.1.12 Notify_Type Test | Needs one event-reporting object that reports alarms and another that reports events; this device has only the Lift, which reports alarms. |
| 9.24.1.1, .3, .4 | Test the DeviceCommunicationControl DISABLE option, which this Protocol_Revision deprecates; the device correctly refuses it (9.24.1.11 passed). |
| 9.8.6 GetEventInformation chaining | The clause's own skip: the device's event list fits in one answer. |

Four more B-EM requirements have no runnable VeriBACnet test yet, so they were
not run: the 7.3.1.11.3 and 7.3.1.11.4 Acked_Transitions tests, and the 8.4 and
8.5 event notification initiation tests (the tool does not yet resolve "any of
the 8.4 / 8.5 tests"). All tracked.

DeviceCommunicationControl was tested without a password: this example has no
`--dcc-password` option, and its DCC password is a constant in `main.cpp` that
ships empty.

No public issue has been opened for any of these.
