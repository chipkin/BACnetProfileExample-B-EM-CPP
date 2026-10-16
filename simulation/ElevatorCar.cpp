// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
// =============================================================================
// ElevatorCar.cpp - SIMULATION. See ElevatorCar.h.
// =============================================================================

#include "ElevatorCar.h"

#include <algorithm>

namespace ElevatorSimulation {

Car::Car(uint8_t lowestFloor, uint8_t highestFloor, uint8_t startFloor)
    : m_lowestFloor(lowestFloor), m_highestFloor(highestFloor), m_position(startFloor) {}

bool Car::IsStopPending(uint8_t floor) const {
    return std::find(m_stops.begin(), m_stops.end(), floor) != m_stops.end();
}

void Car::RemoveStop(uint8_t floor) {
    m_stops.erase(std::remove(m_stops.begin(), m_stops.end(), floor), m_stops.end());
}

bool Car::RequestStop(uint8_t floor) {
    if (!Serves(floor)) {
        return false;
    }
    if (floor == m_position && m_direction == Direction::Stopped) {
        // Standing at that floor: answer it with the doors, not with a trip.
        if (m_door == Door::Opened) {
            m_phaseElapsed = 0;             // hold the doors open for another dwell
        } else if (m_door == Door::Closing || m_door == Door::Closed) {
            OpenDoors();
        }
        return true;
    }
    if (!IsStopPending(floor)) {
        m_stops.push_back(floor);
        std::sort(m_stops.begin(), m_stops.end());
    }
    return true;
}

bool Car::StopAhead(Direction direction) const {
    for (uint8_t stop : m_stops) {
        if ((direction == Direction::Up && stop > m_position) || (direction == Direction::Down && stop < m_position)) {
            return true;
        }
    }
    return false;
}

uint32_t Car::SetFault(bool active) {
    if (active && m_faults.empty()) {
        m_faults.push_back(Fault::DoorClosing);
        return CHANGE_FAULTS;
    }
    if (!active && !m_faults.empty()) {
        m_faults.clear();
        return CHANGE_FAULTS;
    }
    return CHANGE_NONE;
}

uint32_t Car::OpenDoors() {
    m_door = Door::Opening;
    m_phaseElapsed = 0;
    return CHANGE_DOOR;
}

uint32_t Car::Tick(uint32_t elapsedMilliseconds) {
    uint32_t changes = CHANGE_NONE;
    m_phaseElapsed += elapsedMilliseconds;

    switch (m_door) {
    case Door::Opening:
        if (m_phaseElapsed >= m_timing.doorMoveMilliseconds) {
            m_door = Door::Opened;
            m_phaseElapsed = 0;
            changes |= CHANGE_DOOR;
        }
        break;

    case Door::Opened:
        if (m_phaseElapsed >= m_timing.dwellMilliseconds) {
            m_door = Door::Closing;
            m_phaseElapsed = 0;
            changes |= CHANGE_DOOR;
        }
        break;

    case Door::Closing:
        if (m_obstructed && m_phaseElapsed >= m_timing.doorMoveMilliseconds / 2) {
            // The door's safety edge meets the obstruction half way: re-open.
            ++m_failedCloses;
            changes |= OpenDoors();
            if (m_failedCloses >= m_timing.doorCloseAttempts) {
                changes |= SetFault(true);
            }
        } else if (m_phaseElapsed >= m_timing.doorMoveMilliseconds) {
            m_door = Door::Closed;
            m_phaseElapsed = 0;
            m_failedCloses = 0;
            changes |= CHANGE_DOOR | SetFault(false); // the doors closed: the fault is gone
        }
        break;

    case Door::Closed:
        // Stopped at a floor with the doors shut. Leave for the next stop, if any.
        // This is the only way out of a floor: the car moves only with its door
        // locked (the door interlock), so a door that cannot close keeps it here.
        if (IsStopPending(m_position)) {
            RemoveStop(m_position);
            changes |= CHANGE_STOPS | OpenDoors();
        } else if (!m_stops.empty()) {
            if (!StopAhead(m_travel)) {
                m_travel = (m_travel == Direction::Up) ? Direction::Down : Direction::Up;
            }
            m_door = Door::Locked;      // lock the doors, then go
            m_direction = m_travel;
            m_phaseElapsed = 0;
            changes |= CHANGE_DOOR | CHANGE_DIRECTION;
        }
        break;

    case Door::Locked:
        if (m_direction == Direction::Stopped || m_phaseElapsed < m_timing.floorTravelMilliseconds) {
            break;
        }
        // One floor further.
        m_position = (uint8_t)(m_direction == Direction::Up ? m_position + 1 : m_position - 1);
        m_phaseElapsed = 0;
        changes |= CHANGE_POSITION;
        if (IsStopPending(m_position) || m_stops.empty() ||
            m_position == m_lowestFloor || m_position == m_highestFloor) {
            // Arrived: stop, unlock, open. The stop is served the moment the car
            // stops at the floor (cl. 12.59.32 CALL_BUTTON_STUCK is a call that
            // "continues to remain registered after the car has stopped at the floor").
            // With no stop left (ClearStops) or at a terminal floor it just stops.
            const bool served = IsStopPending(m_position);
            RemoveStop(m_position);
            m_direction = Direction::Stopped;
            changes |= CHANGE_DIRECTION;
            if (served) {
                changes |= CHANGE_STOPS | OpenDoors();
            } else {
                m_door = Door::Closed;  // no stop here: wait, doors shut
                changes |= CHANGE_DOOR;
            }
        }
        break;
    }
    return changes;
}

const char* DirectionName(Direction direction) {
    switch (direction) {
    case Direction::Up: return "up";
    case Direction::Down: return "down";
    default: return "stopped";
    }
}

const char* DoorName(Door door) {
    switch (door) {
    case Door::Locked: return "locked";
    case Door::Closed: return "closed";
    case Door::Opening: return "opening";
    case Door::Opened: return "opened";
    default: return "closing";
    }
}

} // namespace ElevatorSimulation
