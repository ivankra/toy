#!/usr/bin/python2.5
import sys, os, random

def run(cmd, input):
    file('/tmp/regtest.in', 'wb').write(input)
    n = os.system(cmd + ' </tmp/regtest.in >/tmp/regtest.out')
    if n != 0: return None
    return file('/tmp/regtest.out', 'rb').read()

def test(seed):
    sys.stderr.write('.')
    sys.stderr.flush()

    r = random.Random(seed)
    data = hex(r.getrandbits(r.randint(1, 50000000*4)))

    bzip2_output = run('bzip2 -9 -z -c', data)
    #mtbzip2_output = run('./mtbzip2 -9 -p 2', data)
    mtbzip2_output = run('mpirun -n 3 ./mpibzip2 -9', data)
    if bzip2_output == mtbzip2_output: return True

    s = '/tmp/input%d' % seed
    file(s, 'wb').write(data)
    sys.stderr.write('\nFound different outputs with seed=%s, input written to %s\n' % (seed, s))
    return False

if len(sys.argv) == 1:
    while True:
        test(random.randint(1, 1000000000))
else:
    for s in sys.argv[1:]:
        test(int(s))
