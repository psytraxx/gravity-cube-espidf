#!/usr/bin/env python3
"""Voxel Skin enclosure for the Gravity Cube.

Six mitred face shells hold six flexible WS2812 8x8 panels. Each shell is one
translucent resin part built as a folded plate of constant thickness: the
outer skin, the edge bevels and the 45 degree mitre walls are one continuous
1.4 mm wall, and that wall carries the load. Inside it, 1.3 mm baffles split
the light into 8x8 funnel cells; the outermost cells run right out to the
mitre walls, so there is no solid material along the edges. Glued together,
the six shells form a closed box whose mitre walls lie face to face.

Two 8x8 panel profiles (VOXEL_PROFILE environment variable), same firmware:
  65x65  65 x 66 mm rigid panels, 8.125 mm pitch -> 65x65/  (default)
  80x80  80 x 80 mm flexible panels on a 1 mm backing sheet, 10 mm pitch -> 80x80/

Coordinates: mm, cube centred on the origin. A shell is modelled with its
face normal along +Z; the front (skin) face is at z = OUTER.

    pip install manifold3d trimesh numpy rtree
    python3 generate.py                        # 65 mm panels
    VOXEL_PROFILE=80x80 python3 generate.py    # 80 mm panels
"""

import json
import math
import os

import numpy as np
import trimesh
from manifold3d import Manifold

PROFILE = os.environ.get("VOXEL_PROFILE", "65x65")
if PROFILE not in ("65x65", "80x80"):
    raise SystemExit(f"unknown VOXEL_PROFILE {PROFILE!r}; use 65x65 or 80x80")
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), PROFILE)
SQ2 = math.sqrt(2)

# ---------------------------------------------------------------- panel
GRID_N = 8
PCB_T = 1.6  # panel stack thickness
if PROFILE == "65x65":
    # Rigid boards, 65 x 66 mm: the 65 mm side runs along X, the 66 mm side
    # along Y. The LED grid (8.125 mm pitch) is assumed centred; on the 66 mm
    # side it may sit up to 0.5 mm off-centre.
    PANEL_X, PANEL_Y = 65.0, 66.0
    GRID_SPAN = 65.0
    FIT = 0.25  # clearance around the panel in its pocket
else:
    # Flexible boards, 80 x 80 mm, 10 mm pitch, glued to an 80 x 80 x 1 mm
    # backing sheet: panel + glue + sheet make the same 1.6 mm stack.
    PANEL_X, PANEL_Y = 80.0, 80.0
    GRID_SPAN = 80.0
    FIT = 0.4  # an 80.7 mm backing sheet still fits
PITCH = GRID_SPAN / GRID_N
LED_H = 1.6  # WS2812 5050 package height
H = GRID_SPAN / 2  # half the LED grid
HX, HY = PANEL_X / 2, PANEL_Y / 2
HMAX = max(HX, HY)  # depths use the long side, so a plain shell's panel can face either way

# ---------------------------------------------------------------- walls
SKIN = 1.4  # outer skin: structural wall and diffuser (> 1.2 mm)
MITRE_T = 1.4  # mitre wall, measured square to the mitre (> 1.2 mm)
BEVEL_T = 1.4  # wall under the edge bevels (> 1.2 mm)
BAFFLE = 1.3  # light baffles, measured square to their faces (> 1.2 mm)
MIN_WALL = 1.3  # outer walls must be at least this (the rule is > 1.2)
MIN_PART = 0.8  # every other feature must be at least this

# ---------------------------------------------------------------- shell
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
OUTER = Z_SKIN + SKIN  # front face: 43.0 / 50.0 -> 86 / 100 mm cube
Z_LIP = max(Z_BACK, HMAX + FIT + LIP_MIN)  # back edge of the shell (bottom of the pocket lip)
MITRE_OFF = MITRE_T * SQ2  # inner face of the mitre wall: |x| = z - MITRE_OFF
BEVEL_S = 2.5  # bevel depth from the edge; meets each face BEVEL_S * sqrt(2) from the edge
FACET_D = OUTER * SQ2 - BEVEL_S  # bevel facet distance from the centre
W_SKIN = Z_SKIN - MITRE_OFF  # half the lit width at the skin (39.6 / 46.6)
BIG = OUTER + 20

