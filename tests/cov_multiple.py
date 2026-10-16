# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""SubscribeCOVPropertyMultiple (DS-COVM) from the subscriber's side.

GENERIC - proposed for wiretest/ 0.7.0, which has no COV support yet.

    request      subscribe_cov_property_multiple_request(invoke_id, process_id, confirmed,
                 lifetime, specifications)
    decoding     CovNotificationMultiple (Confirmed- or UnconfirmedCOVNotificationMultiple)

Productions verbatim from ANSI/ASHRAE 135-2024 clause 21:
  SubscribeCOVPropertyMultiple-Request ::= SEQUENCE { subscriber-process-identifier [0]
  Unsigned32, issue-confirmed-notifications [1] Boolean, lifetime [2] Unsigned OPTIONAL,
  max-notification-delay [3] Unsigned OPTIONAL, list-of-cov-subscription-specifications [4]
  SEQUENCE OF SEQUENCE { monitored-object-identifier [0] BACnetObjectIdentifier,
  list-of-cov-references [1] SEQUENCE OF SEQUENCE { monitored-property [0]
  BACnetPropertyReference, cov-increment [1] Real OPTIONAL, timestamped [2] Boolean } } }
  (cl. 21.2; service choice subscribe-cov-property-multiple (30)).
  Confirmed/UnconfirmedCOVNotificationMultiple-Request ::= SEQUENCE {
  subscriber-process-identifier [0], initiating-device-identifier [1], time-remaining [2],
  timestamp [3] BACnetDateTime OPTIONAL, list-of-cov-notifications [4] SEQUENCE OF SEQUENCE {
  monitored-object-identifier [0], list-of-values [1] SEQUENCE OF SEQUENCE {
  property-identifier [0], property-array-index [1] OPTIONAL, property-value [2],
  time-of-change [3] Time OPTIONAL } } } (cl. 21.2 choice 31 / cl. 21.3 choice 11).

Notifications go to the address the subscription came from, so subscribe with the socket
that will collect them: a wiretest.Client, then client.collect() / client.wait_for() - and
make no further requests on it (Client.request clears the frames it has kept).
"""
import struct

from wiretest import codec

SERVICE_SUBSCRIBE_COV_PROPERTY_MULTIPLE = 30
SERVICE_CONFIRMED_COV_NOTIFICATION_MULTIPLE = 31
SERVICE_UNCONFIRMED_COV_NOTIFICATION_MULTIPLE = 11


def _context_boolean(tag, value):
    return bytes([(tag << 4) | 0x08 | 1, 1 if value else 0])


def subscribe_cov_property_multiple_request(invoke_id, process_identifier, confirmed, lifetime, specifications,
                                            max_notification_delay=0):
    """`specifications`: [(object_type, instance, [property identifier, ...])], each property
    subscribed whole (no array index, no COV increment, not timestamped). cl. 13.16.1.1.3/.4:
    Lifetime and Max Notification Delay "shall be present if the request is a subscription, or
    re-subscription, and absent if the request is a cancellation" - pass lifetime=None for a
    cancellation and both are left out."""
    parameters = codec.context_unsigned(0, process_identifier) + _context_boolean(1, confirmed)
    if lifetime is not None:
        parameters += codec.context_unsigned(2, lifetime) + codec.context_unsigned(3, max_notification_delay)
    parameters += codec.opening(4)
    for object_type, instance, properties in specifications:
        parameters += codec.context_object_id(0, object_type, instance) + codec.opening(1)
        for property_identifier in properties:
            parameters += (codec.opening(0) + codec.context_unsigned(0, property_identifier) + codec.closing(0) +
                           _context_boolean(2, False))
        parameters += codec.closing(1)
    parameters += codec.closing(4)
    return codec.confirmed_request(invoke_id, SERVICE_SUBSCRIBE_COV_PROPERTY_MULTIPLE, parameters)


def is_cov_notification_multiple(frame):
    return ((frame.pdu_type == codec.PDU_UNCONFIRMED_REQUEST and frame.service == SERVICE_UNCONFIRMED_COV_NOTIFICATION_MULTIPLE)
            or (frame.pdu_type == codec.PDU_CONFIRMED_REQUEST and frame.service == SERVICE_CONFIRMED_COV_NOTIFICATION_MULTIPLE))


class CovNotificationMultiple:
    """One COVNotificationMultiple, decoded. `values` is [(object, property, array index, value octets)]."""

    def __init__(self, frame):
        self.frame = frame
        self.confirmed = frame.pdu_type == codec.PDU_CONFIRMED_REQUEST
        self.process_identifier = None
        self.initiating_device = None
        self.time_remaining = None
        self.values = []
        octets = frame.payload[4:] if self.confirmed else frame.payload[2:]
        position = 0
        depth = 0
        monitored = None
        property_identifier = None
        array_index = None
        while position < len(octets):
            number, is_context, kind, body, position = codec.read_tag(octets, position)
            if kind == "open" and depth == 2 and number == 2:   # property-value [2]
                start = position
                inner = 1
                while inner:
                    tag_start = position
                    _, _, inner_kind, _, position = codec.read_tag(octets, position)
                    inner += 1 if inner_kind == "open" else -1 if inner_kind == "close" else 0
                self.values.append((monitored, property_identifier, array_index, octets[start:tag_start]))
                array_index = None
                continue
            if kind == "open":
                depth += 1
                continue
            if kind == "close":
                depth -= 1
                continue
            if not is_context:
                continue
            if depth == 0 and number == 0:
                self.process_identifier = int.from_bytes(body, "big")
            elif depth == 0 and number == 1:
                raw = struct.unpack(">I", body)[0]
                self.initiating_device = (raw >> 22, raw & codec.MAX_INSTANCE)
            elif depth == 0 and number == 2:
                self.time_remaining = int.from_bytes(body, "big")
            elif depth == 1 and number == 0:
                raw = struct.unpack(">I", body)[0]
                monitored = (raw >> 22, raw & codec.MAX_INSTANCE)
            elif depth == 2 and number == 0:
                property_identifier = int.from_bytes(body, "big")
            elif depth == 2 and number == 1:
                array_index = int.from_bytes(body, "big")

    def value(self, object_identifier, property_identifier):
        """The decoded value of `property_identifier` in this notification, or None."""
        for monitored, identifier, _, octets in self.values:
            if monitored == object_identifier and identifier == property_identifier:
                return codec.decode_application(octets)
        return None
