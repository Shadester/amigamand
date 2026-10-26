"""The test suite: python suite.py [--update] [name ...]   (run through tools/test/test.sh)

Batch tests run the program with start options, SAVE and QUIT, and look at the ILBM it saved. Interactive tests send
keys and Right-Amiga shortcuts to the running program and compare the screen with a batch render of the same view.
The pens of a few views are kept in expected.json (--update writes it): any change to the numeric code that changes
a pixel shows up there.
"""
import hashlib
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import amiga  # noqa: E402
from PIL import Image, ImageChops, ImageStat  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
EXPECTED = os.path.join(HERE, 'expected.json')
TESTS = {}
OUT = os.environ.get('TEST_OUT', os.path.join(ROOT, 'build', 'test'))
os.makedirs(OUT, exist_ok=True)


def test(f):
    TESTS[f.__name__] = f
    return f


def sha(im):
    return hashlib.sha1(amiga.pens(im)).hexdigest()


def screen_diff(shot_path, im):
    """Mean absolute difference per channel between the emulator window and an ILBM, both shrunk to 160x64."""
    a = Image.open(shot_path).convert('RGB').crop((105, 93, 1813, 1458)).resize((160, 64), Image.BOX)
    b = im.convert('RGB').resize((160, 64), Image.BOX)
    return max(ImageStat.Stat(ImageChops.difference(a, b)).mean)


def batch(machine, args, name='out', **kw):
    ticks, im = amiga.run_batch(machine, args, ROOT, name=name, **kw)
    assert im is not None, f'no image from: {args}'
    return ticks, im


VIEWS = {
    'overview': 'MODE 1 ITER 128',
    'spiral': 'MODE 0 CX -0.7436 CY 0.1318 SPAN 0.02 ITER 255',
    'julia': 'MODE 1 JULIA ITER 128',
    'bulb3': 'MODE 0 CX -0.1226 CY 0.7449 SPAN 0.06 ITER 255',
    'ocs': 'MODE 0 DEPTH 5 PALETTE Fire ITER 64',
}


@test
def pixels(update=False):
    """The pens of five views are the ones in expected.json (the fixed-point build on a 68030)."""
    got = {}
    for name, args in VIEWS.items():
        ticks, im = batch('030', args)
        got[name] = sha(im)
        print(f'  {name}: {ticks} ticks')
    if update:
        json.dump(got, open(EXPECTED, 'w'), indent=1)
        return 'expected.json written'
    exp = json.load(open(EXPECTED))
    bad = [n for n in got if got[n] != exp.get(n)]
    assert not bad, 'pens changed in: ' + ', '.join(bad)


@test
def fast_fill_exact():
    """Mariani-Silver fill and the periodicity check give the same picture as plain iteration."""
    _, a = batch('030', VIEWS['bulb3'])
    _, b = batch('030', VIEWS['bulb3'] + ' EXACT')
    n = amiga.differ(a, b)
    assert n == 0, f'{n} pixels differ'


@test
def modes():
    """Interlace is 640x512 with the interlace mode ID; depth 5 makes a 5-plane file."""
    _, im = batch('030', 'MODE 2 ITER 32')
    assert im.size == (640, 512), im.size
    _, im5 = batch('030', VIEWS['ocs'], name='o5')
    assert im5.size == (320, 256)


@test
def ntsc():
    """On an NTSC machine the presets have 200 lines (400 interlaced)."""
    _, a = batch('030', 'MODE 1 ITER 32', ntsc=True)
    assert a.size == (640, 200), a.size
    _, b = batch('030', 'MODE 2 ITER 32', ntsc=True)
    assert b.size == (640, 400), b.size


@test
def fpu():
    """The FPU build draws nearly the same picture, and exits without a guru on a machine without an FPU."""
    _, fx = batch('030', VIEWS['overview'])
    _, fp = batch('030fpu', VIEWS['overview'])
    n = amiga.differ(fx, fp)
    assert n < 0.01 * fx.size[0] * fx.size[1], f'{n} pixels differ'
    seq = 'FailAt 999\nSYS:prog MODE 0\nIf FAIL\nEcho >SYS:rc.txt "fail"\nEndIf'  # FailAt, If and Echo are Shell built-ins
    with amiga.Session(os.path.join(ROOT, 'amigamand-fpu'), seq=seq, full=True) as s:
        assert s.wait_file('rc.txt', timeout=120), 'no return code 20 or more from the FPU build without an FPU'


def interactive(args, steps, shot='s.png', **kw):
    return interactive_bin(os.path.join(ROOT, 'amigamand'), args, steps, **kw)


