class CountGenerator:
    """Interpolates hardware-style revolution events from a continuous rate.

    A real encoder emits (cumulative_count, event_tick) on each interrupt.
    When only an instantaneous rate (e.g. cadence in rpm) is available, this
    class integrates the rate and emits a synthetic event whenever the
    running float count crosses the next integer. The event timestamp is
    interpolated linearly between polls, assuming the rate was constant
    across the interval.

    All time values are in ticks (1/1024 s each); see clock.now_tick().

    Usage: call inc_count() or set_count() at each poll, passing the current
    time as `now`. This marks the end of the measurement interval. The class
    then derives whether an integer crossing (event) occurred within that
    interval and back-calculates the exact tick when it happened.
    """

    def __init__(self) -> None:
        self.count_float = 0.0
        self.count_int = 0
        self.event_tick = 0

    def inc_count(self, inc: float, now: int) -> None:
        """Step the count by the given increment (e.g. cadence * dt / 60).

        `now` marks the end of the measurement interval — the event tick is
        derived retroactively if the count crossed an integer boundary.
        """
        self.count_float += inc
        self._compute_event(now)

    def set_count(self, val: float, now: int) -> None:
        """Set the absolute count directly.
        """
        self.count_float = val
        self._compute_event(now)

    def get_event(self) -> tuple[int, int]:
        return self.count_int, self.event_tick

    def _compute_event(self, now: int) -> None:
        """Derive whether an event (integer crossing) occurred in the past interval.

        Checks if count_float crossed an integer boundary since the last poll.
        If so, back-calculates the exact event_tick by linear interpolation.
        """
        diff = self.count_float - self.count_int
        dt_ticks = now - self.event_tick
        if diff >= 1:
            frac_over = (diff - int(diff)) / diff
            self.event_tick = int(now - dt_ticks * frac_over)
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
