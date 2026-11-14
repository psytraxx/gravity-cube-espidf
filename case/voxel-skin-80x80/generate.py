#!/usr/bin/env python3
"""Voxel Skin enclosure for the Gravity Cube (flexible 80 x 80 mm panels).

Six mitred face shells hold six flexible WS2812 8x8 panels. Each shell is one
translucent resin part built as a folded plate of constant thickness: the
outer skin, the edge bevels and the 45 degree mitre walls are one continuous
1.4 mm wall, and that wall carries the load. Inside it, 0.8 mm baffles split
the light into 8x8 funnel cells; the outermost cells run right out to the
mitre walls, so there is no solid material along the edges. Glued together,
the six shells form a closed box whose mitre walls lie face to face.

Coordinates: mm, cube centred on the origin. A shell is modelled with its
face normal along +Z; the front (skin) face is at z = OUTER.

    pip install manifold3d trimesh numpy rtree
    python3 generate.py
"""

import math
import os

import numpy as np
import trimesh
from manifold3d import Manifold

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
SQ2 = math.sqrt(2)

# ---------------------------------------------------------------- panel
# Flexible 80 x 80 mm boards, 10 mm pitch (outer LEDs 2.5 mm from the edge),
# glued to an 80 x 80 x 1 mm backing sheet: a 1.6 mm stack.
GRID_N = 8
PANEL = 80.0
PCB_T = 1.6
PITCH = PANEL / GRID_N
LED_H = 1.6  # WS2812 5050 package height
HMAX = PANEL / 2

# ---------------------------------------------------------------- walls
SKIN = 1.4  # outer skin: structural wall and diffuser (> 1.2 mm)
MITRE_T = 1.4  # mitre wall, measured square to the mitre (> 1.2 mm)
BEVEL_T = 1.4  # wall under the edge bevels (> 1.2 mm)
BAFFLE = 0.85  # light baffles, measured square to their faces (thinnest part >= 0.8 mm)
MIN_WALL = 1.3  # outer walls must be at least this (the rule is > 1.2)
MIN_PART = 0.8  # every other feature must be at least this

# ---------------------------------------------------------------- shell
FIT = 0.4  # clearance around the panel in its pocket
# Panel back to the neighbour's panel edge. The USB-C leads pass between the
# panel corners along the port edge: with centred panels that slot is
# sqrt(2) * PANEL_GAP = 1.7 mm wide.
PANEL_GAP = 1.2
RELIEF = 1.3  # baffles stop this far above the PCB (clears the capacitors)
GRID_DEPTH = 5.8  # PCB front to the back of the skin
CLAMP_RING = 0.6  # ring that rests on the panel margin
LIP_MIN = 0.85  # the pocket lip is cut off where it would be thinner than this

Z_BACK = HMAX + PANEL_GAP  # back of the panel stack
Z_PCB = Z_BACK + PCB_T  # front of the panel (LED side)
Z_GRID = Z_PCB + RELIEF  # bottom of the baffles
Z_KINK = max(Z_PCB + LED_H + 0.2, Z_GRID + MIN_PART + 0.05)  # baffles stay square to the face up to here, clear of the LEDs
Z_SKIN = Z_PCB + GRID_DEPTH  # back of the skin
OUTER = Z_SKIN + SKIN  # front face: 50.0 -> 100 mm cube
Z_LIP = max(Z_BACK, HMAX + FIT + LIP_MIN)  # back edge of the shell (bottom of the pocket lip)
MITRE_OFF = MITRE_T * SQ2  # inner face of the mitre wall: |x| = z - MITRE_OFF
BEVEL_S = 2.5  # bevel depth from the edge; meets each face BEVEL_S * sqrt(2) from the edge
FACET_D = OUTER * SQ2 - BEVEL_S  # bevel facet distance from the centre
W_SKIN = Z_SKIN - MITRE_OFF  # half the lit width at the skin (46.6)
BIG = OUTER + 20

# Along the port edge the outer row of cells in PORT_COLS is kept narrow so
# the solid material around the USB-C socket and the switch stays clear of it.
PORT_COLS = (2, 3, 4, 5)
PORT_BOTTOM = HMAX - 1.7  # outer side of those cells beside the LEDs (0.8 clear of the LED body)
PORT_EDGE = OUTER - 10.0  # ... and at the skin (40)

