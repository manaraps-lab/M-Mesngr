#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Generates MANARA's 21 built-in avatars (96x96, gradient + person glyph).
#   python3 tools/gen_avatars.py src/resources/avatars
import os, sys
from PIL import Image, ImageDraw
PAL = [('#2C6BD8', '#1B3D9F'), ('#F28A0C', '#E0561A'), ('#1D9A8A', '#0F6B66'), ('#7A5AF8', '#4B2FC4'),
       ('#E0457B', '#B0245A'), ('#2F9E44', '#1B6E2D'), ('#0EA5E9', '#0369A1'), ('#D97706', '#92400E'),
       ('#6366F1', '#3730A3'), ('#14B8A6', '#0F766E'), ('#EF4444', '#B91C1C'), ('#64748B', '#334155'),
       ('#A855F7', '#6B21A8'), ('#22C55E', '#15803D'), ('#F59E0B', '#B45309'), ('#3B82F6', '#1E40AF'),
       ('#EC4899', '#9D174D'), ('#06B6D4', '#0E7490'), ('#84CC16', '#4D7C0F'), ('#F97316', '#C2410C'),
       ('#8B5CF6', '#5B21B6')]
def hexrgb(h): return tuple(int(h[i:i+2], 16) for i in (1, 3, 5))
def make(c0, c1, n=96, ss=4):
    N = n * ss
    a, b = hexrgb(c0), hexrgb(c1)
    im = Image.new('RGB', (N, N)); px = im.load()
    for y in range(N):
        for x in range(N):
            t = (x + y) / (2.0 * N)
            px[x, y] = tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))
    d = ImageDraw.Draw(im, 'RGBA'); w = (255, 255, 255, 235)
    d.ellipse([N*.33, N*.20, N*.67, N*.54], fill=w)                  # head
    d.pieslice([N*.16, N*.58, N*.84, N*1.28], 180, 360, fill=w)      # shoulders
    return im.resize((n, n), Image.LANCZOS)
out = sys.argv[1] if len(sys.argv) > 1 else '.'
# 0..19 are the picker avatars; default.png is the neutral one used before a choice is made
for i, (c0, c1) in enumerate(PAL[:20]):
    make(c0, c1).save(os.path.join(out, f'{i}.png'))
make('#64748B', '#334155').save(os.path.join(out, 'default.png'))
print('21 avatars written')
