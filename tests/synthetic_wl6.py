"""Generate inert data from WOLFSRC constants. Never reads registered assets."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

registered, probe, shareware = map(str, sys.argv[1:4])
layout = json.loads(subprocess.check_output([probe], text=True))
assert layout['maps'] == 60 and layout['lastmusic'] == 27
assert layout['graphics'] == 149 and layout['pictures'] == 132
assert layout['audio'] == layout['music'] + layout['lastmusic'] == 288


def pack(fmt, *values):
    return struct.pack('<' + fmt, *values)


def make_data(root):
    def write(name, data):
        (root / (name + '.WL6')).write_bytes(data)

    # Complete Huffman tree with byte-valued leaves; LSB-first encoded bytes
    # are their own compressed representation. Root is node 254.
    nodes = []

    def tree(prefix=0, depth=0):
        if depth == 8:
            return prefix
        left = tree(prefix, depth + 1)
        right = tree(prefix | (1 << depth), depth + 1)
        nodes.append((left, right))
        return 256 + len(nodes) - 1

    assert tree() == 510
    write('VGADICT', b''.join(pack('HH', *n) for n in nodes))
    offsets, graphics = [], bytearray()
    for chunk in range(layout['graphics']):
        offsets.append(len(graphics))
        if chunk == 0:
            payload = pack('HH', 8, 8) * layout['pictures']
        elif chunk == layout['font'] or chunk == layout['font'] + 1:
            payload = pack('H', 1) + pack('H', 770) * 256 + bytes([1]) * 256 + bytes(256)
        elif chunk == layout['tile']:
            graphics.extend(bytes(layout['tiles'] * 64))
            continue
        elif layout['startpics'] <= chunk < layout['startpics'] + layout['pictures']:
            payload = bytes(64)
        else:
            payload = b'^E\0'  # inert article/demo placeholder; never executed
        graphics.extend(pack('I', len(payload)) + payload)
    offsets.append(len(graphics))
    write('VGAHEAD', b''.join(pack('I', n)[:3] for n in offsets))
    write('VGAGRAPH', graphics)

    maps = bytearray(layout['maps'] * 38)
    plane = pack('5H', 8, 8192, 0xabcd, 4096, 0)
    for m in range(layout['maps']):
        offset = len(maps)
        maps.extend(plane * 2)
        maps[m * 38:(m + 1) * 38] = pack('IIIHHHHH16s', offset, offset + len(plane), 0,
                                                   len(plane), len(plane), 0, 64, 64, b'SYNTHETIC')
    # Nonzero map header offsets are required, including the first map.
    # Adjust plane offsets for the four-byte prefix.
    maps = bytearray(b'FAKE' + maps)
    for m in range(layout['maps']):
        for p in range(2):
            at = 4 + m * 38 + p * 4
            maps[at:at + 4] = pack('I', struct.unpack_from('<I', maps, at)[0] + 4)
    write('GAMEMAPS', maps)
    write('MAPHEAD', pack('H', 0xabcd) + b''.join(pack('I', 4 + m * 38) for m in range(layout['maps'])))
    audio, offsets = bytearray(), []
    for chunk in range(layout['audio']):
        offsets.append(len(audio))
        audio.extend(bytes(6 if chunk < layout['adlib'] else 23 if chunk < layout['digi'] else 2))
    offsets.append(len(audio))
    write('AUDIOHED', b''.join(pack('I', n) for n in offsets))
    write('AUDIOT', audio)
    pages = [bytes(4096), bytes(8), bytes(4)]
    offsets = [24, 24 + 4096, 24 + 4096 + 8]
    write('VSWAP', pack('HHH', 3, 1, 2) + pack('III', *offsets) + pack('HHH', 4096, 8, 4) + b''.join(pages))


def run(game, root, success):
    result = subprocess.run([game, '-data', str(root), '-datacheck', '-nosound'],
                            capture_output=True, text=True, timeout=10)
    assert (result.returncode == 0) == success, (result.returncode, result.stdout, result.stderr)
    if success:
        assert 'PASS: WL6' in result.stdout
    else:
        assert result.returncode in (1, 2), result.stderr  # no signal/crash
        assert result.stderr.strip(), 'Missing diagnostic'
    return result.stderr


if len(sys.argv) == 5:
    output = Path(sys.argv[4])
    output.mkdir(parents=True, exist_ok=True)
    make_data(output)
    sys.exit(0)

with tempfile.TemporaryDirectory(prefix='wolf-synthetic-wl6-') as directory:
    root = Path(directory)
    make_data(root)
    run(registered, root, True)
    assert 'WL1' in run(shareware, root, False)
    # A WL6 graphics layout renamed to WL1 must fail the shareware header check.
    for path in list(root.glob('*.WL6')):
        path.with_suffix('.WL1').write_bytes(path.read_bytes())
    assert 'WL1' in run(shareware, root, False)
    font_offset = int.from_bytes((root / 'VGAHEAD.WL6').read_bytes()[3:6], 'little')
    cases = {
        'wrong graphics count': ('VGAHEAD', lambda b: b[:-3]),
        'out of range graphics offset': ('VGAHEAD', lambda b: b[:-3] + b'\xfe\xff\xff'),
        'descending graphics offset': ('VGAHEAD', lambda b: b[:6] + bytes(3) + b[9:]),
        'invalid picture dimensions': ('VGAGRAPH', lambda b: b[:4] + b'\xff\xff' + b[6:]),
        'invalid font glyph range': ('VGAGRAPH', lambda b: b[:font_offset + 6] + b'\xff\xff' + b[font_offset + 8:]),
        'truncated dictionary': ('VGADICT', lambda b: b[:5]),
        'missing map 60': ('MAPHEAD', lambda b: b[:-4] + b'\xff' * 4),
        'truncated map table': ('MAPHEAD', lambda b: b[:-4]),
        'out of range map plane': ('GAMEMAPS', lambda b: b[:4] + b'\xff' * 4 + b[8:]),
        'wrong audio count': ('AUDIOHED', lambda b: b[:-4]),
        'out of range audio offset': ('AUDIOHED', lambda b: b[:-4] + b'\xff' * 4),
        'descending audio offset': ('AUDIOHED', lambda b: b[:8] + bytes(4) + b[12:]),
        'bad VSWAP range': ('VSWAP', lambda b: b[:6] + b'\xff' * 4 + b[10:]),
        'truncated graphics payload': ('VGAGRAPH', lambda b: b[:4]),
    }
    for label, (name, mutate) in cases.items():
        path = root / (name + '.WL6')
        original = path.read_bytes()
        path.write_bytes(mutate(original))
        run(registered, root, False)
        path.write_bytes(original)
        print('PASS: rejected', label)
    path = root / 'VSWAP.WL6'
    path.unlink()
    assert 'VSWAP.WL6' in run(registered, root, False)
print('PASS: synthetic WL6: 60 maps, 149 graphics chunks, 288 audio chunks (27 music)')
