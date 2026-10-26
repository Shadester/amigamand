"""Boots a binary in FS-UAE and checks what it does. Used by tools/test/test.sh; see tools/test/README.md.

A machine is an A1200 with the user's own Kickstart (~/Documents/FS-UAE/Kickstarts). The program is started from
the Startup-Sequence of a directory hard drive, so what it writes to SYS: ends up in a directory on the Mac.
With full=True the user's Workbench HDF is mounted read-only as well and its Libs: are added, so asl.library,
icon.library and the C: commands (Copy, Assign) are there.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

EMU = os.environ.get('FSUAE', '/Applications/FS-UAE.app/Contents/MacOS/fs-uae')
FSUAE_DIR = os.path.expanduser('~/Documents/FS-UAE')
ROM = os.environ.get('KICKSTART', FSUAE_DIR + '/Kickstarts/Kickstart-v3.1-rev-40.68-1993-Commodore-A1200.rom')
WB = os.environ.get('WB', FSUAE_DIR + '/Hard Drives/workbench-311.hdf')

MACHINES = {  # name: (binary, emulator options)
    '020': ('amigamand', []),
    '030': ('amigamand', ['--cpu=68030', '--uae_fpu_model=68882']),
    '030fpu': ('amigamand-fpu', ['--cpu=68030', '--uae_fpu_model=68882']),
}
KEYS = dict(a=0, s=1, d=2, f=3, h=4, g=5, z=6, x=7, c=8, v=9, b=11, q=12, w=13, e=14, r=15, y=16, t=17,
            n1=18, n2=19, n3=20, n4=21, n6=22, n5=23, eq=24, n9=25, n7=26, minus=27, n8=28, n0=29, o=31, u=32,
            i=34, p=35, l=37, j=38, k=40, m=46, ret=36, left=123, right=124, down=125, up=126, space=49, esc=53)
ARROWS = (123, 124, 125, 126)


class Session:
    def __init__(self, binary, args='', files=(), extra=(), full=False, seq=None, ntsc=False, size=(960, 720)):
        self.dir = tempfile.mkdtemp(prefix='amigamand-test-')
        os.makedirs(self.dir + '/S')
        shutil.copy(binary, self.dir + '/prog')
        for f in files:
            shutil.copy(f, self.dir)
        lines = []
        if full:
            lines.append('Workbench3.1:C/Assign LIBS: Workbench3.1:Libs ADD')
            lines += ['Workbench3.1:C/MakeDir RAM:ENV', 'Workbench3.1:C/Assign ENV: RAM:ENV']  # asl.library wants ENV:
        lines.append(seq if seq else 'SYS:prog ' + args)
        open(self.dir + '/S/Startup-Sequence', 'w').write('\n'.join(lines) + '\n')
        opts = ['--amiga_model=A1200', '--kickstart_file=' + ROM, '--hard_drive_0=' + self.dir,
                '--hard_drive_0_label=SYS', '--joystick_port_1=none', '--fullscreen=0',
                f'--window_width={size[0]}', f'--window_height={size[1]}', '--floppy_drive_volume=0']
        if full:
            opts += ['--hard_drive_1=' + WB, '--hard_drive_1_read_only=1']
        if ntsc:
            opts += ['--ntsc_mode=1']
        self.p = subprocess.Popen([EMU] + opts + list(extra), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # ---- files the program wrote
    def path(self, name):
        return os.path.join(self.dir, name)

    def wait_file(self, name, until=None, timeout=300):
        """Waits until the file exists and (with until) contains that text, else until its size stops changing."""
        end, last = time.time() + timeout, -1
        while time.time() < end:
            time.sleep(1.5)
            f = self.path(name)
            if not os.path.exists(f):
                continue
            if until:
                if until.encode() in open(f, 'rb').read():
                    return True
            else:
                size = os.path.getsize(f)
                if size == last and size > 0:
                    return True
                last = size
        return False

    # ---- the window
    def window(self):
        import Quartz
        best = None
        for w in Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionAll, Quartz.kCGNullWindowID):
            if w.get('kCGWindowOwnerPID') == self.p.pid and w.get('kCGWindowLayer') == 0:
                b = w['kCGWindowBounds']
                if b['Width'] > 200 and b['Height'] > 200 and (
                        best is None or b['Width'] * b['Height'] > best['kCGWindowBounds']['Width'] * best['kCGWindowBounds']['Height']):
                    best = w
        return best

    def shot(self, path):
        w = self.window()
        subprocess.run(['screencapture', '-x', '-o', f"-l{w['kCGWindowNumber']}", path], check=True)

    def key(self, name, flags=0):
        """A key to the emulator. Posted to its process, so it needs neither focus nor accessibility rights."""
        import Quartz
        code = KEYS[name]
        if code in ARROWS:
            flags |= 0x00A00000  # real arrow keys carry the Fn and numeric-pad flags
        for down in (True, False):
            e = Quartz.CGEventCreateKeyboardEvent(None, code, down)
            if flags:
                Quartz.CGEventSetFlags(e, flags)
            Quartz.CGEventPostToPid(self.p.pid, e)
            time.sleep(0.08)

    def amiga(self, name):
        """Right-Amiga plus a key: a menu shortcut (right Command on the Mac)."""
        import Quartz
        e = Quartz.CGEventCreateKeyboardEvent(None, 54, True)
        Quartz.CGEventPostToPid(self.p.pid, e)
        time.sleep(0.15)
        self.key(name)
        e = Quartz.CGEventCreateKeyboardEvent(None, 54, False)
        Quartz.CGEventPostToPid(self.p.pid, e)
        time.sleep(0.1)

    def close(self):
        self.p.terminate()
        try:
            self.p.wait(10)
        except subprocess.TimeoutExpired:
            self.p.kill()
        shutil.rmtree(self.dir, ignore_errors=True)

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()


def run_batch(machine, args, root, name='out', timeout=900, **kw):
    """Runs the program with start options that end in SAVE ... QUIT, stdout to a log. Returns (ticks, Image or None)."""
    binary, extra = MACHINES[machine]
    binary = os.path.join(root, binary)
    seq = f'SYS:prog {args} SAVE SYS:{name}.iff QUIT >SYS:log.txt'
    with Session(binary, extra=extra, seq=seq, **kw) as s:
        ok = s.wait_file('log.txt', until='ticks', timeout=timeout)
        if not ok:
            return None, None
        ticks = int(re.search(r'render (\d+) ticks', open(s.path('log.txt')).read()).group(1))
        s.wait_file(name + '.iff')
        im = decode(s.path(name + '.iff'))[0] if os.path.exists(s.path(name + '.iff')) else None
        return ticks, im


# ---- IFF ILBM reading (ByteRun1 or raw), and comparing

def decode(path):
    from PIL import Image
    d = open(path, 'rb').read()
    assert d[:4] == b'FORM' and d[8:12] == b'ILBM', 'not an ILBM'
    assert struct.unpack('>I', d[4:8])[0] == len(d) - 8, 'FORM size does not match the file'
    pos, bmhd, cmap, body, camg = 12, None, None, None, None
    while pos < len(d):
        cid, ln = d[pos:pos + 4], struct.unpack('>I', d[pos + 4:pos + 8])[0]
        data = d[pos + 8:pos + 8 + ln]
        if cid == b'BMHD':
            bmhd = data
        elif cid == b'CMAP':
            cmap = data
        elif cid == b'BODY':
            body = data
        elif cid == b'CAMG':
            camg = struct.unpack('>I', data)[0]
        pos += 8 + ln + (ln & 1)
    w, h, _, _, nplanes, _, comp = struct.unpack('>HHhhBBB', bmhd[:11])
    rowbytes = ((w + 15) >> 4) << 1
    rows, i = [], 0
    for _y in range(h):
        planes = []
        for _p in range(nplanes):
            if comp == 1:
                out = bytearray()
                while len(out) < rowbytes:
                    n = body[i]
                    i += 1
                    if n < 128:
                        out += body[i:i + n + 1]
                        i += n + 1
                    elif n > 128:
                        out += bytes([body[i]]) * (257 - n)
                        i += 1
                assert len(out) == rowbytes, 'bad run'
                planes.append(out)
            else:
                planes.append(body[i:i + rowbytes])
                i += rowbytes
        row = bytearray(w)
        for x in range(w):
            v = 0
            for p in range(nplanes):
                v |= ((planes[p][x >> 3] >> (7 - (x & 7))) & 1) << p
            row[x] = v
        rows.append(bytes(row))
    assert i == len(body), 'BODY not used up'
    im = Image.frombytes('P', (w, h), b''.join(rows))
    im.putpalette(list(cmap))
    return im, dict(w=w, h=h, planes=nplanes, camg=camg)


def pens(im):
    return im.tobytes()


def differ(a, b):
    """Number of pixels whose pen differs (the two must be the same size)."""
    return sum(1 for x, y in zip(pens(a), pens(b)) if x != y)
