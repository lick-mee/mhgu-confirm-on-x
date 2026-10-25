"""Swap the Cross and Circle button icons.

Button prompts are characters of the button font (GUI\\02_common\\font\\button_festa), drawn from the texture
button_festa_00_ID (512x512 BC3, Switch block-linear). Its top row holds Square, Triangle, Cross, Circle in
48-pixel cells. Towns use the copy in loc/arc/resident.arc (loaded at startup), quests their own copy in
eng/arc/quest_eng.arc; this swaps the Cross and Circle cells in both, or only the town one with --town-only.

Each archive is built from the one the game would load otherwise (the last mod in folder order that has it, else
the game files), so other mods' changes to it (e.g. a font mod) are kept. Run it again after updating those mods.

Usage: python make_icons.py [--town-only] [recomp folder]
"""
import os
import struct
import sys
import zlib

TOWN = (os.path.join('romfs', 'nativeNX', 'loc', 'arc', 'resident.arc'), 'GUI\\02_common\\font\\button_festa_00_ID')
QUEST = (os.path.join('romfs', 'nativeNX', 'eng', 'arc', 'quest_eng.arc'),
         'eng\\GUI\\02_common\\font\\button_festa_00_ID')
OUT_MOD = 'zzz-confirm-on-x-icons'
CELL = 48
CROSS_X, CIRCLE_X, ROW_Y = 96, 144, 0


def read_entries(arc):
    magic, _version, count = struct.unpack_from('<4sHH', arc, 0)
    assert magic == b'ARC\0', 'not an MT archive'
    entries = []
    for i in range(count):
        off = 12 + i * 0x50
        name = arc[off:off + 0x40].split(b'\0')[0].decode('latin1')
        type_hash, csize, dsize, data_off = struct.unpack_from('<IIII', arc, off + 0x40)
        entries.append((off, name, type_hash, csize, dsize, data_off))
    return entries


# Tegra block-linear layout (64-byte x 8-row GOBs, 16 GOBs per block), in 16-byte units.
def gob_addr(xb, y, width_bytes, bh=16):
    gobs_x = (width_bytes + 63) // 64
    gob_y = y // 8
    base = (((gob_y // bh) * gobs_x + xb // 64) * bh + gob_y % bh) * 512
    x, r = xb % 64, y % 8
    return base + (((x // 32) << 8) | ((r // 2) << 6) | (((x % 32) // 16) << 5) | ((r % 2) << 4) | (x % 16))


def swap_cells(tex):
    w2 = struct.unpack_from('<I', tex, 8)[0]
    width, height, fmt = (w2 >> 6) & 0x1FFF, (w2 >> 19) & 0x1FFF, tex[0xD]
    assert fmt == 23, 'expected a BC3 texture, got format %d' % fmt
    data = bytearray(tex[0x18:])
    row_bytes = (width // 4) * 16  # one row of 4x4 blocks
    for by in range(ROW_Y // 4, (ROW_Y + CELL) // 4):
        for bx in range(CELL // 4):
            a = gob_addr((CROSS_X // 4 + bx) * 16, by, row_bytes)
            b = gob_addr((CIRCLE_X // 4 + bx) * 16, by, row_bytes)
            data[a:a + 16], data[b:b + 16] = data[b:b + 16], data[a:a + 16]
    return tex[:0x18] + bytes(data)


def source_arc(root, arc_rel):
    mods = os.path.join(root, 'mods')
    for name in sorted(os.listdir(mods), reverse=True):
        path = os.path.join(mods, name, arc_rel)
        if name != OUT_MOD and os.path.isfile(path):
            return path
    return os.path.join(root, 'data', arc_rel)


def patch(root, arc_rel, texture):
    src = source_arc(root, arc_rel)
    arc = bytearray(open(src, 'rb').read())
    for off, name, _hash, csize, dsize, data_off in read_entries(arc):
        if name != texture:
            continue
        tex = swap_cells(zlib.decompress(bytes(arc[data_off:data_off + csize])))
        packed = zlib.compress(tex, 9)
        new_off = len(arc)  # appended; every other entry stays where it is
        arc += packed
        struct.pack_into('<III', arc, off + 0x44, len(packed), (dsize & ~0x1FFFFFFF) | len(tex), new_off)
        break
    else:
        raise SystemExit('%s has no %s' % (src, texture))
    dst = os.path.join(root, 'mods', OUT_MOD, arc_rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, 'wb').write(arc)
    print('from %s\nwrote %s' % (src, dst))


def main(args):
    town_only = '--town-only' in args
    rest = [a for a in args if a != '--town-only']
    root = rest[0] if rest else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
    patch(root, *TOWN)
    quest_out = os.path.join(root, 'mods', OUT_MOD, QUEST[0])
    if town_only:
        if os.path.isfile(quest_out):
            os.remove(quest_out)
            print('removed %s' % quest_out)
    else:
        patch(root, *QUEST)


if __name__ == '__main__':
    main(sys.argv[1:])
