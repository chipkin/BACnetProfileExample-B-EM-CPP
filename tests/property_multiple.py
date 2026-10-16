# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""ReadPropertyMultiple from the client side: build the request, decode the ACK.

GENERIC - proposed for wiretest/ 0.7.0 (0.6.0 has only the server side: a SimulatedServer
answers RPM, and codec.read_access_specification builds one ReadAccessSpecification).

One RPM is ONE answer from the device, so the values in it are a consistent snapshot - what
a test needs to check an invariant between two properties ("the car never moves with its door
open") without a change slipping in between two separate ReadProperty requests.

ANSI/ASHRAE 135-2024 cl. 21.2: ReadPropertyMultiple-Request ::= SEQUENCE {
list-of-read-access-specs SEQUENCE OF ReadAccessSpecification }; ReadPropertyMultiple-ACK ::=
SEQUENCE { list-of-read-access-results SEQUENCE OF ReadAccessResult }. ReadAccessResult
(cl. 21.6): object-identifier [0], list-of-results [1] SEQUENCE OF SEQUENCE {
property-identifier [2], property-array-index [3] OPTIONAL, read-result CHOICE {
property-value [4], property-access-error [5] } }.
"""
import struct

import wiretest
from wiretest import codec


def read_property_multiple_request(invoke_id, specifications):
    """`specifications`: [(object_type, instance, [property identifiers])]."""
    parameters = b"".join(codec.read_access_specification(object_type, instance, properties)
                          for object_type, instance, properties in specifications)
    return codec.confirmed_request(invoke_id, codec.SERVICE_READ_PROPERTY_MULTIPLE, parameters)


def _constructed_body(octets, position):
    """`position` is just after an opening tag: (the octets inside, position after its closing tag)."""
    depth = 1
    start = position
    while True:
        tag_start = position
        _, _, kind, _, position = codec.read_tag(octets, position)
        if kind == "open":
            depth += 1
        elif kind == "close":
            depth -= 1
            if depth == 0:
                return octets[start:tag_start], position


def parse_read_property_multiple_ack(apdu):
    """{(object_type, instance): {property_identifier: value octets, or ('error', class, code)}}
    from a ReadPropertyMultiple ComplexACK APDU; None if it is not one."""
    if len(apdu) < 3 or apdu[0] >> 4 != codec.PDU_COMPLEX_ACK or apdu[2] != codec.SERVICE_READ_PROPERTY_MULTIPLE:
        return None
    results = {}
    current = None
    property_identifier = None
    position = 3
    while position < len(apdu):
        number, is_context, kind, body, position = codec.read_tag(apdu, position)
        if kind == "value" and is_context and number == 0:
            raw = struct.unpack(">I", body)[0]
            current = results.setdefault((raw >> 22, raw & codec.MAX_INSTANCE), {})
        elif kind == "value" and is_context and number == 2:
            property_identifier = int.from_bytes(body, "big")
        elif kind == "open" and number == 4:
            value, position = _constructed_body(apdu, position)
            current[property_identifier] = value
        elif kind == "open" and number == 5:
            error, position = _constructed_body(apdu, position)
            values = codec.application_values(error)
            current[property_identifier] = ("error",) + tuple(int.from_bytes(body, "big") for _, body in values[:2])
    return results


def read_multiple(client, port, specifications, timeout=2.0):
    """Send one RPM with `client` (a wiretest.Client or Workstation); the decoded results, or None."""
    answer = client.request(lambda invoke_id: read_property_multiple_request(invoke_id, specifications),
                            port, timeout=timeout)
    if not answer.answered:
        return None
    return parse_read_property_multiple_ack(answer.frame.payload)


def value_of(octets):
    """One decoded value from a result's octets (an error tuple or None passes through)."""
    if octets is None or isinstance(octets, tuple):
        return octets
    return wiretest.decode_application(octets)
