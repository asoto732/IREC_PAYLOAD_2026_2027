"""Verify the default synthetic flight CSV: python verify.py [path] [flight_seconds]."""
import argparse
import csv
from collections import Counter
from pathlib import Path

HEADER = (
    't_us,sensor,flight_state,reef_state,status,alt_m,press_pa,temp_c,'
    'ax_g,ay_g,az_g,act_state,grants,motor_pos,motor_a,rail12_v,rail24_v,'
    'tgt_rate,meas_rate,drag,err,cmd_width,meas_width,event'
).split(',')


def verify(path, seconds):
    raw = path.read_bytes()
    # Also accept files copied through older PowerShell stdout redirection.
    text = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    rows = list(csv.reader(text.splitlines()))
    assert rows and rows[0] == HEADER, 'Incorrect CSV header'
    assert all(len(row) == 24 for row in rows), 'Every row must have 24 columns'
    data = list(csv.DictReader(text.splitlines()))
    end = (seconds + 1) * 1_000_000
    counts = Counter(row['sensor'] for row in data)
    assert counts == {'ADXL375': seconds * 800 + 512, 'BMP390': (seconds + 1) * 25, 'EVENT': 13}, counts
    assert data[0]['event'] == 'LOG_START:V1'
    assert data[-1]['event'] == 'LOG_STOP' and int(data[-1]['t_us']) == end
    assert data[-1]['flight_state'] == '6' and data[-1]['reef_state'] == '3'
    events = [row for row in data if row['sensor'] == 'EVENT']
    expected_events = [
        'LOG_START:V1', 'META:GROUND_REF_PA:101325', 'STATE:PAD_IDLE',
        'STATE:BOOST', 'FLAG:LAUNCH', 'STATE:COAST', 'STATE:DESCENT',
        'FLAG:APOGEE', 'APOGEE:120', 'STATE:DEPLOY', 'STATE:MAIN_DESCENT',
        'STATE:LANDED', 'LOG_STOP',
    ]
    assert [row['event'] for row in events] == expected_events
    assert {row['flight_state'] for row in data} == set('0123456')
    for sensor, period, start in [('ADXL375', 1250, 360_000), ('BMP390', 40_000, 0)]:
        stream = [row for row in data if row['sensor'] == sensor]
        assert [int(row['t_us']) for row in stream] == list(range(start, end, period))
        relevant = {'t_us', 'sensor', 'flight_state', 'reef_state', 'status'}
        relevant |= {'ax_g', 'ay_g', 'az_g'} if sensor == 'ADXL375' else {'alt_m', 'press_pa', 'temp_c'}
        for row in stream:
            assert all(row[key] != '' for key in relevant)
            assert all(row[key] == '' for key in set(HEADER) - relevant)
    for sensor in counts:
        times = [int(row['t_us']) for row in data if row['sensor'] == sensor]
        assert times == sorted(times), f'Nonmonotonic {sensor} timestamps'
    assert all(row['status'] == '0x0' for row in data)
    assert float(next(row for row in data if row['sensor'] == 'BMP390')['press_pa']) == 101325
    barometer = [row for row in data if row['sensor'] == 'BMP390']
    assert max(float(row['alt_m']) for row in barometer) > 119, 'Synthetic altitude must vary'
    print(f'PASS {path}: 24 columns; {counts["ADXL375"]} acceleration + '
          f'{counts["BMP390"]} barometer + {counts["EVENT"]} events; '
          '800/25 Hz timestamps, PAD history, BOOST/LANDED, units and blanks.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('path', nargs='?', type=Path, default=Path('demo.csv'))
    parser.add_argument('flight_seconds', nargs='?', type=int, default=10)
    args = parser.parse_args()
    if not 1 <= args.flight_seconds <= 3600:
        parser.error('flight_seconds must be 1..3600')
    verify(args.path, args.flight_seconds)
