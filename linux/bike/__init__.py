# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2023-2026 Tao Jin
import math
from dataclasses import dataclass


@dataclass(slots=True)
class BikeState:
    """Unified telemetry state produced by a Bike source.

    A source populates whichever fields it natively measures; the protocol
    encoder's RateEventBridge fills in whichever half is missing (rate from
    events, or events from rate). Sources never need to pre-integrate or
    differentiate — just stamp what they know.

    Rotational channels (crank, wheel) each expose two views:
      - rate: rpm for crank, m/s for wheel (NaN = unknown)
      - event: cumulative revolution count + event timestamp in **ticks**
        (1/1024 s each; matches BLE CSC/CP and ANT+ wire format)
    `has_*_event` signals the source is the authoritative owner of the event
    fields; the bridge then derives rate from Δcount / Δtick.

    Scalar fields (power, hr, resistance_pct, incline_pct, distance_m,
    elapsed_s) use NaN to mark unknown. Encoders gate per-characteristic
    emission on `!isnan()` — a source that doesn't measure HR just leaves
    `hr` at NaN and HRS will skip it.

    `last_update_tick` is stamped by the source on every fresh sample;
    consumers compare it against get_tick_now() to detect silence. A value
    of 0 means the source has never produced data.

    Keep this a plain data record — no methods, no asyncio — so it ports
    to a C struct on MCU unchanged (see DEVELOPMENT.md §2 BikeData).
    """

    # Rotational rates
    cadence: float = math.nan  # rpm
    speed: float = math.nan    # m/s

    # Rotational events (hardware-originated or bridge-synthesized)
    has_crank_event: bool = False
    crank_revs: int = 0
    crank_event_tick: int = 0

    has_wheel_event: bool = False
    wheel_revs: int = 0
    wheel_event_tick: int = 0

    # Scalars — NaN marks unknown, encoders gate emission per field
    power: float = math.nan          # watts
    hr: float = math.nan             # bpm
    resistance_pct: float = math.nan # 0..100
    incline_pct: float = math.nan    # -100..100
    distance_m: float = math.nan     # meters, cumulative
    elapsed_s: float = math.nan      # seconds, session duration

    last_update_tick: int = 0


class Bike:
    """Source base class: owns a BikeState, nothing else.

    Silence detection is the consumer's job: it compares
    `state.last_update_tick` against the current time. Sources need not
    track "am I silent?" themselves — they just stop stamping
    `last_update_tick` when no fresh data arrives.

    Subclasses may hold their own private sync primitives (e.g. an
    asyncio.Event for driver-internal plumbing), but those are not part of
    the base-class contract.
    """

    def __init__(self) -> None:
        self.state = BikeState()
