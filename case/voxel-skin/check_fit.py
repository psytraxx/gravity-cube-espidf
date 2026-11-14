#!/usr/bin/env python3
"""Fit and printability checks for the Voxel Skin. Run after changing generate.py.

Interference checks build solids with manifold3d and measure overlap volume;
any overlap above 0.001 mm3 fails. Wall checks cast rays through the STLs:

- outer walls (skin, bevels, mitre walls: every surface on the outside of the
  cube or on a mitre) must be at least MIN_WALL thick, measured to the nearly
  parallel face behind them;
- every feature must be at least MIN_PART thick, counting wedges up to 45
  degrees.

    python3 check_fit.py                        # 65 mm panels
    VOXEL_PROFILE=80x80 python3 check_fit.py    # 80 mm panels
"""

import itertools
import os
import sys

import numpy as np
import trimesh

import generate as g

TOL = 1e-3
failures = []


def check(name, vol, want_zero=True):
    ok = vol <= TOL if want_zero else vol > TOL
    print(f"{'ok  ' if ok else 'FAIL'} {name}: {vol:.4f}")
    if not ok:
        failures.append(name)


def panel(dx=0.0, dy=0.0, oversize=0.0, gdy=0.0, pieces=None):
    """Panel stack + LEDs + capacitors in face coordinates, shifted by dx, dy in its pocket;
    gdy moves the LED grid on the board (65 mm boards: up to 0.5 mm along the 66 mm side)."""
    hx, hy = g.HX + oversize / 2, g.HY + oversize / 2
    pcb = g.box(-hx + dx, hx + dx, -hy + dy, hy + dy, g.Z_BACK, g.Z_PCB)
    leds, caps = [], []
    for i in range(g.GRID_N):
        for j in range(g.GRID_N):
            cx = (i + 0.5) * g.PITCH - g.H + dx
            cy = (j + 0.5) * g.PITCH - g.H + dy + gdy
            leds.append(g.box(cx - 2.5, cx + 2.5, cy - 2.5, cy + 2.5, g.Z_PCB, g.Z_PCB + g.LED_H))
            caps.append(g.box(cx - 1.0, cx + 1.0, cy + 2.75, cy + 3.35, g.Z_PCB, g.Z_PCB + 1.25))
    if pieces is not None:
        pieces[:] = [pcb] + leds + caps
    return pcb, g.sum_all(leds), g.sum_all(caps)


def grown(m, d=0.4):
    out = m
    for v in ([d, 0, 0], [-d, 0, 0], [0, d, 0], [0, -d, 0], [0, 0, d], [0, 0, -d]):
        out = out + m.translate(v)
    return out


cut = g.port_cut()
box, lid = g.box_and_lid(cut)
parts = box + lid
s = g.FIT
goff = (g.PANEL_Y - g.GRID_SPAN) / 2


def placed_panel(name, dx=0.0, dy=0.0, dz=0.0, gdy=0.0):
    """One face's panel stack, LEDs and capacitors in cube coordinates, dz along the face normal."""
    return g.sum_all(list(panel(dx, dy, gdy=gdy))).translate([0, 0, dz]).rotate(g.FACES[name])


def sweep(name, dz, move):
    """Space a panel passes through when it moves by `move` (cube coordinates) from dz in from
    its face: one hull per board, LED and capacitor, as each of them is a box."""
    pieces = []
    panel(pieces=pieces)
    out = []
    for p in pieces:
        p = p.translate([0, 0, dz]).rotate(g.FACES[name])
        out.append(g.Manifold.batch_hull([p, p.translate(list(move))]))
    return g.sum_all(out)


print(f"== every panel in its pocket (centred, and pushed {g.FIT} mm into each corner)")
for name in g.FACES:
    worst = 0.0
    for gdy in sorted({0.0, goff, -goff}):
        # an off-centre LED grid is checked with the panel centred, as the 0.5 mm is a worst case already
        for dx, dy in [(0, 0), (s, s), (-s, s), (s, -s), (-s, -s)] if not gdy else [(0, 0)]:
            worst = max(worst, (parts ^ placed_panel(name, dx, dy, gdy=gdy)).volume())
    check(f"{name} panel, LEDs and capacitors vs box and lid", worst)
over = round(2 * g.FIT - 0.1, 2)
pcb, _, _ = panel(oversize=over)
check(f"panel {g.PANEL_X + over} x {g.PANEL_Y + over} mm vs box and lid",
      max((parts ^ pcb.rotate(r)).volume() for r in g.FACES.values()))
