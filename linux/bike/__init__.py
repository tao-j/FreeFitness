import math
from dataclasses import dataclass


@dataclass(slots=True)
class BikeState:
    """Unified telemetry state produced by a Bike source.

    Rates are the continuous, instantaneous view of the bike. Events are the
    discrete hardware-style signal (cumulative revolution count + the event
    timestamp in **ticks** — 1/1024-second units, matching BLE CSC/CP wire
    format).

    A source populates whichever fields it can measure:
    - Rate sources (e.g. Keiser) set power/cadence; the encoder interpolates
      events from those rates via CountGenerator.
    - Event sources (e.g. a real hall-effect encoder, or the simulator) set
      crank/wheel events directly; the encoder passes them through.

    NaN marks an unknown rate; has_*_event marks whether the discrete fields
    are valid this tick. `last_update_tick` is stamped by the source on
    every fresh sample — consumers compare it against now_tick() to detect
    silence. A value of 0 means the source has never produced data.

    Keep this a plain data record — no methods, no asyncio — so it ports to
    a C struct on MCU unchanged.
    """

    power: float = math.nan
    cadence: float = math.nan
    speed: float = math.nan

    has_crank_event: bool = False
    crank_revs: int = 0
    crank_event_tick: int = 0

    has_wheel_event: bool = False
    wheel_revs: int = 0
    wheel_event_tick: int = 0

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
