# TODO

Defects this example cannot fix itself. A defect that stops a Wire Test check is
reported as a `[KNOWN GAP]` line ([`tests/`](tests/README.md)): reported on every
run, not counted as a failure. **There are none at Stack Pin `6.x` @ `69b6249f`**
- every check of the Profile Function and its Failure Modes passes.

## Observed in the stack, not affecting a check

### 1. `UpdateValue(Fault_Signals)` logs a CHANGE_OF_STATE error

Filed: [#3967](https://github.com/chipkin/cas-bacnet-stack/issues/3967). Verification found it is more than a log line: after a fault
clears, CHANGE_OF_STATE is evaluated on the now-empty Fault_Signals list instead of
Passenger_Alarm, so a Passenger_Alarm that stayed TRUE through the fault is not re-judged.


- **What:** Lift 1 arms both CHANGE_OF_STATE (on `Passenger_Alarm`) and
  FAULT_LISTED (on `Fault_Signals`), as cl. 12.59 describes. The
  `BACnetStack_UpdateValue(..., Lift, 1, Fault_Signals)` that CLEARS a fault
  (FAULT -> NORMAL; not the one that raises it) makes the stack's debug log print
  `BACnetChangeOfStateEventAlgorithm::SetMonitoredValue() - Error: monitoredValue has invalid datatype=[255]`,
  then `Run() - Error: failed to set the monitoredValue` and
  `BACnetStackAlarmAndEventObject::CheckAndRunAlgorithms() - Error: error occurrend when running the event algorithm`:
  on the way back from FAULT, the CHANGE_OF_STATE algorithm appears to be run
  with the fault algorithm's monitored list (the property passed to
  `UpdateValue`) as its monitored value.
- **Effect here:** none seen. The FAULT_LISTED transitions, `Reliability`,
  `Event_State` and both notifications are right (`tests/car_fault_test.py`), and
  `Passenger_Alarm` still alarms afterwards. Not tested: a fault and a
  passenger alarm at the same time.
- **Evidence:** the example's console, Windows STATIC and Linux (WSL, SOURCE
  build), 2026-10-16: `obstruct on` -> `Fault_Signals = { DOOR_CLOSING_FAULT }`
  with no error; `obstruct off` -> the three error lines, then `Fault_Signals
  cleared`. The source line is `BACnetChangeOfStateEventAlgorithm.cpp(225)` at
  the pin.

### 2. `SetPropertySubscribable` leaves `COV_Increment` absent

Filed: `chipkin/cas-bacnet-stack#4602` (ref for support).

Enabling COV on Analog Input 1 does not make the stack serve `COV_Increment`;
cl. 12.2 requires it for an Analog Input that supports COV reporting. The
example serves it itself (read-only, 1.0), so nothing is wrong on the wire; the
note is here so nobody removes that workaround.

## Known limitations of this example

- **A written `Recipient_List` is lost at restart.** Notification Class 1's
  `Recipient_List` is the one writable property of this otherwise read-only
  monitor; the list lives only in memory and returns to the standard default
  (local broadcast) when the example restarts (cl. 12.21.8). Whether it should
  be kept, or whether this monitor should stay write-free, is an open question.
  (ref: `chipkin/chipkin-bacnet-testing-tool#684`)
- **No writable REAL `Present_Value` exists**, so a test tool cannot drive a
  point to exercise COV and range scenarios. (ref: `chipkin/chipkin-bacnet-testing-tool#172`)
- **Operator-driven conformance rows stay manual** (AE-N-I-B, AE-ACK-B).
  (ref: `chipkin/chipkin-bacnet-testing-tool#680`)
