# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""The Wire Tests' hands on an example's Simulation: a client for its test-only control channel.

GENERIC - proposed for wiretest/ 0.7.0. Nothing here knows about elevators. An example that
has a Simulation (a door contact, a foreign bus, a lift car) may open a loopback-only UDP
socket when an environment variable names a port; each datagram is one plain-text command
and the example answers each with one datagram: "ok", a value, or "error <reason>". B-GW-CPP
has the same channel (BGW_SIMULATION_CONTROL_PORT); this is the client half, written once.

    control = SimulationControl(port)
    control.send("obstruct on")           # raises if the answer is not "ok"/a value
    control.ask("state")                  # returns the answer text, whatever it is
"""
import socket


class SimulationControl:
    def __init__(self, port, ip="127.0.0.1", timeout=2.0):
        self.address = (ip, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(timeout)

    def ask(self, command):
        """Send one command; the example's answer, or 'no answer'."""
        self.sock.sendto(command.encode("ascii"), self.address)
        try:
            data, _ = self.sock.recvfrom(512)
        except (socket.timeout, ConnectionResetError):
            return "no answer"
        return data.decode("ascii", errors="replace")

    def send(self, command):
        """Send one command that must succeed; returns the answer, raises RuntimeError otherwise."""
        reply = self.ask(command)
        if reply == "no answer" or reply.startswith("error"):
            raise RuntimeError(f"simulation control '{command}': {reply}")
        return reply

    def close(self):
        self.sock.close()