# ---------------------------------------------------------------- pins
# 2 mm dowels across the mitre, two per edge. Each hole sits in a small boss at
# the root of the mitre wall, centred on a baffle and clear of the panel and LEDs.
PIN_D = 2.1
PIN_DEPTH = 3.0  # per side
PIN_ALONG = (30.0,)  # on the baffle between the outer two LED columns
BOSS_WALL = 0.9
BOSS_R = PIN_D / 2 + BOSS_WALL
PIN_AT = 43.2  # pin axis meets the mitre at x = z = PIN_AT

# ---------------------------------------------------------------- USB-C + switch (cube coordinates; front = +Z, bottom = -Y)
# Same parts as the Voxel Shell. They sit on the bevel of the bottom front edge
# and point diagonally into the wedge between the front and bottom panels.
# Part geometry is written as if the facet were a front face at z = OUTER with
# its centre line at y = 0; bevel_place() turns it onto the facet.
USB_FLANGE = (15.8, 8.2, 2.0)
USB_BODY = (12.2, 5.2)
USB_BODY_DEPTH = 9.0
USB_WINGS = 15.6
USB_CUTOUT = (14.2, 5.5)  # supplier's panel cut-out 14.2 x 5.4, 0.1 taller for resin
USB_WALL = 1.7  # wall the snap wings grip
USB_NECK = 0.5  # pocket stays body-tight this far behind the wall, so it opens out 1.35 under the faces
USB_POCKET_H = 8.6  # pocket height behind the neck
USB_TAIL_H = 6.0  # pocket height over the last part of the body, clear of the light cells
USB_X = -9.2
SW_PLATE = (19.6, 5.5)
SW_BODY = (11.5, 5.5)
SW_BODY_DEPTH = 5.0
SW_DEPTH = 7.8
SW_SOLDER = 2.5
SW_PLATE_T = 0.6
SW_HOLE_PITCH = 15.0
SW_LEVER = (3.3, 2.0)
SW_SLOT = (7.0, 2.4)  # 3 mm travel + 3.3 lever; 2.4 tall leaves 1.3 to each face along the facet
SW_WALL = 2.4  # keeps the plate pocket corners >= 1.3 under the faces
SW_X = 10.0
PART_FIT = 0.2
WIRE_SLOT = (8.0, 4.0)  # wire channel from each pocket into the cube, along the diagonal
WIRE_LEN = 6.0

