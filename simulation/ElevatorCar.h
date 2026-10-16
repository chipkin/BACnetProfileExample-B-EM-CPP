// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef ELEVATOR_CAR_H
#define ELEVATOR_CAR_H

// =============================================================================
// ElevatorCar - SIMULATION. One lift car in its shaft, standing in for the real
// elevator controller a product would read over its own bus. Nothing in this
// file knows about BACnet: main.cpp reads the car's state and serves it as the
// Lift object's properties (135-2024 cl. 12.59).
//
// The car:
//   - serves a range of universal floor numbers (lowest..highest), one floor at
//     a time at a set speed (Timing::floorTravelMilliseconds per floor);
//   - keeps a set of pending stops. It runs them collectively: it keeps going in
//     its direction of travel while there is a stop ahead, then reverses;
//   - at a stop: doors OPENING -> OPENED (dwell) -> CLOSING -> CLOSED, then the
//     doors lock before the car moves again. It never moves with a door open;
//   - a door obstruction (something in the doorway) makes a closing door re-open.
//     After Timing::doorCloseAttempts failed closes the car raises
//     DoorClosingFault and stays where it is, doors open, until the doorway is
//     clear and the doors close - which clears the fault. The car only ever
//     leaves a floor with its door closed and locked (the door interlock), so a
//     faulted car serves no stops: they stay pending.
//
// The same file is used unchanged by B-EM (a monitor: simulated passengers press
// car buttons) and B-EC (a controller: BACnet landing calls add stops).
// =============================================================================

#include <stdint.h>
#include <vector>

namespace ElevatorSimulation {

enum class Direction { Stopped, Up, Down };

// Door states, in the order a stop goes through them. Locked = fully closed and
// locked (the car may move); Closed = fully closed, not locked (stopped at a floor).
enum class Door { Locked, Closed, Opening, Opened, Closing };

// The faults this simulation can raise. main.cpp maps each to its BACnetLiftFault.
enum class Fault { DoorClosing };

struct Timing {
    uint32_t floorTravelMilliseconds = 2000; // the car's speed: one floor per this
    uint32_t doorMoveMilliseconds = 1000;    // fully closed <-> fully open
    uint32_t dwellMilliseconds = 2000;       // doors held open at a stop
    uint32_t doorCloseAttempts = 3;          // obstructed closes before DoorClosingFault
};

// What changed during one Tick() - main.cpp calls BACnetStack_UpdateValue for each.
enum Change : uint32_t {
    CHANGE_NONE = 0,
    CHANGE_POSITION = 1u << 0,
    CHANGE_DIRECTION = 1u << 1,
    CHANGE_DOOR = 1u << 2,
    CHANGE_STOPS = 1u << 3,   // a stop was added or served
    CHANGE_FAULTS = 1u << 4
};

class Car {
public:
    Car(uint8_t lowestFloor, uint8_t highestFloor, uint8_t startFloor);

    // A stop at `floor` (a passenger's car call, or a landing call the group
    // controller assigned to this car). False if the car does not serve that floor.
    // A stop at the floor the car is standing at re-opens (or holds open) its doors.
    bool RequestStop(uint8_t floor);

    bool Serves(uint8_t floor) const { return floor >= m_lowestFloor && floor <= m_highestFloor; }
    bool IsStopPending(uint8_t floor) const;

    // Forget every pending stop (a controller restart loses its calls). The car
    // finishes the floor it is travelling to and stops there.
    void ClearStops() { m_stops.clear(); }

    void SetObstructed(bool obstructed) { m_obstructed = obstructed; }
    void SetTiming(const Timing& timing) { m_timing = timing; }

    // Advance the simulation by elapsedMilliseconds; returns the Change bits.
    uint32_t Tick(uint32_t elapsedMilliseconds);

    uint8_t Position() const { return m_position; }
    Direction MovingDirection() const { return m_direction; }
    Door DoorState() const { return m_door; }
    bool Obstructed() const { return m_obstructed; }
    const std::vector<uint8_t>& PendingStops() const { return m_stops; } // ascending
    const std::vector<Fault>& Faults() const { return m_faults; }
    bool Faulted() const { return !m_faults.empty(); }
    uint8_t LowestFloor() const { return m_lowestFloor; }
    uint8_t HighestFloor() const { return m_highestFloor; }
    const Timing& CurrentTiming() const { return m_timing; }

private:
    void RemoveStop(uint8_t floor);
    bool StopAhead(Direction direction) const;
    uint32_t SetFault(bool active);
    uint32_t OpenDoors();

    uint8_t m_lowestFloor;
    uint8_t m_highestFloor;
    uint8_t m_position;
    Direction m_direction = Direction::Stopped;
    Direction m_travel = Direction::Up; // the direction of the current run (kept while stopped at a floor)
    Door m_door = Door::Closed;
    bool m_obstructed = false;
    uint32_t m_failedCloses = 0;
    uint32_t m_phaseElapsed = 0;        // milliseconds spent in the current door phase / floor-to-floor run
    Timing m_timing;
    std::vector<uint8_t> m_stops;
    std::vector<Fault> m_faults;
};

const char* DirectionName(Direction direction);
const char* DoorName(Door door);

} // namespace ElevatorSimulation

#endif // ELEVATOR_CAR_H
