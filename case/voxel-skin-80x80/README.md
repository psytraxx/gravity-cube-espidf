# Voxel Skin 80×80 enclosure

A lighter, smaller enclosure for the flexible **80 × 80 mm WS2812 8×8 panels**, built from the same parts as the [Voxel Shell 80×80](../voxel-shell-80x80/README.md).

Each of the six mitred shells is a folded plate of constant thickness. The outer skin, the 45° edge bevels and the mitre walls are one continuous 1.4 mm wall that carries the load. Glued together, the shells form a closed box whose mitre walls lie face to face along every edge. Inside the skin, 0.85 mm baffles split the light into 8×8 funnel cells. The outermost cells run out to the mitre walls, so no solid material runs along the edges.

| | |
|---|---|
| Outer cube | 100.0 mm |
| Resin per shell | 24.3 cm³ (146 cm³ for the cube) |
| Skin, bevel and mitre walls | 1.4 mm |
| Light baffles | 0.85 mm, square to their faces |
| Thinnest feature | 0.84 mm (`check_fit.py`) |
| Lit area | 93.2 × 93.2 mm per face |
| LED top to skin | 4.2 mm |
| Panel back to the neighbouring panel edge | 1.2 mm; the USB-C leads pass between the panel corners on the port edge (1.7 mm slot) |

**Material:** translucent resin (SLA) for all six shells.

## Files

| File | Qty | Purpose |
|---|---|---|
| `shell.stl` | 4 | Plain face shell (top, back, left, right) |
| `shell_port_front.stl` | 1 | Front shell with the USB-C cut-out and switch slot |
| `shell_port_bottom.stl` | 1 | Bottom shell with the matching relief |
| `fit_test.stl` | optional | Pocket lip, clamp ring and the bottom of the baffles, to test-fit a panel |
| `assembly_preview.stl` | – | Whole cube, for viewing only |
| `part_*.stl` | – | USB-C socket and switch reference models, in cube coordinates |
| `generate.py` | – | Parametric source; rebuilds every STL |
| `check_fit.py` | – | Fit and wall-thickness checks |

## Bought parts

| Qty | Part |
|---|---|
| 6 | Flexible WS2812 8×8 panel, 80 × 80 mm |
| 6 | Backing sheet 80 × 80 × 1 mm (aluminium, FR4 or acrylic) |
| 1 | Snap-in USB-C socket with leads |
| 1 | SS12F15 slide switch |
| 24 | 2 mm dowel, about 5.5 mm long |

The dowels sit in small bosses at the root of each mitre wall, two per edge, centred on a baffle.

## Rebuilding

```bash
cd case/voxel-skin-80x80
pip install manifold3d trimesh numpy rtree scipy shapely
python3 generate.py
python3 check_fit.py   # must end with "All checks passed."
```