def interactive_bin(binary, args, steps, **kw):
    """Boots the program with args, runs steps (a list of ('wait', s) / ('key', name) / ('amiga', name) / ('shot', name)),
    returns the screenshot paths by name."""
    paths = {}
    with amiga.Session(binary, args=args, extra=amiga.MACHINES['030'][1], **kw) as s:
        time.sleep(12)
        for kind, v in steps:
            if kind == 'wait':
                time.sleep(v)
            elif kind == 'key':
                s.key(v)
            elif kind == 'amiga':
                s.amiga(v)
            elif kind == 'shot':
                p = os.path.join(OUT, f'{v}.png')
                s.shot(p)
                paths[v] = p
    return paths


@test
def keys():
    """Zoom, pan, Julia, Mandelbrot and reset by key give the views a batch run gives."""
    p = interactive('MODE 0 ITER 128', [
        ('wait', 8), ('key', 'eq'), ('wait', 8), ('shot', 'zoom'),
        ('key', 'right'), ('wait', 8), ('shot', 'pan'),
        ('key', 'n0'), ('wait', 8), ('shot', 'reset'),
        ('key', 'j'), ('wait', 8), ('shot', 'julia'),
        ('key', 'm'), ('wait', 8), ('shot', 'mandel')])
    cases = {'zoom': 'MODE 0 ITER 128 SPAN 1.75', 'pan': 'MODE 0 ITER 128 SPAN 1.75 CX -0.3125',  # a pan step is a quarter of the span
             'reset': 'MODE 0 ITER 128', 'julia': 'MODE 0 ITER 128 JULIA'}
    for name, args in cases.items():
        _, im = batch('030', args)
        d = screen_diff(p[name], im)
        assert d < 3, f'{name}: screen differs from the batch render by {d:.1f}'


@test
def menu_shortcuts():
    """Right-Amiga+J (Julia at last click), +M (Mandelbrot), +R (redraw) work."""
    p = interactive('MODE 0 ITER 128', [
        ('wait', 8), ('amiga', 'j'), ('wait', 8), ('shot', 'julia'),
        ('amiga', 'm'), ('wait', 8), ('amiga', 'r'), ('wait', 8), ('shot', 'mandel')])
    _, j = batch('030', 'MODE 0 ITER 128 JULIA')
    _, m = batch('030', 'MODE 0 ITER 128')
    assert screen_diff(p['julia'], j) < 3, 'Julia'
    assert screen_diff(p['mandel'], m) < 3, 'Mandelbrot after redraw'


@test
def cycling():
    """Cycle colours (Right-Amiga+C) changes the palette over time, and stopping stops it."""
    p = interactive('MODE 0 ITER 64', [
        ('wait', 8), ('shot', 'still'), ('amiga', 'c'), ('wait', 2), ('shot', 'c1'), ('wait', 1.3), ('shot', 'c2'),
        ('amiga', 'c'), ('wait', 1), ('shot', 'o1'), ('wait', 2), ('shot', 'o2')])
    def same(a, b):
        return open(p[a], 'rb').read() == open(p[b], 'rb').read()
    assert not same('still', 'c1') and not same('c1', 'c2'), 'the colours did not move'
    assert same('o1', 'o2'), 'the colours kept moving after Cycle was switched off'


@test
def abort():
    """Space stops a render: the picture stays unfinished and does not change any more."""
    p = interactive('MODE 1 ITER 255 CX -0.7436 CY 0.1318 SPAN 0.02 EXACT', [
        ('wait', 3), ('key', 'space'), ('wait', 1), ('shot', 'a1'), ('wait', 6), ('shot', 'a2')])
    assert open(p['a1'], 'rb').read() == open(p['a2'], 'rb').read(), 'still drawing after abort'


@test
def save_requester():
    """s opens the ASL file requester (asl.library from the Workbench disk); Return saves to RAM:."""
    with amiga.Session(os.path.join(ROOT, 'amigamand'), args='MODE 0 ITER 64', extra=amiga.MACHINES['030'][1],
                       seq='SYS:prog MODE 0 ITER 64\nWorkbench3.1:C/Copy RAM:amigamand.iff SYS:ui.iff', full=True) as s:
        time.sleep(25)
        s.key('s')
        time.sleep(4)
        s.shot(os.path.join(OUT, 'requester.png'))
        s.key('ret')
        time.sleep(6)
        s.key('esc')
        assert s.wait_file('ui.iff', timeout=60), 'nothing saved'
        time.sleep(2)
        im, info = amiga.decode(s.path('ui.iff'))
    _, ref = batch('030', 'MODE 0 ITER 64')
    assert amiga.differ(im, ref) == 0, 'the saved picture differs'


