// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef SIMULATION_CONTROL_H
#define SIMULATION_CONTROL_H

// =============================================================================
// SIMULATION ONLY, TEST ONLY - the back door the Wire Tests (tests/) use to
// drive the elevator Simulation from outside the process: press the car's
// passenger-alarm button, block a doorway, speed the car up so a test does not
// take minutes. Nothing here is BACnet: it plays the physical world.
//
// It is OFF unless the environment variable named in Start() holds a UDP port.
// Then it listens on 127.0.0.1 (loopback only) on that port for one plain-text
// command per datagram and answers each with one datagram, produced by the
// handler main.cpp passes to Service() (main.cpp lists the commands).
//
// It is deliberately NOT a command-line option: nothing here changes what the
// device does on the BACnet network, and a customer reading --help should never
// meet it. A real product has no such thing - delete this file together with
// simulation/ElevatorCar.cpp. Socket code adapted from B-GW-CPP's
// simulation/SimulationControl.cpp.
// =============================================================================

#include <stddef.h>

namespace SimulationControl {

// Runs one command; writes the answer (always non-empty) into reply.
typedef void (*CommandHandler)(const char* command, char* reply, size_t replyLength);

// Opens the control socket if the environment variable `portVariable` is set.
// Call after common/'s SetupUDP (which has already initialised sockets on
// Windows). Returns false only if the variable is set and the socket could not
// be opened.
bool Start(const char* portVariable);

// Answer any command waiting on the socket. Non-blocking; call every loop.
void Service(CommandHandler handler);

void Stop();

} // namespace SimulationControl

#endif // SIMULATION_CONTROL_H