for name, r in g.FACES.items():
    contact = g.box(-g.HX, g.HX, -g.HY, g.HY, g.Z_PCB, g.Z_PCB + 0.05).rotate(r)
    check(f"{name} clamp ring rests on the panel margin (must be > 0)", (parts ^ contact).volume(), want_zero=False)

print("== assembled cube")
check("box vs lid", (box ^ lid).volume())
for d in ((0, 0), (g.FIT, 0), (-g.FIT, 0), (0, g.FIT), (0, -g.FIT)):
    bs = [placed_panel(n, *d) for n in g.FACES]
    w = max((bs[a] ^ bs[b]).volume() for a in range(6) for b in range(a + 1, 6))
    check(f"panels vs each other, all shifted {d}", w)
allboards = g.sum_all([placed_panel(n) for n in g.FACES])
usb, sw = g.usb_socket_model(), g.switch_model()
check("USB-C socket vs box, lid and panels", (parts ^ usb).volume() + (allboards ^ usb).volume())
check("switch vs box, lid and panels", (parts ^ sw).volume() + (allboards ^ sw).volume())

print("== fitting the panels through the open front")
# Each box panel goes in through the opening, held DEPTH in from its face so its LEDs pass
# under the lip of the front edge, slides down into line with its pocket and is pressed out
# into it. The back panel goes first and straight in. Checked against the box and every
# other panel already in place.
DEPTH = g.Z_PCB + g.LED_H - g.Z_LIP + 0.1
lid_dir = np.array([0, 0, 1.0])
for name in g.FACES:
    if name == g.LID:
        continue
    dz = 0.0 if name == "back" else -DEPTH
    path = sweep(name, dz, lid_dir * 2 * g.OUTER)
    if dz:
        normal = g._rot(g.FACES[name]) @ np.array([0, 0, 1.0])
        path = path + sweep(name, dz, normal * DEPTH)
    others = g.sum_all([placed_panel(n) for n in g.FACES if n not in (name, g.LID)])
    check(f"{name} panel: path in vs box and the other panels", (path ^ box).volume() + (path ^ others).volume())
print(f"     side panels go in {DEPTH:.2f} mm in from their faces, then out into their pockets")

print("== light cells")
for name, side in g.PORT_FACES.items():
    cells = g.sum_all(g.light_cells(side, g.joints_of(name))).rotate(g.FACES[name])
    check(f"{name} port pockets vs light cells, 0.4 mm margin", (grown(cut) ^ cells).volume())
holes, dowels, inside = [], [], 0.0
for name, r in g.FACES.items():
    j = g.joints_of(name)
    if not j:
        continue
    holes.append(g.pin_holes(j).rotate(r))
    cells = g.sum_all(g.light_cells(g.PORT_FACES.get(name, 0), j)) - g.pin_bosses(j)
    inside += (grown(g.pin_holes(j)) ^ cells).volume()
    if name == g.LID:
        for along, k in g._pin_axes(j):
            d = g.Manifold.cylinder(5.5, 1.0, 1.0, g.SEG, True)
            dowels.append(d.rotate([0, -45, 0]).translate([g.PIN_AT, along, g.PIN_AT]).rotate([0, 0, 90 * k]))
check("dowel holes inside their bosses (hole vs cells, 0.4 mm margin)", inside)
check(f"{len(dowels)} dowels of 5.5 mm vs box and lid", (g.sum_all(dowels) ^ parts).volume())
check("dowel holes vs port pockets", (g.sum_all(holes) ^ cut).volume())

print(f"== printability: outer walls >= {g.MIN_WALL} mm, every part >= {g.MIN_PART} mm")


def confirm(tm, o, dirs, d, dot, limit=1.4, step=0.05):
    """Re-cast the rays that found a thin spot from four points 0.05 mm around their origin and
    keep the median: a real thin wall stays thin, a ray that grazes an edge does not."""
    d = d.copy()
    idx = np.where((d < limit) & (dot < -0.7))[0]
    if not len(idx):
        return d
    t1 = np.cross(dirs[idx], [0.577, 0.577, 0.577])
    t1 /= np.linalg.norm(t1, axis=1, keepdims=True)
    t2 = np.cross(dirs[idx], t1)
    runs = [d[idx]]
    for t in (t1, -t1, t2, -t2):
        oo = o[idx] + t * step
        loc, ri, _ = tm.ray.intersects_location(oo, dirs[idx], multiple_hits=False)
        r = np.full(len(idx), np.inf)
        r[ri] = np.linalg.norm(loc - oo[ri], axis=1)
        runs.append(r)
    d[idx] = np.median(np.vstack(runs), axis=0)
    return d


