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


def panel(dx=0.0, dy=0.0, oversize=0.0, gdy=0.0):
    """Panel stack + LEDs + capacitors in shell coordinates, shifted by dx, dy in its pocket;
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
    return pcb, g.sum_all(leds), g.sum_all(caps)


def grown(m, d=0.4):
    out = m
    for v in ([d, 0, 0], [-d, 0, 0], [0, d, 0], [0, -d, 0], [0, 0, d], [0, 0, -d]):
        out = out + m.translate(v)
    return out


shell = g.face_shell()
cut = g.port_cut()
shells = {n: (g.face_shell(g.PORT_FACES[n]).rotate(r) - cut) if n in g.PORT_FACES else shell.rotate(r)
          for n, r in g.FACES.items()}

print(f"== one shell and its own panel (centred, and pushed {g.FIT} mm into each corner)")
s = g.FIT
goff = (g.PANEL_Y - g.GRID_SPAN) / 2
port_shell = g.face_shell(-1) - cut
for gdy in sorted({0.0, goff, -goff}):
    # an off-centre LED grid is checked with the panel centred, as the 0.5 mm is a worst case already
    for dx, dy in [(0, 0), (s, s), (-s, s), (s, -s), (-s, -s)] if not gdy else [(0, 0)]:
        pcb, leds, caps = panel(dx, dy, gdy=gdy)
        for nm, sh in (("plain", shell), ("port", port_shell)):
            check(f"{nm} shell vs panel/LEDs/capacitors at ({dx:+.2f}, {dy:+.2f}), LED grid {gdy:+.1f}",
                  (sh ^ pcb).volume() + (sh ^ leds).volume() + (sh ^ caps).volume())
over = round(2 * g.FIT - 0.1, 2)
pcb, _, _ = panel(oversize=over)
check(f"panel {g.PANEL_X + over} x {g.PANEL_Y + over} mm vs shell", (shell ^ pcb).volume())
for dx in (0, g.FIT, -g.FIT):
    contact = g.box(-g.HX + dx, g.HX + dx, -g.HY, g.HY, g.Z_PCB, g.Z_PCB + 0.05)
    check(f"clamp ring rests on the panel margin, panel shifted {dx:+.1f} (must be > 0)",
          (shell ^ contact).volume(), want_zero=False)

print("== assembled cube")
# the four plain shells take their panel either way round (matters for 65 x 66 boards)
spins = [(0, 90)] * 4 if g.PANEL_X != g.PANEL_Y else [(0,)] * 4
for d in ((0, 0), (g.FIT, 0), (-g.FIT, 0), (0, g.FIT), (0, -g.FIT)):
    worst = 0.0
    # every orientation with centred panels; the shifted panels in the base orientation
    for combo in itertools.product(*spins) if d == (0, 0) else [(0, 0, 0, 0)]:
        turns = dict(zip(("back", "top", "right", "left"), combo))
        sh, bd = [], []
        for n, r in g.FACES.items():
            spin = turns.get(n, 0)
            base = g.face_shell(g.PORT_FACES[n]) if n in g.PORT_FACES else shell
            x = base.rotate([0, 0, spin]).rotate(r)
            sh.append(x - cut if n in g.PORT_FACES else x)
            bd.append(g.sum_all(list(panel(*d))).rotate([0, 0, spin]).rotate(r))
        for a in range(6):
            for b in range(a + 1, 6):
                worst = max(worst, (sh[a] ^ sh[b]).volume(), (bd[a] ^ bd[b]).volume())
        worst = max(worst, (g.sum_all(sh) ^ g.sum_all(bd)).volume())
    check(f"shells and panels, all panels shifted {d}", worst)
allshells = g.sum_all(list(shells.values()))
allboards = g.sum_all([g.sum_all(list(panel())).rotate(r) for r in g.FACES.values()])
usb, sw = g.usb_socket_model(), g.switch_model()
check("USB-C socket vs shells and panels", (allshells ^ usb).volume() + (allboards ^ usb).volume())
check("switch vs shells and panels", (allshells ^ sw).volume() + (allboards ^ sw).volume())

print("== light cells")
for name, side in g.PORT_FACES.items():
    cells = g.sum_all(g.light_cells(side)).rotate(g.FACES[name])
    check(f"{name} port pockets vs light cells, 0.4 mm margin", (grown(cut) ^ cells).volume())
dowels = []
for along, k in g._pin_axes():
    d = g.Manifold.cylinder(5.5, 1.0, 1.0, g.SEG, True)
    dowels.append(d.rotate([0, -45, 0]).translate([g.PIN_AT, along, g.PIN_AT]).rotate([0, 0, 90 * k]))
dowels = g.sum_all(dowels)
check("dowel holes inside their bosses (hole vs cells, 0.4 mm margin)",
      (grown(g.pin_holes()) ^ (g.sum_all(g.light_cells(0)) - g.pin_bosses())).volume())
check("5.5 mm dowels vs own shell", (dowels ^ shell).volume())
check("dowel holes vs port pockets", (g.pin_holes().rotate(g.FACES["front"]) ^ cut).volume()
      + (g.pin_holes().rotate(g.FACES["bottom"]) ^ cut).volume())

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


def outer_surface(pts, nn):
    """Points on the outside of the cube (faces, bevels) or on a mitre plane."""
    x, y, z = np.abs(pts).T
    on_face = np.isclose(z, g.OUTER, atol=1e-3)
    on_bevel = np.isclose((np.maximum(x, y) + z) / g.SQ2, g.FACET_D, atol=1e-3)
    on_mitre = np.isclose(np.maximum(x, y), z, atol=1e-3)
    return on_face | on_bevel | on_mitre


for fname in ("shell.stl", "shell_port_front.stl", "shell_port_bottom.stl", "fit_test.stl"):
    tm = trimesh.load(os.path.join(g.OUT_DIR, fname))
    np.random.seed(0)
    pts, fi = trimesh.sample.sample_surface_even(tm, 120000)
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
        sel = outer_surface(pts[ri], nn[ri]) & (dot < -0.9)
        wall = float(d[sel].min())
        check(f"{fname} thinnest outer wall {wall:.2f} mm (>= {g.MIN_WALL})",
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