# Along the port edge the outer row of cells in PORT_COLS is kept narrow so
# the solid material around the USB-C socket and the switch stays clear of it.
PORT_COLS = (1, 2, 3, 4, 5, 6) if PROFILE == "65x65" else (2, 3, 4, 5)  # the parts span ~37 mm
PORT_BOTTOM = H - PITCH / 2 + 2.5 + 0.8  # outer side of those cells beside the LEDs (0.8 clear of the LED body)
PORT_EDGE = OUTER - 10.0  # ... and at the skin (33 / 40)

# ---------------------------------------------------------------- pins
# 2 mm dowels across the mitre, two per edge. Each hole sits in a small boss at
# the root of the mitre wall, centred on a baffle and clear of the panel and LEDs.
PIN_D = 2.1
PIN_DEPTH = 3.0  # per side
PIN_ALONG = (H - PITCH,)  # on the baffle between the outer two LED columns
BOSS_WALL = 0.9
BOSS_R = PIN_D / 2 + BOSS_WALL
# pin axis meets the mitre at x = z = PIN_AT: as low as it can go with the boss clear of the panel corner
PIN_AT = round((HMAX + FIT + Z_PCB) / 2 + (BOSS_R + 0.15) / SQ2, 1)

# ---------------------------------------------------------------- USB-C + switch (cube coordinates; front = +Z, bottom = -Y)
# A snap-in USB-C socket and an SS12F15 slide switch sit on the bevel of the bottom front edge
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
USB_X = -10.5 if PROFILE == "65x65" else -9.2  # socket centre along the edge
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
SW_X = 8.7 if PROFILE == "65x65" else 10.0  # the 65 mm grid packs both parts closer to the middle
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


# The four edges of a face, as quarter turns about its normal: 0 = +X, 1 = +Y, 2 = -X, 3 = -Y.
ALL_EDGES = (0, 1, 2, 3)


def _pin_axes(joints=ALL_EDGES):
    """(along, quarter turn) for every dowel on the given edges of one face."""
    return [(s * a, k) for a in PIN_ALONG for s in (-1, 1) for k in joints]


def pin_holes(joints=ALL_EDGES):
    holes = []
    for along, k in _pin_axes(joints):
        h = Manifold.cylinder(PIN_DEPTH * 2, PIN_D / 2, PIN_D / 2, SEG, True)
        holes.append(h.rotate([0, -45, 0]).translate([PIN_AT, along, PIN_AT]).rotate([0, 0, 90 * k]))
    return sum_all(holes)


def pin_bosses(joints=ALL_EDGES):
    """Solid around each dowel hole on this shell's side of the mitre."""
    out = []
    for along, k in _pin_axes(joints):
        b = Manifold.cylinder(PIN_DEPTH + BOSS_WALL + 0.5, BOSS_R, BOSS_R, SEG).translate([0, 0, -0.5])
        out.append(b.rotate([0, -45, 0]).translate([PIN_AT, along, PIN_AT]).rotate([0, 0, 90 * k]))
    return sum_all(out)


# ---------------------------------------------------------------- light cells
# Baffle centre lines: square to the face beside the LEDs (on the LED cell
# boundaries), then fanning out to GRID_N equal cells across the lit width.
LED_LINES = [-H + k * PITCH for k in range(GRID_N + 1)]
SKIN_LINES = [-W_SKIN + k * 2 * W_SKIN / GRID_N for k in range(GRID_N + 1)]
RISE = Z_SKIN - Z_KINK
TAPER = 1.0  # height over which a baffle widens from BAFFLE to its sloped width
Z_TAPER = Z_KINK + TAPER


def _half(k):
    """Half the baffle width, measured along the face, for a BAFFLE thick sloped baffle."""
    return BAFFLE / 2 / math.cos(math.atan2(abs(SKIN_LINES[k] - LED_LINES[k]), RISE))


def _edge(z, joint=True):
    """Inner face of the mitre wall at height z. A joint between two parts has a full
    MITRE_T wall on each side; where two faces are printed as one, each face takes half
    and the edge is a single MITRE_T wall."""
    return z - (MITRE_OFF if joint else MITRE_OFF / 2)


def _baffle(k, z):
    """(left face, right face) of baffle k at height z, measured along the face.

    Beside the LEDs the baffle is BAFFLE wide and square to the face. Above
    Z_KINK it fans out towards its place on the skin; a sloped baffle has to
    be wider along the face to stay BAFFLE thick, and it widens over TAPER so
    no face overhangs in a thin ledge."""
    b, h = LED_LINES[k], _half(k)
    if z <= Z_KINK:
        return b - BAFFLE / 2, b + BAFFLE / 2
    c = b + (z - Z_KINK) / RISE * (SKIN_LINES[k] - b)
    if z >= Z_TAPER:
        return c - h, c + h
    t = (z - Z_KINK) / TAPER
    l0, r0 = _baffle(k, Z_KINK)
    l1, r1 = _baffle(k, Z_TAPER)
    return l0 + t * (l1 - l0), r0 + t * (r1 - r0)


