"""Wall clock in units the rest of the pipeline cares about.

The pipeline's single canonical time unit is the **tick** — one 1/1024-second
period. This is the BLE CSC/CP and ANT+ speed/cadence event-time wire
encoding, and using it everywhere removes the seconds/ticks mixing that
once lived inside CountGenerator. All int fields named `*_tick` carry this
unit; derive seconds only where physics needs them (power_to_speed, dt
integration) by dividing by 1024.

Porting to MCU: replace `now_tick()` with a thin wrapper over
`esp_timer_get_time()` or `millis()` scaled to 1/1024 s.
"""

import time


def now_tick() -> int:
    """Wall-clock time in ticks (1/1024 s each).

    Monotonic within a session; matches the BLE event-time wire encoding
    directly (subject to 16-bit truncation at the protocol boundary).
    """
    return int(time.time() * 1024)
