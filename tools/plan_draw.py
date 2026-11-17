"""Draws plan.txt (written by plan_test.exe) to plan.png: streets dark, footpaths light (dashed grey = street edge,
not built), buildings by kind, entrances green. Usage: python plan_draw.py [plan.txt] [plan.png]"""
import sys
from PIL import Image, ImageDraw
src = sys.argv[1] if len(sys.argv) > 1 else 'plan.txt'
dst = sys.argv[2] if len(sys.argv) > 2 else 'plan.png'
L = [l.split() for l in open(src)]
W, H = [float(x) for x in L[0][1:3]]
k = 2.0
img = Image.new('RGB', (int(W * k) + 40, int(H * k) + 40), (120, 150, 90))
d = ImageDraw.Draw(img)
P = lambda u, v: (20 + u * k, 20 + (H - v) * k)
d.rectangle([P(0, H), P(W, 0)], outline=(255, 230, 0), width=2)
col = {'LIVING': (90, 120, 200), 'SHOP': (220, 160, 60), 'SCHOOL': (90, 190, 90), 'KINDERGARTEN': (150, 220, 120)}
for l in L[1:]:
    t = l[0]; f = [float(x) for x in l[1:6] if x.replace('.', '').replace('-', '').replace('e', '').isdigit()]
    if t == 'R':
        u1, v1, u2, v2, w = map(float, l[1:6]); d.line([P(u1, v1), P(u2, v2)], fill=(60, 60, 66), width=int(w * k))
for l in L[1:]:
    t = l[0]
    if t == 'D':
        u1, v1, u2, v2 = map(float, l[1:5]); d.line([P(u1, v1), P(u2, v2)], fill=(80, 80, 90), width=int(7 * k))
    if t == 'P':
        u1, v1, u2, v2 = map(float, l[1:5]); b = int(l[5])
        d.line([P(u1, v1), P(u2, v2)], fill=(225, 225, 215) if b else (150, 150, 150), width=int(2.5 * k) if b else 1)
for l in L[1:]:
    if l[0] == 'B':
        u, v, w, h = map(float, l[1:5]); net = int(l[6]); c = col.get(l[7], (200, 90, 200)) if net < 0 else (240, 80, 60)
        d.rectangle([P(u - w / 2, v + h / 2), P(u + w / 2, v - h / 2)], fill=c, outline=(20, 20, 20))
    if l[0] == 'U':
        u1, v1, u2, v2 = map(float, l[1:5]); d.line([P(u1, v1), P(u2, v2)], fill=(250, 220, 60), width=1)
    if l[0] == 'E':
        u, v = map(float, l[1:3]); x, y = P(u, v); d.ellipse([x - 3, y - 3, x + 3, y + 3], fill=(40, 220, 80))
img.save(dst)
print('saved', dst)
