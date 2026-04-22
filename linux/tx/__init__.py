class CountGenerator:
    """Generates discrete count data and closest event time from continuous value

    Note that in real world, when the count up event happens, an interrupt is triggered, then the precise event time is recorded. However here, the event time is an approximation with the assumption that the value increases steadily.
    """

    def __init__(self) -> None:
        self.val_float = 0
        self.val_int = 0
        self.event_time_ms = 0
        self.notify = False
        self.rpm = 0

    def add(self, inc, now):
        self.val_float += inc
        self.round(now)

    def set(self, val, now):
        self.val_float = val
        self.round(now)

    def get(self):
        return self.val_int, self.event_time_ms

    def round(self, now):
        diff = self.val_float - self.val_int
        self.rpm = diff / (now - self.event_time_ms / 1024) * 60
        # print(diff)
        if diff >= 1:
            self.event_time_ms = (
                now * 1024
                - (now * 1024 - self.event_time_ms) * (diff - int(diff)) / diff
            )

            self.val_int = int(self.val_float)
            # self.notify = True
        # else:
        #     self.notify = False


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
