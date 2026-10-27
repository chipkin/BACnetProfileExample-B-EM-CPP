#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-EM discovery: read every property the elevator monitor says it has, the way a client does.

    python3 tests/discovery_test.py [--exe build/BACnetExampleBEM] [--port 47832]

What a BACnet client such as Chipkin BACnet Explorer does when it discovers a device,
done over the wire from outside the example:

    1. Who-Is   -> the I-Am of Device 389015
    2. ReadProperty Device Object_List
    3. for every object: ReadProperty Property_List, then ReadProperty of every
       property it lists (and of Object_Identifier, Object_Name, Object_Type and
       Property_List itself, which cl. 12.1.1.4.1 leaves out of the list)

Every one of those reads must come back as a value - a ReadProperty ComplexACK. An
Error, Reject or Abort, or no answer, fails the check: Property_List holds "one property
identifier for each property that exists within the object" (135-2024 cl. 12.1.1.4.1),
so a listed property the device cannot read is a defect, whoever owns it.

The three reads named separately are the ones Explorer 4.23.2.0 found failing on
Network Port 1 at stack 6.0.21 (B-SS-CPP issues #20, #21, #22); every example in the
series has the same stack Network Port, so they are checked here too.

Exit code 0 = every check passed.
"""
import argparse
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY))

import wiretest  # noqa: E402  (the vendored harness at the repository root)
from wiretest import record, summary  # noqa: E402

# Match main.cpp.
DEVICE_INSTANCE = 389015
DEVICE = (wiretest.OBJECT_TYPE_DEVICE, DEVICE_INSTANCE)
NETWORK_PORT = (wiretest.OBJECT_TYPE_NETWORK_PORT, 1)            # Network Port 1 "Vermilion"
EXPECTED_OBJECTS = [DEVICE,
                    (wiretest.OBJECT_TYPE_ANALOG_INPUT, 1),          # "Bronze"
                    (wiretest.OBJECT_TYPE_BINARY_INPUT, 1),          # "Emerald"
                    (wiretest.OBJECT_TYPE_MULTI_STATE_INPUT, 1),     # "Hot Pink"
                    NETWORK_PORT,                                    # "Vermilion"
                    (57, 1),                                         # "Maroon" - Elevator Group object type: 57
                    (59, 1),                                         # "Mauve" - Lift object type: 59
                    (58, 1),                                         # "Mint" - Escalator object type: 58
                    (48, 1),                                         # "Turquoise" - Positive Integer Value object type: 48
                    (15, 1),                                         # "Crimson" - Notification Class object type: 15
                    ]

# Database_Revision (cl. 12.11): the stack starts it at 0 and adds 1 for each object the
# example adds at start-up - the Device and the nine objects above - and nothing creates,
# deletes or renames an object afterwards, so it reads 10 for the life of the process
# (docs/PICS.md).
PROPERTY_DATABASE_REVISION = 155
EXPECTED_DATABASE_REVISION = len(EXPECTED_OBJECTS)

# Analog Input 1's COV_Increment (cl. 12.2.16), as main.cpp serves it.
PROPERTY_COV_INCREMENT = 22
EXPECTED_COV_INCREMENT = 1.0

# The four properties cl. 12.1.1.4.1 says Property_List does not contain.
PROPERTY_OBJECT_TYPE = 79
NOT_IN_PROPERTY_LIST = [wiretest.PROPERTY_OBJECT_IDENTIFIER, wiretest.PROPERTY_OBJECT_NAME,
                        PROPERTY_OBJECT_TYPE, wiretest.PROPERTY_PROPERTY_LIST]

# Network Port properties Explorer could not read at stack 6.0.21 (cl. 12.56; numbers from
# cl. 21 BACnetPropertyIdentifier), with the issue that reported each.
PROPERTY_IP_DNS_SERVER = 406
PROPERTY_FD_BBMD_ADDRESS = 418
PROPERTY_FD_SUBSCRIPTION_LIFETIME = 419
EXPLORER_FAILURES = [(PROPERTY_IP_DNS_SERVER, "IP_DNS_Server", "B-SS-CPP #20"),
                     (PROPERTY_FD_BBMD_ADDRESS, "FD_BBMD_Address", "B-SS-CPP #21"),
                     (PROPERTY_FD_SUBSCRIPTION_LIFETIME, "FD_Subscription_Lifetime", "B-SS-CPP #22")]

OBJECT_TYPE_NAMES = {wiretest.OBJECT_TYPE_DEVICE: "Device", wiretest.OBJECT_TYPE_NETWORK_PORT: "Network Port",
                     57: "Elevator Group",
                     59: "Lift",
                     58: "Escalator",
                     48: "Positive Integer Value",
                     15: "Notification Class",
                     wiretest.OBJECT_TYPE_ANALOG_INPUT: "Analog Input",
                     wiretest.OBJECT_TYPE_BINARY_INPUT: "Binary Input",
                     wiretest.OBJECT_TYPE_MULTI_STATE_INPUT: "Multi-state Input",
                     }
PDU_NAMES = {wiretest.PDU_ERROR: "Error", wiretest.PDU_REJECT: "Reject", wiretest.PDU_ABORT: "Abort"}


def parser():
    arguments = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--exe", default=str(wiretest.default_exe(REPOSITORY, "BACnetExampleBEM")),
                           help="path to BACnetExampleBEM (default: build/BACnetExampleBEM)")
    arguments.add_argument("--port", type=int, default=47832,
                           help="the elevator monitor's UDP port (default 47832)")
    return arguments


def object_text(object_identifier):
    object_type, instance = object_identifier
    return f"{OBJECT_TYPE_NAMES.get(object_type, f'object type {object_type}')} {instance}"


def outcome(answer):
    """What a ReadProperty got back, in words: the value, or why there is none."""
    if not answer.answered:
        return "no answer"
    if answer.frame.pdu_type == wiretest.PDU_COMPLEX_ACK:
        return f"value {answer.value!r}"
    if answer.error is not None:
        return f"Error class={answer.error[0]} code={answer.error[1]}"
    if answer.frame.pdu_type in (wiretest.PDU_REJECT, wiretest.PDU_ABORT) and len(answer.frame.payload) > 2:
        return f"{PDU_NAMES[answer.frame.pdu_type]} reason={answer.frame.payload[2]}"
    return answer.describe()


def is_value(answer):
    return answer.answered and answer.frame.pdu_type == wiretest.PDU_COMPLEX_ACK


def who_is(client, port, attempts=2):
    """Who-Is for this one device, unicast to it (a client that already knows the address
    does this; Explorer broadcasts). Returns the I-Am frame that comes back, or None.
    Who-Is is unconfirmed, so nothing else retries it: if no I-Am comes back it is sent
    once more, with a visible [RETRY] line (the example's UDP receive can drop a datagram
    on Windows - common/ SimpleUDP)."""
    for attempt in range(attempts):
        if attempt:
            print("[RETRY] Who-Is got no I-Am in 3.0 s - sent again", flush=True)
        client.frames.clear()
        client.send(wiretest.bvlc(wiretest.npdu(wiretest.who_is(DEVICE_INSTANCE, DEVICE_INSTANCE))), port)
        frame = client.wait_for(lambda candidate: wiretest.i_am_device_instance(candidate.payload) == DEVICE_INSTANCE,
                                3.0)
        if frame is not None:
            return frame
    return None


def main():
    args = parser().parse_args()
    device = wiretest.start(args.exe, ["--port", args.port], "elevator monitor")
    client = wiretest.Client(retries=1)
    try:
        i_am = who_is(client, args.port)
        record(f"Who-Is: Device {DEVICE_INSTANCE} answers with an I-Am", i_am is not None,
               "no I-Am" if i_am is None else "")

        object_list_answer = client.read_property(args.port, *DEVICE, wiretest.PROPERTY_OBJECT_LIST, timeout=2.0)
        object_list = wiretest.decoded_values(object_list_answer.value_octets) if is_value(object_list_answer) else []
        # Control: the walk below proves nothing if it walks nothing.
        record(f"Device Object_List names exactly the {len(EXPECTED_OBJECTS)} objects main.cpp creates",
               sorted(object_list) == sorted(EXPECTED_OBJECTS),
               f"{len(object_list)} objects" if is_value(object_list_answer) else outcome(object_list_answer))

        reads = 0
        unreadable = {}
        property_lists = {}
        for object_identifier in object_list:
            list_answer = client.read_property(args.port, *object_identifier, wiretest.PROPERTY_PROPERTY_LIST,
                                               timeout=2.0)
            property_list = wiretest.decoded_values(list_answer.value_octets) if is_value(list_answer) else []
            property_lists[object_identifier] = property_list
            record(f"{object_text(object_identifier)}: Property_List reads", is_value(list_answer) and property_list,
                   outcome(list_answer) if not is_value(list_answer) else f"{len(property_list)} properties")
            for property_identifier in NOT_IN_PROPERTY_LIST + property_list:
                answer = client.read_property(args.port, *object_identifier, property_identifier, timeout=2.0)
                reads += 1
                if not is_value(answer):
                    unreadable[(object_identifier, property_identifier)] = outcome(answer)
                    print(f"[UNREADABLE] {object_text(object_identifier)} property {property_identifier}: "
                          f"{outcome(answer)}", flush=True)

        record(f"every property of every object reads as a value ({reads} reads)", reads and not unreadable,
               f"{len(unreadable)} unreadable: " + ", ".join(
                   f"{object_text(object_identifier)} {property_identifier}"
                   for object_identifier, property_identifier in unreadable) if unreadable else "")

        # The three Explorer reported, by name - listed in Property_List and readable.
        network_port_list = property_lists.get(NETWORK_PORT, [])
        for property_identifier, name, issue in EXPLORER_FAILURES:
            listed = property_identifier in network_port_list
            failure = unreadable.get((NETWORK_PORT, property_identifier))
            record(f"Network Port 1 {name} ({property_identifier}): listed in Property_List and reads as a value "
                   f"(issue {issue})", listed and failure is None,
                   "not in Property_List" if not listed else (failure or ""))

        # Analog Input 1 supports COV reporting (DS-COV-B), so cl. 12.2 footnote 2 requires
        # its COV_Increment: listed in Property_List, and reads main.cpp's 1.0.
        analog_input = (wiretest.OBJECT_TYPE_ANALOG_INPUT, 1)
        cov_increment = client.read_property(args.port, *analog_input, PROPERTY_COV_INCREMENT, timeout=2.0)
        record(f"Analog Input 1 COV_Increment ({PROPERTY_COV_INCREMENT}): listed in Property_List and reads "
               f"{EXPECTED_COV_INCREMENT} (cl. 12.2 footnote 2: required where COV reporting is supported)",
               PROPERTY_COV_INCREMENT in property_lists.get(analog_input, []) and is_value(cov_increment)
               and cov_increment.value == EXPECTED_COV_INCREMENT, outcome(cov_increment))

        database_revision = client.read_property(args.port, *DEVICE, PROPERTY_DATABASE_REVISION, timeout=2.0)
        record(f"Device Database_Revision reads {EXPECTED_DATABASE_REVISION}, one per object added at start-up "
               "(docs/PICS.md)", is_value(database_revision) and database_revision.value == EXPECTED_DATABASE_REVISION,
               outcome(database_revision))
    finally:
        client.close()
        device.stop()
    return summary()


if __name__ == "__main__":
    sys.exit(main())