def outer_surface(pts):
    """Points on the outside of the cube (faces, rounded edges and corners, the port pad) or on
    a mitre between box and lid."""
    a = np.abs(pts)
    on_face = np.isclose(a.max(1), g.OUTER, atol=1e-3)
    c = g.OUTER - g.EDGE_R
    on_round = np.isclose(np.linalg.norm(np.maximum(a - c, 0), axis=1), g.EDGE_R, atol=0.05) & \
        ((a > c).sum(1) >= 2)
    on_pad = np.isclose((a[:, 1] + a[:, 2]) / g.SQ2, g.FACET_D, atol=1e-3) & (pts[:, 1] < 0) & (pts[:, 2] > 0)
    on_bevel = on_round | on_pad
    on_mitre = (pts[:, 2] > 0) & np.isclose(np.maximum(a[:, 0], a[:, 1]), pts[:, 2], atol=1e-3)
    return on_face | on_bevel | on_mitre


def edge_wall(pts, nn):
    """Faces at 45 degrees between two axes: the walls of the edges, from inside and out.
    The dowel bosses are left out; they are parts, held to MIN_PART."""
    a = np.sort(np.abs(nn), axis=1)
    sel = np.isclose(a[:, 1], 0.7071, atol=0.01) & np.isclose(a[:, 2], 0.7071, atol=0.01)
    for name, r in g.FACES.items():
        m = g._rot(r)
        for along, k in g._pin_axes(g.joints_of(name)):
            q = g._rot([0, 0, 90 * k])
            p0 = m @ q @ np.array([g.PIN_AT, along, g.PIN_AT])
            d = m @ q @ np.array([1, 0, -1]) / g.SQ2
            v = pts - p0
            dist = np.linalg.norm(v - np.outer(v @ d, d), axis=1)
            sel &= ~((dist < g.BOSS_R + 0.3) & (np.abs(v @ d) < g.PIN_DEPTH + g.BOSS_WALL + 0.6))
    return sel


for fname in ("box.stl", "lid.stl", "fit_test.stl"):
    tm = trimesh.load(os.path.join(g.OUT_DIR, fname))
    np.random.seed(0)
    pts, fi = trimesh.sample.sample_surface_even(tm, 200000 if fname == "box.stl" else 120000)
    nn = tm.face_normals[fi]
    o = pts - nn * 1e-3
    loc, ri, ti = tm.ray.intersects_location(o, -nn, multiple_hits=False)
    d = np.linalg.norm(loc - o[ri], axis=1)
    dot = (tm.face_normals[ti] * nn[ri]).sum(1)
    d = confirm(tm, o[ri], -nn[ri], d, dot)
    part = d[dot < -0.7]
    thinnest = float(part.min())
    check(f"{fname} thinnest part {thinnest:.2f} mm (>= {g.MIN_PART})",
          g.MIN_PART - thinnest if thinnest < g.MIN_PART - 0.005 else 0.0)
    if fname != "fit_test.stl":
        sel = (outer_surface(pts[ri]) | edge_wall(pts[ri], nn[ri])) & (dot < -0.9)
        wall = float(d[sel].min())
        check(f"{fname} thinnest outer and edge wall {wall:.2f} mm (>= {g.MIN_WALL})",
              g.MIN_WALL - wall if wall < g.MIN_WALL - 0.005 else 0.0)

print("== wiring")
# slot between the front and bottom panel corners along the port edge, panels centred
gap = g.SQ2 * (g.Z_BACK - g.HY)
print(f"     slot between panel corners on the port edge: {gap:.2f} mm (USB-C leads {g.USB_LEAD_D} mm)")
check("slot passes the USB-C leads", 0.0 if gap >= g.USB_LEAD_D + 0.3 else 1.0)
corner = g.box(-20, 20, -g.Z_BACK, -g.HY, g.HY, g.Z_BACK)
check("port pockets reach the slot between the panels (must be > 0)", (cut ^ corner).volume(), want_zero=False)

print()
inner = (g.PITCH - g.BAFFLE - 5.0) / 2
print(f"LED to baffle: {inner:.2f} mm per side (LED grid centred); lit width {2 * g.W_SKIN:.1f} mm of {2 * g.OUTER:.1f} mm")
print(f"LED top to skin: {g.Z_SKIN - g.Z_PCB - g.LED_H:.1f} mm; outer cube {2 * g.OUTER:.1f} mm")
if failures:
    print("FAILED:", ", ".join(failures))
    sys.exit(1)
print("All checks passed.")