def _bounds(i, z, port, joint_lo=True, joint_hi=True):
    """(lo, hi) of cell column i at height z. port = -1 / +1 narrows the outer
    cell on the -/+ side to PORT_BOTTOM beside the LEDs and PORT_EDGE at the skin.
    joint_lo / joint_hi: whether the -/+ edge is a joint (see _edge)."""
    def outer(sign):
        if port == sign:
            t = max(0.0, (z - Z_KINK) / RISE)
            return PORT_BOTTOM + t * (PORT_EDGE - PORT_BOTTOM)
        return _edge(z, joint_hi if sign > 0 else joint_lo)

    lo = -outer(-1) if i == 0 else _baffle(i, z)[1]
    hi = outer(1) if i == GRID_N - 1 else _baffle(i + 1, z)[0]
    return lo, hi


def _cell(i, j, port_side, joints=ALL_EDGES):
    """One cell: hulls between the levels where its sides change slope."""
    py = port_side if i in PORT_COLS else 0
    levels = (Z_GRID - 0.01, Z_KINK, Z_TAPER, Z_SKIN)
    parts = []
    for z0, z1 in zip(levels, levels[1:]):
        pts = []
        for z in (z0, z1):
            x0, x1 = _bounds(i, z, 0, 2 in joints, 0 in joints)
            y0, y1 = _bounds(j, z, py, 3 in joints, 1 in joints)
            pts += [[x, y, z] for x in (x0, x1) for y in (y0, y1)]
        parts.append(Manifold.hull_points(np.array(pts)))
    return sum_all(parts)


def light_cells(port_side=0, joints=ALL_EDGES):
    """The GRID_N x GRID_N cells of one face (as solids to subtract), kept BEVEL_T inside the bevels."""
    inner = chamfered_cube(BEVEL_T)
    return [_cell(i, j, port_side, joints) ^ inner for i in range(GRID_N) for j in range(GRID_N)]


def face_shell(port_side=0, pins=True, joints=ALL_EDGES):
    """One face in its own frame. port_side = -1 / +1 narrows the outer cells along the
    -Y / +Y edge in PORT_COLS, leaving solid material for the USB-C socket and switch.
    joints: the edges that meet another part (full mitre wall and dowels); the other
    edges are printed in one piece with the neighbouring face."""
    return (frustum() ^ chamfered_cube()) - face_cuts(port_side, pins, joints)


def face_cuts(port_side=0, pins=True, joints=ALL_EDGES):
    """Everything removed from a face, in its own frame: pocket, relief, cells, dowel holes."""
    ax, ay = HX + FIT, HY + FIT
    rx, ry = HX - CLAMP_RING, HY - CLAMP_RING
    cut = box(-ax, ax, -ay, ay, Z_LIP - 1, Z_PCB)  # panel pocket
    cut = cut + box(-rx, rx, -ry, ry, Z_PCB - 0.01, Z_GRID)  # relief over the LEDs and capacitors
    cells = sum_all(light_cells(port_side, joints))
    if pins and joints:
        cells = cells - pin_bosses(joints)
        cut = cut + pin_holes(joints)
    return cut + cells


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
    r0 = HY - CLAMP_RING - 0.01
    # (only under the narrow port cells: the full cells beside them keep their clamp ring)
    lim = LED_LINES[GRID_N - PORT_COLS[0]] - BAFFLE / 2
    for x, w in ((USB_X, pw), (SW_X, max(ww, 2 * bw))):
        x0, x1 = max(x - w / 2, -lim), min(x + w / 2, lim)
        cut = cut + box(x0, x1, -Z_GRID, -r0, r0, Z_GRID)
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


def profile_info():
    """Numbers the viewer needs to draw the panels and the lit pixels."""
    return {"profile": PROFILE, "n": GRID_N, "pcb_x": PANEL_X, "pcb_y": PANEL_Y, "pcb_t": PCB_T,
            "pitch": PITCH, "z_back": Z_BACK, "z_pcb": Z_PCB, "z_skin": Z_SKIN, "outer": OUTER,
            "skin_lines": SKIN_LINES, "baffle_half": [_half(k) for k in range(GRID_N + 1)],
            "port_cols": list(PORT_COLS), "port_edge": PORT_EDGE, "skin": SKIN, "baffle": BAFFLE}


