# TODO

Defects this example cannot fix itself. A defect that stops a Wire Test check is
reported as a `[KNOWN GAP]` line ([`tests/`](tests/README.md)): reported on every
run, not counted as a failure. **There are none at Stack Pin `6.x` @ `1e7dffe2b`**
- every check of the Profile Function and its Failure Modes passes (Wire Tests on
Windows, 2026-11-01).

## Fixed in the stack at the pin

Both were observed here at earlier stack revisions; the fix for each is in the
pinned revision (`1e7dffe2b`). Kept as a reference for support.

- **After a car door fault cleared, a held Passenger_Alarm was not re-judged**
  ([#3967](https://github.com/chipkin/cas-bacnet-stack/issues/3967), closed). The
  `UpdateValue(..., Lift, 1, Fault_Signals)` that cleared a fault ran
  CHANGE_OF_STATE on the empty `Fault_Signals` list instead of `Passenger_Alarm`
  (debug log: `monitoredValue has invalid datatype=[255]`).
- **`SetPropertySubscribable` left `COV_Increment` absent**
  (`chipkin/cas-bacnet-stack#4602`, closed). The stack now enables it, reading
  1.0, when `Present_Value` is made subscribable. This example still enables and
  serves Analog Input 1's `COV_Increment` itself (read-only, 1.0), as cl. 12.2
  requires for an Analog Input that supports COV reporting.

## Known limitations of this example

- **A written `Recipient_List` is lost at restart.** Notification Class 1's
  `Recipient_List` is the one writable property of this otherwise read-only
  monitor; the list lives only in memory and returns to the standard default
  (local broadcast) when the example restarts (cl. 12.21.8). Whether it should
  be kept, or whether this monitor should stay write-free, is an open question.
- **No writable REAL `Present_Value` exists**, so a test tool cannot drive a
  point to exercise COV and range scenarios.
- **Operator-driven conformance tests stay manual** (AE-N-I-B, AE-ACK-B).
