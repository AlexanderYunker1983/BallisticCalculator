# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline independent RKF45 reference. Python 3 standard library only.

Does not import/call BallisticCore. Upper-atmosphere rows are shared input data.
RKF45: Fehlberg, NASA TR R-315 (1969). Lower atmosphere: PDAS atmos.py.
Usage: python scripts/reference-solver.py [--check]
"""
import argparse
import bisect
import functools
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
R, G, AREA = 6371100.0, 9.80665, 4.908738521875
MASS, FUEL = [70480, 29920, 7700], [60380, 26360, 7346]
THRUST, EXHAUST = [1470000, 392000, 110000], [3297.5, 3498, 3295.6]
TIMES = []
for f, e, t in zip(FUEL, EXHAUST, THRUST):
    TIMES.append((TIMES[-1] if TIMES else 0) + f * e / t)
UPPER = [tuple(map(float, row.split(','))) for row in re.findall(r'\{([^}]+)\}', (ROOT / 'src/core/upper_atmosphere.inc').read_text())]
CASES = [
    ('default', 3000, 40, 310, 14.97),
    ('optimized_seed', 3000, 40, 353.527598424922, 9.07661185847695),
    ('payload_3500', 3500, 40, 310, 14.97),
    ('vertical_30', 3000, 30, 310, 14.97),
    ('turn_330', 3000, 40, 330, 12),
    ('turn_250', 3000, 40, 250, 20),
    ('angle_5', 3000, 40, 310, 5),
    ('angle_30', 3000, 40, 310, 30),
    ('early_cutoff', 3000, 40, 310, 14.97, 570.123456789),
]


def lower(height):
    h = height * 6369000 / (height + 6369000)
    levels = [0, 11000, 20000, 32000, 47000, 51000, 71000, 84852]
    temperatures = [288.15, 216.65, 216.65, 228.65, 270.65, 270.65, 214.65, 186.946]
    pressures = [1, .22336110, .054032950, .0085666784, .0010945601, .00066063531, .000039046834, .00000368501]
    gradients = [-.0065, 0, .001, .0028, 0, -.0028, -.002, 0]
    i = bisect.bisect_right(levels, h) - 1
    delta, base, lapse = h - levels[i], temperatures[i], gradients[i]
    temperature = base + lapse * delta
    ratio = pressures[i] * (math.exp(-.034163195 * delta / base) if lapse == 0 else (base / temperature) ** (.034163195 / lapse))
    return temperature, 1.225 * ratio * 288.15 / temperature


@functools.lru_cache(maxsize=300001)
def air_node(height):
    if height <= 86000:
        return lower(height)
    i = bisect.bisect_left([row[0] * 1000 for row in UPPER], height)
    hi, temp, _, density = UPPER[i]
    lo, a = (86000, lower(86000)) if i == 0 else (UPPER[i-1][0] * 1000, (UPPER[i-1][1], UPPER[i-1][3]))
    x = (height - lo) / (hi * 1000 - lo)
    return a[0] + x * (temp - a[0]), a[1] + x * (density - a[1])


def air(height):
    height = max(0, min(300000, height))
    i = int(height)
    a, b = air_node(i), air_node(min(300000, i + 1))
    x = height - i
    temp, rho = a[0] + x * (b[0] - a[0]), a[1] + x * (b[1] - a[1])
    return rho, math.sqrt(1.4 * 287.05287 * temp)


def equations(case, stage):
    _, payload, t0, t1, degrees = case[:5]
    phi1, end = math.radians(degrees), case[5] if len(case) > 5 else TIMES[-1]
    slope = -phi1 / (end - t1)
    quadratic = (math.pi/2 - phi1 - slope*(t0-t1)) / (t0-t1)**2
    start = 0 if stage == 0 else TIMES[stage-1]
    initial_mass = payload + sum(MASS[stage:])

    def rhs(t, y):
        speed, theta, radius, arc = y
        phi = math.pi/2 if t <= t0 else phi1 * (end-t)/(end-t1) if t >= t1 else phi1 + slope*(t-t1) + quadratic*(t-t1)**2
        alpha = phi - theta + arc
        mass = initial_mass - (t-start)*THRUST[stage]/EXHAUST[stage]
        rho, sound = air(radius-R)
        mach = speed/sound
        cx = .29 if mach <= .8 else mach-.51 if mach <= 1.068 else .089+.5/mach
        cya = 2.8 if mach <= .25 else 2.8+.447*(mach-.25) if mach <= 1.1 else 3.18-.660*(mach-1.1) if mach <= 1.6 else 2.85+.350*(mach-1.6) if mach <= 3.6 else 3.55
        pressure = rho * speed**2 * AREA / 2
        axial, lift = THRUST[stage]-pressure*cx, pressure*(cya-cx)*alpha
        tangent = (axial*math.cos(alpha)-lift*math.sin(alpha))/mass
        normal = (axial*math.sin(alpha)+lift*math.cos(alpha))/mass
        gravity = G*(R/radius)**2
        return [tangent-gravity*math.sin(theta), 0 if abs(speed) <= .5 else normal/speed-math.cos(theta)*(gravity/speed-speed/radius), speed*math.sin(theta), speed/radius*math.cos(theta)]
    return rhs


C = [0, 1/4, 3/8, 12/13, 1, 1/2]
A = [[], [1/4], [3/32, 9/32], [1932/2197, -7200/2197, 7296/2197], [439/216, -8, 3680/513, -845/4104], [-8/27, 2, -3544/2565, 1859/4104, -11/40]]
B = [16/135, 0, 6656/12825, 28561/56430, -9/50, 2/55]
LOW = [25/216, 0, 1408/2565, 2197/4104, -1/5, 0]


def integrate(rhs, begin, end, y, tolerance=1.0):
    t, step = begin, min(.1, end-begin)
    for _ in range(1000000):
        if t >= end:
            return y
        step = min(step, end-t)
        if t+step == t:
            raise RuntimeError('Reference step underflow')
        k = []
        for c, row in zip(C, A):
            state = [value + step*sum(weight*derivative[j] for weight, derivative in zip(row, k)) for j, value in enumerate(y)]
            k.append(rhs(t+c*step, state))
        high = [value+step*sum(weight*derivative[j] for weight, derivative in zip(B, k)) for j, value in enumerate(y)]
        error = [step*sum((b-a)*derivative[j] for a, b, derivative in zip(LOW, B, k)) for j in range(len(y))]
        scales = [tolerance*(1e-10 + 1e-11*max(abs(a), abs(b))) for a, b in zip(y, high)]
        norm = max(abs(e)/s for e, s in zip(error, scales))
        if norm <= 1:
            t += step
            y = high
        step *= min(4, max(.1, .9*norm**(-.2))) if norm > 0 else 4
        step = min(.5, step)
    raise RuntimeError('Reference step limit')


def trajectory(case, tolerance):
    y, begin = [0, math.pi/2, R, 0], 0
    cutoff = case[5] if len(case) > 5 else TIMES[-1]
    boundaries = sorted(set(t for t in [case[2], case[3], *TIMES, cutoff] if t <= cutoff))
    for end in boundaries:
        stage = bisect.bisect_right(TIMES, begin)
        if end > begin:
            y = integrate(equations(case, stage), begin, end, y, tolerance)
        begin = end
    return [y[2]-R, y[0], y[1], y[3]]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true', help='Check references without rewriting the fixture')
    args = parser.parse_args()
    value = integrate(lambda t, y: [y[0]], 0, 1, [1])[0]
    assert abs(value-math.e) < 1e-9
    lines = ['// Generated offline by scripts/reference-solver.py; RKF45, not BallisticCore.']
    for case in CASES:
        coarse, fine = trajectory(case, .01), trajectory(case, .001)
        assert abs(coarse[0]-fine[0]) < .01 and abs(coarse[1]-fine[1]) < .0001, (case, coarse, fine)
        assert abs(coarse[2]-fine[2]) < 1e-9 and abs(coarse[3]-fine[3]) < 1e-10, (case, coarse, fine)
        numbers = ', '.join(format(x, '.17g') for x in [*case[1:5], *fine, case[5] if len(case) > 5 else 0])
        lines.append('{"' + case[0] + '", ' + numbers + '},')
        print(case[0], fine, 'refinement', [a-b for a, b in zip(coarse, fine)], flush=True)
    content = '\n'.join(lines) + '\n'
    path = ROOT / 'tests/core/reference_cases.inc'
    if args.check:
        assert path.read_text() == content, 'Reference fixture differs; review before regeneration'
    else:
        path.write_text(content, encoding='utf-8')


if __name__ == '__main__':
    main()