SEG = 48


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def sum_all(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out + p
    return out


def rounded_slot(w, h, depth):
    """Stadium-shaped prism, centred in XY, extruded along +Z from 0 to depth."""
    r = h / 2
    c = Manifold.cylinder(depth, r, r, SEG)
    return Manifold.batch_hull([c.translate([-(w / 2 - r), 0, 0]), c.translate([w / 2 - r, 0, 0])])


def frustum(z0=None):
    """Mitred slab: |x| <= z and |y| <= z for z0 <= z <= OUTER."""
    z0 = Z_LIP if z0 is None else z0
    pts = [[sx * z, sy * z, z] for z in (z0, OUTER) for sx in (-1, 1) for sy in (-1, 1)]
    return Manifold.hull_points(np.array(pts))


def _rotation_to(n):
    """3x3 rotation taking +Z onto the unit vector n."""
    z = np.array([0.0, 0.0, 1.0])
    n = np.asarray(n, float) / np.linalg.norm(n)
    v = np.cross(z, n)
    c = float(z @ n)
    if np.linalg.norm(v) < 1e-12:
        return np.eye(3) if c > 0 else np.diag([1.0, -1.0, -1.0])
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx / (1 + c)


def chamfered_cube(inset=0.0):
    """The outer cube with all twelve edges bevelled at 45 degrees, shrunk by inset."""
    o, k = OUTER - inset, FACET_D - inset
    c = box(-o, o, -o, o, -o, o)
    for a, b in ((0, 1), (0, 2), (1, 2)):
        for sa in (-1, 1):
            for sb in (-1, 1):
                n = np.zeros(3)
                n[a], n[b] = sa, sb
                n /= np.linalg.norm(n)
                m = np.hstack([_rotation_to(n), (n * k).reshape(3, 1)])
                c = c - box(-200, 200, -200, 200, 0, 200).transform(m)
    return c


def _pin_axes():
    """(along, quarter turn) for every dowel of one shell."""
    return [(s * a, k) for a in PIN_ALONG for s in (-1, 1) for k in range(4)]


def pin_holes():
    holes = []
    for along, k in _pin_axes():
        h = Manifold.cylinder(PIN_DEPTH * 2, PIN_D / 2, PIN_D / 2, SEG, True)
        holes.append(h.rotate([0, -45, 0]).translate([PIN_AT, along, PIN_AT]).rotate([0, 0, 90 * k]))
    return sum_all(holes)


def pin_bosses():
    """Solid around each dowel hole on this shell's side of the mitre."""
    out = []
    for along, k in _pin_axes():
        b = Manifold.cylinder(PIN_DEPTH + BOSS_WALL + 0.5, BOSS_R, BOSS_R, SEG).translate([0, 0, -0.5])
        out.append(b.rotate([0, -45, 0]).translate([PIN_AT, along, PIN_AT]).rotate([0, 0, 90 * k]))
    return sum_all(out)


# ---------------------------------------------------------------- light cells
# Baffle centre lines: square to the face beside the LEDs (on the LED cell
# boundaries), then fanning out to GRID_N equal cells across the lit width.
LED_LINES = [-HMAX + k * PITCH for k in range(GRID_N + 1)]
SKIN_LINES = [-W_SKIN + k * 2 * W_SKIN / GRID_N for k in range(GRID_N + 1)]
RISE = Z_SKIN - Z_KINK


def _half(k):
    """Half the baffle width, measured along the face, for a BAFFLE thick sloped baffle."""
    return BAFFLE / 2 / math.cos(math.atan2(abs(SKIN_LINES[k] - LED_LINES[k]), RISE))


def _edge(z):
    """Inner face of the mitre wall at height z."""
    return z - MITRE_OFF


def _bounds(i, z, part, port):
    """(lo, hi) of cell column i at height z. part: 'straight' or 'funnel'.
    port = -1 / +1 narrows the outer cell on the -/+ side (PORT_EDGE)."""
    def outer(sign):
        if port == sign:
            return PORT_EDGE if z >= Z_SKIN - 1e-9 else PORT_BOTTOM
        return _edge(z)

    def line(k):
        if part == "straight":  # as wide as the sloped part above, so the faces run on without a step
            return LED_LINES[k], _half(k)
        t = max(0.0, (z - Z_KINK) / RISE)
        return LED_LINES[k] + t * (SKIN_LINES[k] - LED_LINES[k]), _half(k)

    if i == 0:
        lo = -outer(-1)
    else:
        c, h = line(i)
        lo = c + h
    if i == GRID_N - 1:
        hi = outer(1)
    else:
        c, h = line(i + 1)
        hi = c - h
    return lo, hi


def _cell(i, j, port_side):
    py = port_side if i in PORT_COLS else 0
    parts = []
    for part, (z0, z1) in (("straight", (Z_GRID - 0.01, Z_KINK)), ("funnel", (Z_KINK - 0.01, Z_SKIN))):
        pts = []
        for z in (z0, z1):
            x0, x1 = _bounds(i, z, part, 0)
            y0, y1 = _bounds(j, z, part, py)
            pts += [[x, y, z] for x in (x0, x1) for y in (y0, y1)]
        parts.append(Manifold.hull_points(np.array(pts)))
    return parts[0] + parts[1]


def light_cells(port_side=0):
    """The GRID_N x GRID_N cells of one face (as solids to subtract), kept BEVEL_T inside the bevels."""
    inner = chamfered_cube(BEVEL_T)
    return [_cell(i, j, port_side) ^ inner for i in range(GRID_N) for j in range(GRID_N)]


def face_shell(port_side=0, pins=True):
    """One face shell. port_side = -1 / +1 narrows the outer cells along the
    -Y / +Y edge in PORT_COLS, leaving solid material for the USB-C socket and switch."""
    s = frustum() ^ chamfered_cube()
    a = HMAX + FIT
    s = s - box(-a, a, -a, a, Z_LIP - 1, Z_PCB)  # panel pocket
    r = HMAX - CLAMP_RING
    s = s - box(-r, r, -r, r, Z_PCB - 0.01, Z_GRID)  # relief over the LEDs and capacitors
    cells = sum_all(light_cells(port_side))
    if pins:
        cells = cells - pin_bosses()
    s = s - cells
    if pins:
        s = s - pin_holes()
    return s


def bevel_place(m):
    """Move geometry from the facet frame (surface at z = OUTER, centre line y = 0) onto the bevel."""
    c = OUTER - BEVEL_S / SQ2
    return m.translate([0, 0, -OUTER]).rotate([45, 0, 0]).translate([0, -c, c])


def port_cut():
    """USB-C socket and slide switch cavities in cube coordinates (on the bottom front bevel)."""
    f = PART_FIT
    cw, ch = USB_CUTOUT
    cut = box(USB_X - cw / 2, USB_X + cw / 2, -ch / 2, ch / 2, OUTER - USB_WALL - 0.01, OUTER + 1)
    pw = USB_FLANGE[0]  # pocket as wide as the flange: room for the snap wings, every opening covered
    cut = cut + box(USB_X - pw / 2, USB_X + pw / 2, -ch / 2, ch / 2, OUTER - USB_WALL - USB_NECK - 0.01, OUTER - USB_WALL)
    # The cut-out is taller than the 5 mm facet: flare its mouth square to the faces (under the
    # flange) so it does not meet them in a knife edge
    flare = USB_WALL + 1
    cut = cut + Manifold.hull_points(np.array([[USB_X + sx * cw / 2, sy * h, z]
                                               for sx in (-1, 1) for sy in (-1, 1)
                                               for h, z in ((ch / 2, OUTER - USB_WALL), (ch / 2 + flare, OUTER + 1))]))
    usb_back = OUTER - USB_WALL - USB_BODY_DEPTH + USB_FLANGE[2] - 0.5
    # behind the neck the pocket opens at 45 degrees, parallel to the faces, to its full height,
    # then narrows again over the tail of the body where the light cells come close
    ph, th = USB_POCKET_H, USB_TAIL_H
    h0 = ch / 2
    z0 = OUTER - USB_WALL - USB_NECK
    z1 = z0 - (ph / 2 - h0)
    taper = Manifold.hull_points(np.array([[USB_X + sx * pw / 2, sy * h, z]
                                           for sx in (-1, 1) for sy in (-1, 1)
                                           for h, z in ((h0, z0 + 0.01), (ph / 2, z1))]))
    z2 = usb_back + 2.0
    narrow = Manifold.hull_points(np.array([[USB_X + sx * pw / 2, sy * h, z]
                                            for sx in (-1, 1) for sy in (-1, 1)
                                            for h, z in ((ph / 2, z2 + (ph - th) / 2), (th / 2, z2))]))
    cut = cut + taper + box(USB_X - pw / 2, USB_X + pw / 2, -ph / 2, ph / 2, z2 + (ph - th) / 2, z1 + 0.01)
    cut = cut + narrow + box(USB_X - pw / 2, USB_X + pw / 2, -th / 2, th / 2, usb_back, z2 + 0.01)
    # switch: plate against the inside of a thin wall, lever through a slot
    sw, sh = SW_PLATE[0] + 2 * f, SW_PLATE[1] + 2 * f
    plate = OUTER - SW_WALL
    sw_back = plate - SW_DEPTH - SW_SOLDER
    cut = cut + box(SW_X - sw / 2, SW_X + sw / 2, -sh / 2, sh / 2, plate - SW_PLATE_T - 0.2, plate)
    bw, bh = SW_BODY[0] / 2 + f, SW_BODY[1] / 2 + f
    cut = cut + box(SW_X - bw, SW_X + bw, -bh, bh, sw_back, plate)
    cut = cut + box(SW_X - SW_SLOT[0] / 2, SW_X + SW_SLOT[0] / 2,
                    -SW_SLOT[1] / 2, SW_SLOT[1] / 2, plate - 1, OUTER + 1)
    ww, wh = WIRE_SLOT
    wires = ((USB_X, usb_back, ww), (SW_X, sw_back, max(ww, 2 * bw)))
    for x, zb, w in wires:
        cut = cut + box(x - w / 2, x + w / 2, -wh / 2, wh / 2, zb - WIRE_LEN, zb + 0.01)
    cut = bevel_place(cut)
    # where the pockets and wire channels cross the pocket lips and clamp rings of both shells,
    # clear the whole corner up to the bottom of the baffles: otherwise thin fins stay standing
    r0 = HMAX - CLAMP_RING - 0.01
    for x, w in ((USB_X, pw), (SW_X, max(ww, 2 * bw))):
        cut = cut + box(x - w / 2, x + w / 2, -Z_GRID, -r0, r0, Z_GRID)
    return cut


def rounded_rect(w, h, r, depth):
    """Rounded rectangle prism, centred in XY, extruded along +Z from 0 to depth."""
    c = Manifold.cylinder(depth, r, r, SEG)
    return Manifold.batch_hull([c.translate([sx * (w / 2 - r), sy * (h / 2 - r), 0]) for sx in (-1, 1) for sy in (-1, 1)])


USB_C_PORT = (8.94, 3.26)  # USB-C receptacle opening (standard)
USB_LEAD_D = 1.2  # lead outer diameter
USB_LEAD_LEN = 5.5  # lead stubs drawn behind the body (they continue into the cube)


def usb_socket_parts():
    """Reference model of the snap-in USB-C socket, as separate parts for colouring.

    Facet frame first (front face of the flange at OUTER + flange thickness),
    then moved onto the bevel. Sizes from the supplier drawing: flange
    15.8 x 8.2 x 2.0, body 12.2 x 5.2, 15.6 across the snap wings.
    """
    fw, fh, ft = USB_FLANGE
    front = OUTER + ft
    back = front - USB_BODY_DEPTH - ft
    bw, bh = USB_BODY
    # clear housing: rounded flange + rounded body
    housing = rounded_rect(fw, fh, 2.6, ft).translate([USB_X, 0, OUTER])
    housing = housing + rounded_rect(bw, bh, 1.4, OUTER - back).translate([USB_X, 0, back])
    # snap wings: thin ramps on both sides, widest just behind the 1.6 mm wall
    wing_z0, wing_z1 = OUTER - USB_WALL - 0.2, OUTER - USB_WALL - 3.6
    for sx in (-1, 1):
        x_in = USB_X + sx * bw / 2
        x_out = USB_X + sx * USB_WINGS / 2
        pts = []
        for y in (-1.8, 1.8):
            pts += [[x_in, y, wing_z0], [x_out, y, wing_z0], [x_in, y, wing_z1]]
        housing = housing + Manifold.hull_points(np.array(pts))
    pw, ph = USB_C_PORT
    mouth = rounded_slot(pw, ph, 7.0).translate([USB_X, 0, front - 7.0 + 0.01])
    housing = housing - mouth
    # metal receptacle shell and the tongue inside it
    shell = rounded_slot(pw, ph, 6.6) - rounded_slot(pw - 0.5, ph - 0.5, 6.7).translate([0, 0, -0.05])
    tongue = box(-3.3, 3.3, -0.35, 0.35, 0, 5.2)
    metal = (shell + tongue).translate([USB_X, 0, front - 6.6 - 0.05])
    # leads: black and red, side by side, out of the back of the body
    leads = {}
    for colour, dx in (("black", -1.5), ("red", 1.5)):
        r = USB_LEAD_D / 2
        leads[colour] = Manifold.cylinder(USB_LEAD_LEN + 0.5, r, r, SEG).translate([USB_X + dx, 0, back - USB_LEAD_LEN])
    parts = {"housing": housing, "metal": metal, "lead_black": leads["black"], "lead_red": leads["red"]}
    return {k: bevel_place(v) for k, v in parts.items()}


def usb_socket_model():
    """The whole USB-C socket as one solid, placed in cube coordinates (for fit checks)."""
    return sum_all(list(usb_socket_parts().values()))


def switch_model():
    """Reference model of the SS12F15 slide switch, placed in cube coordinates."""
    plate_z = OUTER - SW_WALL
    pw, ph = SW_PLATE
    bl, bw = SW_BODY
    # plate: 5.5 wide in the middle, 3.2 wide ears with the screw holes
    plate = box(SW_X - pw / 2, SW_X + pw / 2, -1.6, 1.6, plate_z - 0.5, plate_z)
    plate = plate + box(SW_X - bl / 2, SW_X + bl / 2, -ph / 2, ph / 2, plate_z - 0.5, plate_z)
    for dx in (-SW_HOLE_PITCH / 2, SW_HOLE_PITCH / 2):
        plate = plate - Manifold.cylinder(2, 1.0, 1.0, SEG).translate([SW_X + dx, 0, plate_z - 1])
    body = box(SW_X - bl / 2, SW_X + bl / 2, -bw / 2, bw / 2, plate_z - SW_BODY_DEPTH, plate_z - 0.5)
    pins = sum_all([box(SW_X + dx - 0.75, SW_X + dx + 0.75, -0.25, 0.25, plate_z - SW_DEPTH, plate_z - SW_BODY_DEPTH)
                    for dx in (-3, 0, 3)])
    lw, lt = SW_LEVER
    lever = box(SW_X - 1.5 - lw / 2, SW_X - 1.5 + lw / 2, -lt / 2, lt / 2, plate_z, OUTER + 2.5)
    return bevel_place(plate + body + pins + lever)


# the fit-test piece keeps the grid walls up to Z_KINK, where they are still vertical



def fit_test():
    """Thin slice of a shell: pocket lip, clamp ring and the bottom of the baffles, no skin.

    Cheap to print. Press a real panel on its backing sheet in: if it seats flat
    and no LED touches a baffle, the full shells fit.
    """
    # square outer sides at the lip, so the slice has no 45 degree knife edges
    a = HMAX + FIT + LIP_MIN
    return face_shell(pins=False) ^ box(-a, a, -a, a, Z_LIP - 1, Z_KINK)


FACES = {
    "front": [0, 0, 0],  # +Z
    "back": [180, 0, 0],  # -Z
    "top": [-90, 0, 0],  # +Y
    "bottom": [90, 0, 0],  # -Y
    "right": [0, 90, 0],  # +X
    "left": [0, -90, 0],  # -X
}

# The port edge is the bottom front edge: -Y on the front shell, +Y on the bottom shell.
PORT_FACES = {"front": -1, "bottom": 1}


def unplace(m, rot):
    return m.rotate([-a for a in rot])


def to_trimesh(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts))


