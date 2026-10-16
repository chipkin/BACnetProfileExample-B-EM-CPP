# TODO

Defects this example cannot fix itself. A defect that stops a Wire Test check is
reported as a `[KNOWN GAP]` line ([`tests/`](tests/README.md)): reported on every
run, not counted as a failure. **There are none at Stack Pin `6.x` @ `64b2cfac5`**
- every check of the Profile Function and its Failure Modes passes.

## Observed in the stack, not affecting a check (not filed)

### 1. `UpdateValue(Fault_Signals)` logs a CHANGE_OF_STATE error

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
