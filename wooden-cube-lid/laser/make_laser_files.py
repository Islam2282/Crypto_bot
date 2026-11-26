"""Детали для лазера: площадка под MG996R и качалка на круглую насадку.

Запуск: python3 make_laser_files.py   (нужен ezdxf: pip install ezdxf)
Каждый набор сохраняется в четырёх форматах, масштаб 1:1, миллиметры:
  .dxf  — DXF R12 (AutoCAD R12/LT2): RDWorks, LightBurn, CorelDRAW, AutoCAD
  .svg  — LightBurn, LaserGRBL, Inkscape, CorelDRAW
  .pdf  — CorelDRAW, Illustrator, LightBurn (векторный)
  .plt  — HPGL: RDWorks, LaserCut, режущие плоттеры
Линия реза — красная, 0,1 мм.
"""
import math
import ezdxf

# ---------------- геометрия (мм, ось Y вверх) ----------------
def circle_pts(cx, cy, r, n=96):
    return [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n + 1)]

def rect_pts(x, y, w, h):
    return [(x, y), (x + w, y), (x + w, y + h), (x, y + h), (x, y)]

def plate(ox, oy):
    """Площадка 60 × 75 под MG996R. Левый край — к боковой стенке. (ox, oy) — левый нижний угол."""
    W, H = 60.0, 75.0
    SX, SY = 36.5, 22.0                  # вал: от левого края и от ВЕРХНЕГО края
    BW, BH = 20.2, 41.2                  # окно под корпус 19,7 × 40,7 + зазор
    bcy = SY + 10.35                     # центр корпуса от верхнего края
    polys = [rect_pts(ox, oy, W, H),
             rect_pts(ox + SX - BW / 2, oy + H - (bcy + BH / 2), BW, BH)]
    circles = []
    for dx in (-5, 5):
        for dy in (-24.75, 24.75):       # ушки MG996R: 49,5 × 10 мм, Ø 3,2 под M3
            circles.append((ox + SX + dx, oy + H - (bcy + dy), 1.6))
    for y in (20.0, 58.0):               # Ø 4 под металлический уголок
        circles.append((ox + 8.0, oy + H - y, 2.0))
    return polys, circles

def arm(cx, cy):
    """Качалка: центр на валу (cx, cy), отверстие под тягу на 30 мм вправо."""
    R1, R2, D = 14.0, 6.0, 30.0
    g = math.acos((R1 - R2) / D)
    out = []
    for k in range(181):
        a = g + (2 * math.pi - 2 * g) * k / 180
        out.append((cx + R1 * math.cos(a), cy + R1 * math.sin(a)))
    for k in range(61):
        a = -g + 2 * g * k / 60
        out.append((cx + D + R2 * math.cos(a), cy + R2 * math.sin(a)))
    out.append(out[0])
    polys = [out]
    sw, r0, r1 = 2.2, 5.5, 11.0          # 4 прорези под винты насадки
    for ang in (45, 135, 225, 315):
        a = math.radians(ang); ux, uy = math.cos(a), math.sin(a); r = sw / 2
        c0 = (cx + r0 * ux, cy + r0 * uy); c1 = (cx + r1 * ux, cy + r1 * uy)
        pts = []
        for k in range(19):
            t = -math.pi / 2 + math.pi * k / 18
            pts.append((c1[0] + r * (ux * math.cos(t) - uy * math.sin(t)), c1[1] + r * (uy * math.cos(t) + ux * math.sin(t))))
        for k in range(19):
            t = math.pi / 2 + math.pi * k / 18
            pts.append((c0[0] + r * (ux * math.cos(t) - uy * math.sin(t)), c0[1] + r * (uy * math.cos(t) + ux * math.sin(t))))
        pts.append(pts[0])
        polys.append(pts)
    circles = [(cx, cy, 3.5), (cx + D, cy, 2.1)]   # Ø 7 к винту насадки, Ø 4,2 под M4 к тяге
    return polys, circles

# ---------------- запись ----------------
def write_all(name, sheet_w, sheet_h, parts):
    polys, circles = [], []
    for p, c in parts:
        polys += p; circles += c
    paths = polys + [circle_pts(*c) for c in circles]

    doc = ezdxf.new('R12')
    doc.header['$INSBASE'] = (0, 0, 0)
    msp = doc.modelspace()
    doc.layers.add('CUT', color=1)
    for p in polys:
        msp.add_polyline2d(p[:-1], close=True, dxfattribs={'layer': 'CUT'})
    for x, y, r in circles:
        msp.add_circle((x, y), r, dxfattribs={'layer': 'CUT'})
    doc.saveas(name + '.dxf')

    f = lambda v: ('%.3f' % v).rstrip('0').rstrip('.')
    svg = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<svg xmlns="http://www.w3.org/2000/svg" version="1.1" width="%smm" height="%smm" viewBox="0 0 %s %s">' % (f(sheet_w), f(sheet_h), f(sheet_w), f(sheet_h)),
           '<g fill="none" stroke="#ff0000" stroke-width="0.1">']
    for p in paths:
        svg.append('<path d="M' + ' L'.join('%s %s' % (f(x), f(sheet_h - y)) for x, y in p) + ' Z"/>')
    svg += ['</g>', '</svg>']
    open(name + '.svg', 'w', encoding='utf-8').write('\n'.join(svg) + '\n')

    k = 72 / 25.4
    body = ['1 0 0 RG', '%.4f w' % (0.1 * k)]
    for p in paths:
        body.append(' '.join(('%.3f %.3f ' % (x * k, y * k)) + ('m' if i == 0 else 'l') for i, (x, y) in enumerate(p)) + ' h S')
    stream = '\n'.join(body).encode('ascii')
    objs = [b'<< /Type /Catalog /Pages 2 0 R >>',
            b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
            ('<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.3f %.3f] /Contents 4 0 R >>' % (sheet_w * k, sheet_h * k)).encode('ascii'),
            b'<< /Length ' + str(len(stream)).encode('ascii') + b' >>\nstream\n' + stream + b'\nendstream']
    out = b'%PDF-1.4\n'; offs = []
    for i, o in enumerate(objs, 1):
        offs.append(len(out)); out += ('%d 0 obj\n' % i).encode('ascii') + o + b'\nendobj\n'
    xref = len(out)
    out += ('xref\n0 %d\n0000000000 65535 f \n' % (len(objs) + 1)).encode('ascii')
    for o in offs:
        out += ('%010d 00000 n \n' % o).encode('ascii')
    out += ('trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, xref)).encode('ascii')
    open(name + '.pdf', 'wb').write(out)

    u = lambda v: str(int(round(v * 40)))  # HPGL: 40 единиц на мм
    plt = ['IN;', 'SP1;']
    for p in paths:
        plt.append('PU%s,%s;' % (u(p[0][0]), u(p[0][1])))
        plt.append('PD' + ','.join('%s,%s' % (u(x), u(y)) for x, y in p[1:]) + ';')
    plt += ['PU;', 'SP0;']
    open(name + '.plt', 'w').write('\n'.join(plt) + '\n')

if __name__ == '__main__':
    write_all('servo_plate_MG996R', 135, 85, [plate(5, 5), plate(70, 5)])
    write_all('servo_arm_MG996R', 58, 70, [arm(17, 18), arm(17, 52)])
    write_all('laser_all_parts', 198, 85, [plate(5, 5), plate(70, 5), arm(157, 22), arm(157, 60)])
    print('ok')
