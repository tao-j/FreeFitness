# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2023-2026 Tao Jin
"""Time base and conversion primitives for the rate/event pipeline.

The pipeline's single canonical time unit is the **tick** — one 1/1024-second
period. This is the BLE CSC/CP and ANT+ speed/cadence event-time wire
encoding. All int variables named `*_tick` carry this unit; derive seconds
only where physics needs them (power_to_speed, dt integration) by dividing
by 1024.

Porting to MCU: replace `get_tick_now()` with a thin wrapper over
`esp_timer_get_time()` or `millis()` scaled to 1/1024 s.
"""

import time


def get_tick_now() -> int:
    """Wall-clock time in ticks (1/1024 s each).

    Monotonic within a session; matches the BLE event-time wire encoding
    directly (subject to 16-bit truncation at the protocol boundary).
    """
    return int(time.time() * 1024)


class RateEventBridge:
    """Bidirectional bridge between the rate view and event view of a rotating channel.

    A rotating channel (crank, wheel) has two equivalent representations:
      - rate view: revolutions per second (continuous)
      - event view: cumulative revolution count + timestamp of the most
        recent integer crossing (discrete, 1/1024 s ticks, matching BLE
        CSC/CP and ANT+ wire encoding)

    A source populates whichever half it natively measures — this bridge
    fills in the other half so the encoder always has both.

    Use one of two feed methods per tick:
      - feed_rate(rate_rps, now_tick): integrate forward; back-interpolate an
        event tick when the running count crosses the next integer.
      - feed_event(cum_revs, event_tick, now_tick): adopt hardware events;
        derive the rate from Δcount / Δtick across consecutive events.

    Both feeds converge on the same outputs: `count_int`, `event_tick`,
    `rate_rps`.

    Unit note: rate is in revs/sec. Callers convert to the channel's
    natural unit (rpm for crank, m/s for wheel via wheel circumference).
    All timestamp arguments are ticks (1/1024 s); see get_tick_now().
    """

    def __init__(self) -> None:
        self.count_float = 0.0
        self.count_int = 0
        self.event_tick = 0
        self.rate_rps = 0.0

        self._last_feed_tick = 0
        self._last_event_revs = 0
        self._last_event_tick = 0

    def feed_rate(self, rate_rps: float, now_tick: int) -> None:
        """Step the count forward by rate × elapsed seconds since last feed."""
        if self._last_feed_tick == 0:
            self._last_feed_tick = now_tick
            self.event_tick = now_tick
            self.rate_rps = rate_rps
            return
        dt_sec = (now_tick - self._last_feed_tick) / 1024
        self._last_feed_tick = now_tick
        self.count_float += rate_rps * dt_sec
        self.rate_rps = rate_rps
        self._interpolate_event(now_tick)

    def feed_event(self, cum_revs: int, event_tick: int, now_tick: int) -> None:
        """Adopt a source-stamped event; derive rate from Δ against last event.

        Deduped internally: re-feeding the same (cum_revs, event_tick) is a
        no-op, so sources can set their event fields every poll without
        worrying about whether a new integer crossing actually occurred.
        """
        self._last_feed_tick = now_tick
        if event_tick != self._last_event_tick and cum_revs > self._last_event_revs:
            dt_tick = event_tick - self._last_event_tick
            if dt_tick > 0 and self._last_event_tick != 0:
                d_revs = cum_revs - self._last_event_revs
                self.rate_rps = d_revs * 1024 / dt_tick
            self._last_event_revs = cum_revs
            self._last_event_tick = event_tick
        self.count_int = cum_revs
        self.count_float = float(cum_revs)
        self.event_tick = event_tick

    def get_event(self) -> tuple[int, int]:
        return self.count_int, self.event_tick

    def get_rate_rps(self) -> float:
        return self.rate_rps

    def _interpolate_event(self, now_tick: int) -> None:
        """Back-calculate the event tick when the count crosses an integer boundary."""
        diff = self.count_float - self.count_int
        if diff >= 1:
            dt_tick = now_tick - self.event_tick
            frac_over = (diff - int(diff)) / diff
            self.event_tick = int(now_tick - dt_tick * frac_over)
            self.count_int = int(self.count_float)


def power_to_speed(power):
    """
    power in watts
    speed in m/s
    """
    Cd = 0.9
    A = 0.5
    rho = 1.225
    Crr = 0.0045
    F_gravity = 75 * 9.81

    # Define the power equations
    coeff_P_drag = 0.5 * Cd * A * rho  # * v**3
    coeff_P_roll = Crr * F_gravity  # * v

    # P_drag v^3 + P_roll v + (-power) = 0
    p = coeff_P_roll / coeff_P_drag
    q = -power / coeff_P_drag

    delta = p**3 / 27 + q**2 / 4
    cubic_root = lambda x: x ** (1.0 / 3) if x > 0 else -((-x) ** (1.0 / 3))
    u = cubic_root(-q / 2 + delta ** (1.0 / 2))
    v = cubic_root(-q / 2 - delta ** (1.0 / 2))

    return u + v