FACES = {
    "front": [0, 0, 0],  # +Z: the lid
    "back": [180, 0, 0],  # -Z
    "top": [-90, 0, 0],  # +Y
    "bottom": [90, 0, 0],  # -Y
    "right": [0, 90, 0],  # +X
    "left": [0, -90, 0],  # -X
}
LID = "front"

# The port edge is the bottom front edge: -Y on the front face, +Y on the bottom face.
PORT_FACES = {"front": -1, "bottom": 1}


def _rot(rot):
    """3x3 matrix of Manifold.rotate(rot) for the single-axis rotations in FACES."""
    m = np.eye(3)
    for axis, deg in enumerate(rot):
        if deg:
            c, s_ = math.cos(math.radians(deg)), math.sin(math.radians(deg))
            r = np.eye(3)
            a, b = [(1, 2), (2, 0), (0, 1)][axis]
            r[a, a], r[a, b], r[b, a], r[b, b] = c, -s_, s_, c
            m = r @ m
    return m


EDGE_DIRS = {0: (1, 0, 0), 1: (0, 1, 0), 2: (-1, 0, 0), 3: (0, -1, 0)}


def joints_of(name):
    """Edges of a face that meet another part: the lid has four, the box faces next to it
    one each (towards the lid), the back none."""
    if name == LID:
        return ALL_EDGES
    lid_n = _rot(FACES[LID]) @ np.array([0, 0, 1.0])
    return tuple(k for k, d in EDGE_DIRS.items() if np.allclose(_rot(FACES[name]) @ np.array(d, float), lid_n))


def placed_face(name):
    """One face, with its port side and joints, in cube coordinates."""
    return face_shell(PORT_FACES.get(name, 0), joints=joints_of(name)).rotate(FACES[name])


def box_and_lid(cut=None):
    """The five faces printed as one open box, and the lid, both in cube coordinates.

    The box is cut from one solid (bevelled cube minus the interior and the lid's
    frustum), so the faces share their edges without seams."""
    cut = port_cut() if cut is None else cut
    lid_space = Manifold.hull_points(np.array([[sx * z, sy * z, z] for z in (Z_LIP - 0.01, OUTER + 1)
                                               for sx in (-1, 1) for sy in (-1, 1)])).rotate(FACES[LID])
    box_ = chamfered_cube() - box(-Z_LIP, Z_LIP, -Z_LIP, Z_LIP, -Z_LIP, Z_LIP) - lid_space
    for n in FACES:
        if n != LID:
            box_ = box_ - face_cuts(PORT_FACES.get(n, 0), joints=joints_of(n)).rotate(FACES[n])
    lid = placed_face(LID) - cut
    return box_ - cut, lid


def unplace(m, rot):
    return m.rotate([-a for a in rot])


def to_trimesh(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts))


def main():
    out = OUT_DIR
    os.makedirs(out, exist_ok=True)
    box_, lid = box_and_lid()
    parts = {"box.stl": box_, "lid.stl": unplace(lid, FACES[LID]), "fit_test.stl": fit_test()}
    vols = {}
    for fname, m in parts.items():
        t = to_trimesh(m)
        t.export(os.path.join(out, fname))
        vols[fname] = t.volume / 1000
        print(f"{fname:24s} watertight={t.is_watertight} volume={t.volume / 1000:.1f} cm3 "
              f"bbox={np.round(t.extents, 2)}")
    usb = usb_socket_parts()
    refs = {"part_usb_c_socket.stl": usb["housing"], "part_usb_c_metal.stl": usb["metal"],
            "part_usb_c_lead_black.stl": usb["lead_black"], "part_usb_c_lead_red.stl": usb["lead_red"],
            "part_slide_switch_ss12f15.stl": switch_model()}
    for fname, m in refs.items():
        to_trimesh(m).export(os.path.join(out, fname))
        print(f"{fname:24s} (reference only, placed in cube coordinates)")
    print(f"cube {2 * OUTER:.1f} mm, resin {vols['box.stl'] + vols['lid.stl']:.1f} cm3")
    info = profile_info()
    info["box_cm3"] = round(vols["box.stl"], 1)
    info["lid_cm3"] = round(vols["lid.stl"], 1)
    with open(os.path.join(out, "profile.json"), "w") as f:
        json.dump(info, f, indent=1)
        f.write("\n")


if __name__ == "__main__":
    main()
