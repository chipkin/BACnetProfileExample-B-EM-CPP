# B-EM Wire Tests

Tests that drive the running elevator monitor over BACnet/IP from outside it.
They are written against the series' shared harness in
[`../wiretest/`](../wiretest/README.md) (vendored, like `common/`); everything
specific to this device is here. Python 3 standard library only - nothing to
`pip install`.

They prove the B-EM Profile Function - ANSI/ASHRAE 135-2024 L.13.3: "A B-EM
device performs monitoring of an elevator control system" - against a
Python-simulated operator workstation, plus its Failure Modes. Each check has
been shown to fail with its feature removed (see [Mutation results](#mutation-results)).

## Running

Build the example (README "Build"), then from the repository root:

```bash
python3 tests/run_wire_tests.py --exe build/BACnetExampleBEM
```

On Windows, `--exe build\Release\BACnetExampleBEM.exe` (or the
`build\static` tree). It ends with `ALL WIRE TESTS PASSED` (exit 0) or the
scripts that failed, in about a minute. `--port` (default 48030) is the UDP
port the test starts the device on; `--port + 1` is the Simulation's control
channel. CI (`.github/workflows/release.yml`) runs it on Linux after the smoke
test.

## The setup

```
operator workstation (127.0.0.1, ephemeral UDP port)   - the Peer: reads, COV, acks;
    |  BACnet/IP, unicast                                  Crimson's confirmed recipient
BACnetExampleBEM  UDP --port                           - Device 389015, Lift 1 "Mauve"
    ^
    |  127.0.0.1 UDP --port + 1, plain text            - test-only control channel
the test, playing the car's passengers                 (BEM_SIMULATION_CONTROL_PORT)
```

The Peers are Python BACnet clients (`wiretest.Client`, `wiretest.Workstation`),
never another example's binary. The control channel is not BACnet: it presses
buttons inside the simulated car (`call 5`, `alarm on`), blocks its doorway
(`obstruct on`), turns the simulated passengers off and speeds the car up
(500 ms per floor, 250 ms per door move, 600 ms dwell). A test that sets
`BEM_TEST_EVENT_RECIPIENT_PORT` becomes Crimson's second, confirmed recipient.

Every request goes through `em_setup.ask()`: an unanswered request is sent
again after 0.5 s with a visible `[RETRY]` line. On Windows, on a loaded
machine, loopback UDP was measured losing about one datagram in 700 before the
example saw it (699 sent, 698 received by the device); a short retry keeps the
car from moving on while the test waits. The ordered checks (floor by floor,
door cycle) come from COV notifications, which the OS queues for the
subscriber's socket, so a stalled test process misses none of them.

## `car_monitor_test.py` - the Profile Function

1. **Control**: no call - the car stands at floor 1, `STOPPED`, door `CLOSED`,
   and stays there; the COV subscription's first notification says so and no
   other follows; `Registered_Car_Call` is empty.
2. A passenger presses 5: **`Registered_Car_Call` lists 5** (cl. 12.59.13).
3. The trip: the car **arrives at 5, `STOPPED`**; it takes **at least four
   floor-travel times** (the car's speed); **every RPM snapshot of the moving
   car is `UP` with the door `SAFETY_LOCKED`** - one RPM is one consistent
   answer, so the car never moves with its door open.
4. At 5: **`Landing_Door_Status`** shows floor 5's landing door `OPENED` and
   the others `SAFETY_LOCKED` (cl. 12.59: the car door drives the landing door);
   the car call is cleared.
5. **COV** (DS-COVM-B, one SubscribeCOVPropertyMultiple on the car): Car_Position
   2, 3, 4, 5 one floor at a time; direction `UP` then `STOPPED`; door
   `SAFETY_LOCKED` -> `OPENING` -> `OPENED` -> `CLOSING` -> `CLOSED`; every
   notification for this subscriber from Device 389015.
6. Down to 2: the same checks, `DOWN`, and COV 4, 3, 2.

## `car_fault_test.py` - Failure Mode: a door that fails to close

1. **Control**: car at floor 3, door open, `Fault_Signals` empty, `Reliability`
   `NO_FAULT_DETECTED`, `Event_State` `NORMAL`, no notification.
2. The doorway is blocked and a passenger presses 7: the closing door re-opens.
3. **`Fault_Signals` = { `DOOR_CLOSING_FAULT` }**, `Reliability`
   `FAULTS_LISTED`, `Event_State` `FAULT`, `Status_Flags` FAULT.
4. The workstation receives a **ConfirmedEventNotification NORMAL -> FAULT,
   event type CHANGE_OF_RELIABILITY** (cl. 13.2.5.3), priority 50.
5. **GetEventInformation** lists Lift 1 in `FAULT`.
6. **A faulted car serves no stops**: 4 floor-travel times later it is still
   at 3 and the car call to 7 is still registered.
7. The doorway clears: the fault clears, a **FAULT -> NORMAL** notification
   arrives, and the car goes on to 7.

## `passenger_alarm_test.py` - alarm, acknowledgement, and a read-only monitor

1. **Control**: `Passenger_Alarm` FALSE, `NORMAL`, GetEventInformation empty.
2. The alarm button: `Passenger_Alarm` TRUE, `OFFNORMAL`, `IN_ALARM`, and a
   **ConfirmedEventNotification NORMAL -> OFFNORMAL, CHANGE_OF_STATE**,
   priority 100, ack required.
3. **GetEventInformation** lists Lift 1, to-offnormal unacknowledged.
4. **AcknowledgeAlarm** accepted; `Acked_Transitions` to-offnormal TRUE; an
   **ACK_NOTIFICATION** reaches the recipients (L.13.3: "Notifying other
   recipients that the acknowledgment has been received").
5. Released: `NORMAL`, and an OFFNORMAL -> NORMAL notification.
6. **Failure Modes**: AcknowledgeAlarm for Escalator 1 (no alarm) is refused;
   WriteProperty to `Car_Position`, `Passenger_Alarm` and `Out_Of_Service` is
   refused (a B-EM accepts no modifications); `Car_Position` is unchanged.

## `discovery_test.py` - every listed property reads as a value

The same walk as B-SS-CPP's discovery test; only the expected-object list
differs.

1. **Who-Is** (for 389015, unicast) -> the device's I-Am.
2. **Device `Object_List`** names exactly the ten objects `main.cpp` creates:
   the Device, Analog Input 1, Binary Input 1, Multi-state Input 1, Network
   Port 1, Elevator Group 1, Lift 1, Escalator 1, Positive Integer Value 1 and
   Notification Class 1. This is the control: a walk over nothing would pass.
3. For each object, **`Property_List`** reads, and a ReadProperty of every
   property it lists - plus `Object_Identifier`, `Object_Name`, `Object_Type`
   and `Property_List`, which cl. 12.1.1.4.1 leaves out of the list - returns a
   ComplexACK. An Error, Reject, Abort or no answer prints an `[UNREADABLE]` line
   and fails the check. 135-2024 cl. 12.1.1.4.1: Property_List holds "one
   property identifier for each property that exists within the object".
4. Network Port 1's `IP_DNS_Server` (406), `FD_BBMD_Address` (418) and
   `FD_Subscription_Lifetime` (419) are each listed and read as a value - the
   three reads Chipkin BACnet Explorer found failing on B-SS at stack 6.0.21
   (B-SS-CPP #20-#22); every example has the same stack Network Port.

## What it found

Run 2026-10-15 at stack `6.x` @ `64b2cfac5`, with `main.cpp` as it was at
example 1.0.2, Windows STATIC build: 15/16.

```
[UNREADABLE] Elevator Group 1 property 470: Error class=2 code=27
[UNREADABLE] Elevator Group 1 property 471: Error class=2 code=27
[FAIL] every property of every object reads as a value (146 reads) - 2 unreadable: Elevator Group 1 470, Elevator Group 1 471
```

Error class 2 code 27 is `property` / `read-access-denied`. Both were the
example's own defects (CHANGELOG 1.1.0):

- **`Landing_Calls` (470)**: the callback answered an empty list by returning
  `true` without supplying an element. The stack's contract
  (`FPCallbackGetListElevatorGroupLandingCallStatus`): "false if list is empty
  (and rangeIndexOrSequence is 0)". It now returns `false`.
- **`Landing_Call_Control` (471)** was present, enabled by
  `supportLandingCallStatus=true`, but this read-only monitor has nothing to
  answer for it, and cl. 12.58.10 says "If it is present, it shall be
  writable". It is no longer present; `Landing_Calls` is enabled on its own.

After the fix: 16/16, 145 reads, on Windows (STATIC) and Linux (WSL Ubuntu
22.04, SOURCE build), no `[RETRY]`.

## Mutation results

Each feature removed from `main.cpp` (or `simulation/ElevatorCar.cpp`), the
binary rebuilt (its mtime checked), the named script run: it must fail.
Windows STATIC, stack `6.x` @ `64b2cfac5`, 2026-10-16.

| Mutation | Script | Result | First failing check |
|---|---|---|---|
| M1 Car_Position constant | `car_monitor_test.py` | CAUGHT (16/21) | Up 1 -> 5: the car arrives at 5, STOPPED |
| M2 car never ticked | `car_monitor_test.py` | CAUGHT (9/21) | Up 1 -> 5: the car arrives at 5, STOPPED |
| M3 no UpdateValue for Car_Position (COV silent) | `car_monitor_test.py` | CAUGHT (19/21) | COV, up: Car_Position 2, 3, 4, 5 |
| M4 Car_Door_Status constant CLOSED | `car_monitor_test.py` | CAUGHT (17/21) | Up 1 -> 5: every RPM snapshot of the moving car is UP with the door SAFETY_LOCKED |
| M5 Car_Moving_Direction constant STOPPED | `car_monitor_test.py` | CAUGHT (17/21) | Up 1 -> 5: every RPM snapshot of the moving car is UP with the door SAFETY_LOCKED |
| M6 Registered_Car_Call always empty | `car_monitor_test.py` | CAUGHT (19/21) | Up 1 -> 5: a passenger presses 5 |
| M7 Landing_Door_Status ignores the car door | `car_monitor_test.py` | CAUGHT (20/21) | Doors open at 5: Landing_Door_Status shows floor 5 OPENED, the other floors SAFETY_LOCKED |
| M8 car ignores its speed (a floor per tick) | `car_monitor_test.py` | CAUGHT (19/21) | Up 1 -> 5: the trip takes at least 4 floor-travel times (2000 ms) |
| M9 COVM: car properties not subscribable | `car_monitor_test.py` | CAUGHT (14/21) | SubscribeCOVPropertyMultiple on the car (Car_Position, Car_Moving_Direction, Car_Door_Status) is accepted |
| M10 Fault_Signals always empty | `car_fault_test.py` | CAUGHT (8/13) | Fault_Signals = { DOOR_CLOSING_FAULT } |
| M11 no FAULT_LISTED algorithm | `car_fault_test.py` | CAUGHT (9/13) | Reliability FAULTS_LISTED, Event_State FAULT, Status_Flags FAULT set |
| M12 no UpdateValue for Fault_Signals | `car_fault_test.py` | CAUGHT (9/13) | Reliability FAULTS_LISTED, Event_State FAULT, Status_Flags FAULT set |
| M13 obstruction ignored by the door | `car_fault_test.py` | CAUGHT (5/13) | The closing door meets the obstruction and re-opens (CLOSING -> OPENING) |
| (first run) `!Faulted()` departure guard removed | `car_fault_test.py` | SURVIVED (13/13) | none - the guard was dead code: the only fault is a door that cannot close, and the car leaves a floor only with its door locked. Guard deleted; the interlock itself is M14 |
| M15 to-fault not enabled | `car_fault_test.py` | CAUGHT (12/13) | ConfirmedEventNotification from Lift 1: NORMAL -> FAULT, CHANGE_OF_RELIABILITY, priority 50 |
| M16 passenger alarm not reported (no UpdateValue) | `passenger_alarm_test.py` | CAUGHT (8/13) | Alarm pressed: Passenger_Alarm TRUE, Event_State OFFNORMAL, IN_ALARM set |
| M17 no CHANGE_OF_STATE algorithm | `passenger_alarm_test.py` | CAUGHT (8/13) | Alarm pressed: Passenger_Alarm TRUE, Event_State OFFNORMAL, IN_ALARM set |
| M18 AcknowledgeAlarm refused | `passenger_alarm_test.py` | CAUGHT (12/14) | AcknowledgeAlarm for Lift 1's to-offnormal is accepted |
| M14 door interlock removed (the car leaves without closing its door) | `car_fault_test.py` | CAUGHT (10/13) | The closing door meets the obstruction and re-opens (CLOSING -> OPENING) |
| M14b the same, against `car_monitor_test.py` | `car_monitor_test.py` | SURVIVED (21/21) | none - expected: that test never calls the car while its door is open; M14 is caught by `car_fault_test.py` |

M8 and M13 were re-run after the Simulation's last change (the dead guard removed, `ClearStops` added for B-EC); both still caught. `passenger_alarm_test.py`'s ACK_NOTIFICATION check was added after M18 ran; M18 (AcknowledgeAlarm refused) removes the acknowledgment it checks.