def main():
    out = OUT_DIR
    shell = face_shell()
    cut = port_cut()
    parts = {"shell.stl": shell, "fit_test.stl": fit_test()}
    placed = {}
    for name, rot in FACES.items():
        if name in PORT_FACES:
            p = face_shell(PORT_FACES[name]).rotate(rot) - cut
            parts[f"shell_port_{name}.stl"] = unplace(p, rot)
        else:
            p = shell.rotate(rot)
        placed[name] = p
    total = 0.0
    for fname, m in parts.items():
        t = to_trimesh(m)
        t.export(os.path.join(out, fname))
        print(f"{fname:24s} watertight={t.is_watertight} volume={t.volume / 1000:.1f} cm3 "
              f"bbox={np.round(t.extents, 2)}")
    usb = usb_socket_parts()
    refs = {"part_usb_c_socket.stl": usb["housing"], "part_usb_c_metal.stl": usb["metal"],
            "part_usb_c_lead_black.stl": usb["lead_black"], "part_usb_c_lead_red.stl": usb["lead_red"],
            "part_slide_switch_ss12f15.stl": switch_model()}
    for fname, m in refs.items():
        to_trimesh(m).export(os.path.join(out, fname))
        print(f"{fname:24s} (reference only, placed in cube coordinates)")
    cube = sum_all(list(placed.values()))
    t = to_trimesh(cube)
    t.export(os.path.join(out, "assembly_preview.stl"))
    print(f"assembly_preview.stl     edge={t.extents[0]:.1f} mm  volume={t.volume / 1000:.1f} cm3")


if __name__ == "__main__":
    main()
