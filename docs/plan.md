# Plan (STUB): B-EM (Elevator Monitor) — C++ example

> **STATUS: STUB.** Seed facts below. Expand from
> [`bacnet-profile-plan-template.md`](../../bacnet-profile-plan-template.md) after the
> sample plans ([B-LD](../../BACnetProfileExample-B-LD-CPP/docs/plan.md),
> [B-BC](../../BACnetProfileExample-B-BC-CPP/docs/plan.md)) are reviewed.

**Profile:** B-EM · **Family:** Annex L.13 (Elevator Controller) · **Role:** B ·
**Archetype:** Controller · **Difficulty:** 4/5 · **Build wave:** 3

**Thesis:** an elevator **monitor** — a **read-only** view of lifts/escalators with
COV + alarms (no writes). Canonical source for **F-COVM, F-ELEVATOR**.

## Required BIBBs (profiles.md L.13)
`DS-RP-B, DS-RPM-B, DS-COV-B, DS-COVM-B; AE-N-I-B, AE-ACK-B, AE-INFO-B; DM-DDB-B,
DM-DOB-B, DM-DCC-B`. **No DS-WP-B** — read-only.

## Services to enable
- RP (1), RPM (14), SubscribeCOV (5), SubscribeCOVMultiple, DCC (17),
  GetEventInformation (39), AcknowledgeAlarm. **No WriteProperty.**

## Objects (baseline + )
- Lift 1, Escalator 1, Elevator Group 1 (`Car_Position`, `Car_Door_Status`,
  `Car_Moving_Direction`, ... — confirm in DLL), Notification Class 1.

## Shared features
- **DEFINE:** F-COVM (SubscribeCOVMultiple), F-ELEVATOR (Lift/Escalator/Group).
- **REUSE:** F-ALARM (B-AAC), F-COV (B-ACCR), F-DCC (B-ASC).

## Known stack gaps
- Recipient-by-address (F-ALARM; master plan §7 risk 2). profiles.md: ✅ S68
  (lift + elevatorGroup modeled).

## Notes / open questions
- Read-only is the distinguishing trait — **do not** enable WriteProperty (a
  faithful B-EM rejects writes). Confirm the elevator object property set.
