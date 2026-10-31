"""Independent exact-arithmetic oracle: no implementation arithmetic imported."""
import argparse
from fractions import Fraction as F
import random
import subprocess


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--probe', required=True)
    args = p.parse_args()
    random.seed(731)
    cases = []
    for j in range(100):
        on, od = [(30, 1), (24, 1), (30000, 1001)][j % 3]
        sn, sd = on * 6, od
        rn, rd = [(1, 1), (1, 2), (2, 3), (2, 1)][j % 4]
        play, hold, tail = [random.randint(2, 130) for _ in range(3)]
        origin = random.randint(-500, 500)
        last = int(F((play + tail) * od * rn * sn, on * rd * sd))
        samples = [(i, 0, 1) for i in range(play + hold + tail)] if j < 5 else [
            (i, n, d) for i in [0, play-1, play, play+hold-1, play+hold, play+hold+tail-1]
            for n, d in [(0, 1), (1, 2), (1, 7)]]
        samples += [(-1, 0, 1), (play+hold+tail, 0, 1)]
        for i, n, d in samples:
            fields = (on, od, origin, play, hold, tail, rn, rd, sn, sd, last, origin+i, n, d)
            if not 0 <= i < play+hold+tail:
                expected = 'sample_out_of_range'
            else:
                presentation = F(i*d+n, d) * F(od, on)
                index = 0 if i < play else 1 if i < play+hold else 2
                start = [0, play, play+hold][index]
                local = presentation - F(start*od, on)
                sim = (F(0) if index == 0 else F(play*od*rn, on*rd))
                if index != 1:
                    sim += local * F(rn, rd)
                tick = sim * F(sn, sd)
                if tick.denominator != 1:
                    expected = 'unsupported_source_sample'
                elif tick > last:
                    expected = 'source_out_of_range'
                else:
                    expected = 'ok ' + ' '.join(map(str, [index, presentation.numerator,
                        presentation.denominator, local.numerator, local.denominator,
                        sim.numerator, sim.denominator, int(tick)]))
            cases.append((' '.join(map(str, fields)), expected))
    # Entire schedules that cannot represent uniform output frames exactly refuse.
    cases.append(('30 1 0 120 90 90 1 1 25 1 1000 0 0 1', 'unsupported_source_sample'))
    result = subprocess.run([args.probe], input='\n'.join(row for row, _ in cases)+'\n',
                            text=True, capture_output=True, check=True)
    actual = result.stdout.splitlines()
    assert len(actual) == len(cases)
    for (fixture, expected), found in zip(cases, actual):
        assert found == expected, (fixture, expected, found)
    print(f'sequence independent Fraction oracle: {len(cases)} samples passed')


if __name__ == '__main__':
    main()
