#!/usr/bin/env python3
"""Fit and printability checks for the Voxel Skin. Run after changing generate.py.

Interference checks build solids with manifold3d and measure overlap volume;
any overlap above 0.001 mm3 fails. Wall checks cast rays through the STLs:

- outer walls (skin, bevels, mitre walls: every surface on the outside of the
  cube or on a mitre) must be at least MIN_WALL thick, measured to the nearly
  parallel face behind them;
- every feature must be at least MIN_PART thick, counting wedges up to 45
  degrees.

    python3 check_fit.py
"""

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


def panel(dx=0.0, dy=0.0, oversize=0.0):
    """Panel stack + LEDs + capacitors in shell coordinates, shifted by dx, dy in its pocket."""
    h = g.HMAX + oversize / 2
    pcb = g.box(-h + dx, h + dx, -h + dy, h + dy, g.Z_BACK, g.Z_PCB)
    leds, caps = [], []
    for i in range(g.GRID_N):
        for j in range(g.GRID_N):
            cx = (i + 0.5) * g.PITCH - g.HMAX + dx
            cy = (j + 0.5) * g.PITCH - g.HMAX + dy
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

print("== one shell and its own panel (centred, and pushed 0.4 mm into each corner)")
s = g.FIT
for dx, dy in [(0, 0), (s, s), (-s, s), (s, -s), (-s, -s)]:
    pcb, leds, caps = panel(dx, dy)
    for nm, sh in (("plain", shell), ("port", g.face_shell(-1) - cut)):
        check(f"{nm} shell vs panel/LEDs/capacitors at ({dx:+.1f}, {dy:+.1f})",
              (sh ^ pcb).volume() + (sh ^ leds).volume() + (sh ^ caps).volume())
over = round(2 * g.FIT - 0.1, 2)
pcb, _, _ = panel(oversize=over)
check(f"backing sheet {g.PANEL + over} mm vs shell", (shell ^ pcb).volume())
for dx in (0, g.FIT, -g.FIT):
    contact = g.box(-g.HMAX + dx, g.HMAX + dx, -g.HMAX, g.HMAX, g.Z_PCB, g.Z_PCB + 0.05)
    check(f"clamp ring rests on the panel margin, panel shifted {dx:+.1f} (must be > 0)",
          (shell ^ contact).volume(), want_zero=False)

print("== assembled cube")
names = list(shells)
worst = 0.0
for a in range(6):
    for b in range(a + 1, 6):
        worst = max(worst, (shells[names[a]] ^ shells[names[b]]).volume())
check("shells vs each other", worst)
boards = []
for n, r in g.FACES.items():
    pcb, leds, caps = panel()
    boards.append((pcb + leds + caps).rotate(r))
allshells, allboards = g.sum_all(list(shells.values())), g.sum_all(boards)
check("all shells vs all panels", (allshells ^ allboards).volume())
worst = 0.0
for a in range(6):
    for b in range(a + 1, 6):
        worst = max(worst, (boards[a] ^ boards[b]).volume())
check("panels vs each other", worst)
# every panel pushed 0.4 mm towards every edge in turn: worst case for the corners
for d in ((g.FIT, 0), (-g.FIT, 0), (0, g.FIT), (0, -g.FIT)):
    bs = [g.sum_all(list(panel(*d))).rotate(r) for r in g.FACES.values()]
    w = max((bs[a] ^ bs[b]).volume() for a in range(6) for b in range(a + 1, 6))
    w = max(w, (allshells ^ g.sum_all(bs)).volume())
    check(f"panels shifted {d} vs each other and the shells", w)
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
gap = g.SQ2 * (g.Z_BACK - g.HMAX)
print(f"     slot between panel corners on the port edge: {gap:.2f} mm (USB-C leads {g.USB_LEAD_D} mm)")
check("slot passes the USB-C leads", 0.0 if gap >= g.USB_LEAD_D + 0.3 else 1.0)
corner = g.box(-20, 20, -g.Z_BACK, -g.HMAX, g.HMAX, g.Z_BACK)
check("port pockets reach the slot between the panels (must be > 0)", (cut ^ corner).volume(), want_zero=False)

print()
inner = (g.PITCH - g.BAFFLE - 5.0) / 2
print(f"LED to baffle: {inner:.2f} mm per side; lit width {2 * g.W_SKIN:.1f} mm of {2 * g.OUTER:.1f} mm")
print(f"LED top to skin: {g.Z_SKIN - g.Z_PCB - g.LED_H:.1f} mm; outer cube {2 * g.OUTER:.1f} mm")
if failures:
    print("FAILED:", ", ".join(failures))
    sys.exit(1)
print("All checks passed.")