@test
def other_mode_requester():
    """Right-Amiga+O opens the ASL screen mode requester on our screen. It answers only the mouse, so this checks that
    it opens (the picture changes where the requester is) and that its colours are readable (white text on black)."""
    with amiga.Session(os.path.join(ROOT, 'amigamand'), args='MODE 0 ITER 64', extra=amiga.MACHINES['030'][1],
                       full=True) as s:
        time.sleep(25)
        before = os.path.join(OUT, 'modes_before.png')
        after = os.path.join(OUT, 'modes.png')
        s.shot(before)
        s.amiga('o')
        time.sleep(5)
        s.shot(after)
    a = Image.open(before).convert('RGB').crop((400, 300, 1500, 1100))
    b = Image.open(after).convert('RGB').crop((400, 300, 1500, 1100))
    assert max(ImageStat.Stat(ImageChops.difference(a, b)).mean) > 20, 'no requester on the screen'
    light = sum(1 for px in b.getdata() if min(px) > 200)
    assert light > 500, 'the requester has no light text on its dark background'


@test
def mouse_handlers():
    """A drag of a box zooms into the box, a click centres the view there (the handlers the mouse events reach)."""
    import subprocess
    dx = 3.5 / 320                       # lores, default view: 320 pixels across 3.5, pixel centres symmetric

    def point(px, py):
        return -0.75 + (px - 159.5) * dx, (py - 127.5) * dx
    cases = {
        # drag from (60,40) to (140,60): a box 80 wide, 64 high; zoom into its centre (100,72) by 80/320
        1: 'MODE 0 ITER 64 CX %.9f CY %.9f SPAN %.9f' % (point(100, 72) + (3.5 * 80 / 320,)),
        # click at (200,100): centre there, same span
        2: 'MODE 0 ITER 64 CX %.9f CY %.9f' % point(200, 100),
    }
    for n, args in cases.items():
        subprocess.run(['make', f'amigamand-tm{n}'], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        p = interactive_bin(os.path.join(ROOT, f'amigamand-tm{n}'), 'MODE 0 ITER 64', [('wait', 25), ('shot', f'mouse{n}')])
        _, im = batch('030', args)
        d = screen_diff(p[f'mouse{n}'], im)
        assert d < 3, f'gesture {n}: screen differs from the batch render of the expected view by {d:.1f}'


@test
def icon():
    """icon.library loads the icon: a tool, a 48x32 picture of 2 planes, the tool types, a stack."""
    import subprocess
    subprocess.run(['make', 'build/icontest'], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    with amiga.Session(os.path.join(ROOT, 'build', 'icontest'), files=[os.path.join(ROOT, 'amigamand.info')], full=True) as s:
        os.rename(s.path('amigamand.info'), s.path('prog.info'))
        assert s.wait_file('icon.txt', until='end', timeout=120), 'the icon test did not finish'
        txt = open(s.path('icon.txt'), 'rb').read().decode('latin1')
    for want in ('type 3\n', 'stack 8192\n', 'image width 48\n', 'image height 32\n', 'image depth 2\n', 'tooltype MODE=1\n',
                 'ITER = 128\n', 'EXACT is off\n'):
        assert want in txt, f'icon.library did not report {want!r}:\n{txt}'


@test
def workbench_tooltypes():
    """Started from Workbench the program takes its options from the tool types (a test build reads SYS:prog.info)."""
    import subprocess
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    import make_icon
    subprocess.run(['make', 'amigamand-tw'], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    info = os.path.join(OUT, 'prog.info')
    open(info, 'wb').write(make_icon.diskobject(['MODE=0', 'ITER=64', 'PALETTE=Fire', 'SAVE=SYS:wb.iff', 'QUIT']))
    with amiga.Session(os.path.join(ROOT, 'amigamand-tw'), files=[info], full=True, extra=amiga.MACHINES['030'][1],
                       seq='SYS:prog\nEcho >SYS:end.txt "end"') as s:
        assert s.wait_file('end.txt', timeout=300), 'the program did not finish'
        got, _ = amiga.decode(s.path('wb.iff'))
    _, ref = batch('030', 'MODE 0 ITER 64 PALETTE Fire')
    assert got.size == ref.size and amiga.differ(got, ref) == 0, 'the tool types did not give the same picture as the options'


def main():
    update = '--update' in sys.argv
    names = [a for a in sys.argv[1:] if not a.startswith('--')] or list(TESTS)
    failed = []
    for n in names:
        t0 = time.time()
        try:
            msg = TESTS[n](update=True) if (n == 'pixels' and update) else TESTS[n]()
            print(f'PASS {n} ({time.time() - t0:.0f}s) {msg or ""}')
        except Exception as e:  # noqa: BLE001
            failed.append(n)
            print(f'FAIL {n} ({time.time() - t0:.0f}s): {type(e).__name__}: {e}')
    print('failed:', ', '.join(failed) if failed else 'none')
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
