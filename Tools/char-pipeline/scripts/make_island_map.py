"""Draws the RRRandom island map (36 x 36 cells) and prints it as C++ for ARRRandomIsland's constructor.

Run with any Python 3 (Blender's: blender -b --factory-startup --python make_island_map.py -- <out .inc> <preview .txt>).
Writes <out .inc>: the TypeRows / HeightRows / PropRows initializers, and <preview .txt>: the three grids side by side
plus checks (row lengths, the teams' start cells on sand, props on the right kind of cell, cliffs that can't be climbed
off the island).

Layout (row 0 = far up the screen, +X; column 0 = left, -Y): a village toy town in a valley.
- Top: high grass cliffs (pines and round trees), a waterfall at the top right feeding a river down the right side,
  a terrace below them with the windmill on a low hill.
- Two villages on the starting roads (red row 9, blue row 26): mushroom houses, fences, lamps and signposts; each road
  crosses the river on a bridge to the east bank.
- A ring road (the two village roads, a west lane and an east lane) and the main road down the middle, for vehicles.
- The middle field: flower patches, crates and toy blocks (solid, like cover), two low hills to climb (MapleStory-like
  platforms); the game mode adds its cover walls there.
- A mushroom forest down the left terrace, and a beach with two docks onto the bay at the bottom.
"""
import random
import sys

R = C = 36
T = [["."] * C for _ in range(R)]
H = [[0] * C for _ in range(R)]
P = [["."] * C for _ in range(R)]
rnd = random.Random(36)


def fill(grid, r0, r1, c0, c1, v):
    for r in range(r0, r1 + 1):
        for c in range(c0, c1 + 1):
            grid[r][c] = v


def hill(r0, r1, c0, c1, lo, hi):
    for r in range(r0, r1 + 1):
        for c in range(c0, c1 + 1):
            T[r][c] = "g"
            H[r][c] = rnd.randint(lo, hi)


RIVER = (29, 31)
# Top cliffs and the terrace under them
hill(0, 0, 0, C - 1, 8, 9)
hill(1, 1, 0, C - 1, 7, 8)
hill(2, 2, 0, C - 1, 6, 7)
for c in range(0, 29):
    # The terrace: 1-2 levels (climbable from the floor), gaps down to the floor
    if rnd.random() < 0.8:
        T[3][c], H[3][c] = "g", rnd.randint(1, 2)
# Waterfall into the river
for r, h in ((0, 8), (1, 6), (2, 4)):
    fill(T, r, r, RIVER[0], RIVER[1], "F")
    fill(H, r, r, RIVER[0], RIVER[1], h)
fill(T, 3, 29, RIVER[0], RIVER[1], "w")
fill(H, 3, 29, RIVER[0], RIVER[1], 0)
# Right edge: the east bank (floor) under a cliff wall
fill(T, 3, 29, 32, 34, ".")
hill(3, 29, 35, 35, 6, 6)
hill(3, 3, 32, 34, 2, 2)
# Left edge: a wall over a terrace (the terrace climbable from the floor, the wall not)
hill(3, 29, 0, 0, 6, 7)
for r in range(4, 30):
    T[r][1], H[r][1] = "g", rnd.randint(1, 2)
    if r % 7 == 3:
        T[r][1], H[r][1] = ".", 0
# The bay at the bottom: a beach, then water with two docks
fill(T, 30, 35, 0, C - 1, "w")
fill(T, 30, 30, 2, 28, "s")
fill(T, 30, 30, 32, 34, "s")
for c0 in (7, 22):
    fill(T, 31, 33, c0, c0 + 1, "d")
fill(T, 30, 30, 0, 1, "w")

# Roads: the village roads (the teams start on them), the main road down the middle, the west and east lanes
for row in (9, 26):
    fill(T, row, row, 2, 28, "s")
    fill(T, row, row, RIVER[0], RIVER[1], "b")
    fill(T, row, row, 32, 34, "s")
fill(T, 4, 30, 17, 18, "s")
fill(T, 9, 26, 7, 7, "s")
fill(T, 9, 26, 27, 27, "s")
# Plazas where the main road meets the village roads
fill(T, 8, 10, 15, 20, "s")
fill(T, 25, 27, 15, 20, "s")

# Low hills in the field (platforms to climb, 1 then 2 levels)
for (r0, r1, c0, c1) in ((13, 14, 10, 12), (21, 22, 22, 24)):
    hill(r0, r1, c0, c1, 1, 1)
    H[(r0 + r1) // 2][(c0 + c1) // 2] = 2
# The windmill's hill on the terrace
hill(4, 5, 4, 6, 1, 1)
H[4][5] = 2

# ---- Props ----
def put(r, c, letter):
    P[r][c] = letter


# Woods along the top cliffs (tall, far from the camera)
for c in range(0, 29):
    for r in (0, 1):
        if rnd.random() < 0.75:
            put(r, c, rnd.choice("PPT"))
    if rnd.random() < 0.35:
        put(2, c, rnd.choice("TB"))
for c in range(32, 36):
    put(0, c, "P")
    put(1, c, rnd.choice("PT"))
# Boulders where the falls land
put(3, 28, "R")
put(3, 32, "R")
# Terrace: bushes, flowers, a few mushrooms
for c in range(0, 29):
    if T[3][c] == "g" and rnd.random() < 0.35:
        put(3, c, rnd.choice("BYM"))
# The windmill
put(4, 5, "V")
put(5, 8, "Y")
put(4, 9, "B")

# Red village (north of its road, rows 5-8) and blue village (south of its road, rows 27-29)
def village(house_row, fence_row, lamp_row, sign_row, flower_rows):
    for c in (9, 13, 24):
        put(house_row, c, "H")
    for c in list(range(3, 7)) + list(range(9, 13)) + list(range(22, 27)):
        if P[fence_row][c] == "." and T[fence_row][c] == ".":
            put(fence_row, c, "N" if False else "Z")
    for c in (6, 14, 21, 28):
        put(lamp_row, c, "O")
    put(sign_row, 16, "S")
    for r in flower_rows:
        for c in (2, 8, 13, 20, 23, 26):
            if P[r][c] == "." and T[r][c] in ".g":
                put(r, c, "Y")


village(6, 8, 10, 10, (5, 7))
village(29, 27, 25, 25, (28,))
# Crates by the roads
for (r, c) in ((10, 9), (10, 25), (25, 10), (25, 24), (8, 19), (27, 14)):
    put(r, c, "C")
# Bridges
put(9, 30, "J")
put(26, 30, "J")

# Field: toy blocks and crates (solid), flowers (low)
for (r, c) in ((12, 15), (23, 20), (15, 23), (20, 11), (17, 4), (18, 24)):
    if T[r][c] == "." and P[r][c] == ".":
        put(r, c, "K")
for (r, c) in ((11, 13), (11, 22), (13, 16), (16, 9), (16, 25), (19, 9), (19, 25), (22, 19), (24, 12), (24, 23), (12, 25),
               (23, 9), (17, 14), (18, 21), (14, 11), (21, 23)):
    if P[r][c] == ".":
        put(r, c, "Y")

# Mushroom forest on the left (between the terrace and the west lane)
for r in range(11, 25):
    for c in range(2, 7):
        if T[r][c] == "." and P[r][c] == "." and rnd.random() < 0.38:
            put(r, c, rnd.choice("MMYB"))
for r in range(4, 30):
    if T[r][1] == "g" and P[r][1] == "." and rnd.random() < 0.5:
        put(r, 1, rnd.choice("MBT"))

# East bank: round trees, flowers, a lamp at each bridge
for r in range(4, 30):
    for c in (33, 34):
        if T[r][c] == "." and P[r][c] == "." and r not in (8, 9, 10, 25, 26, 27) and rnd.random() < 0.35:
            put(r, c, rnd.choice("TBY"))
put(8, 32, "O")
put(27, 32, "O")
# Reeds along the river banks and the bay shore
for r in range(4, 30):
    if r in (9, 26):
        continue
    if rnd.random() < 0.3:
        put(r, RIVER[0], "L")
    if rnd.random() < 0.2:
        put(r, RIVER[1], "L")
for c in range(0, C):
    if T[31][c] == "w" and rnd.random() < 0.18:
        put(31, c, rnd.choice("LLR"))
# Beach: flowers and a lamp by each dock
put(30, 6, "O")
put(30, 21, "O")

# Room for the big ones: nothing else within a cell of a house or the windmill
for r in range(R):
    for c in range(C):
        if P[r][c] in "HV":
            for rr in range(max(0, r - 1), min(R, r + 2)):
                for cc in range(max(0, c - 1), min(C, c + 2)):
                    if (rr, cc) != (r, c) and P[rr][cc] not in "HV":
                        P[rr][cc] = "."


# ---- Checks ----
problems = []
for row in (9, 26):
    for c in range(16, 20):
        if T[row][c] != "s":
            problems.append(f"start cell ({row},{c}) is {T[row][c]}")
water_ok = set("LJR")
for r in range(R):
    for c in range(C):
        p, t = P[r][c], T[r][c]
        if p == ".":
            continue
        if t in "wd" and p not in water_ok:
            problems.append(f"{p} on water at ({r},{c})")
        if t == "b" and p != "J":
            problems.append(f"{p} on a bridge at ({r},{c})")
        if t == "s" and p not in "OSCJ":
            problems.append(f"{p} on a road at ({r},{c})")
        if p == "J" and t != "b":
            problems.append(f"bridge off its cells at ({r},{c})")
# Nobody climbs out: from any walkable cell (height h), a neighbor of height >= h + 3 is a wall; the outer ring must be
# water or a cliff of at least 3 over its inner neighbor
for r in range(R):
    for c in range(C):
        if r in (0, R - 1) or c in (0, C - 1):
            if T[r][c] not in "wFg":
                problems.append(f"edge cell ({r},{c}) is {T[r][c]}")
for c in range(1, 29):
    if H[2][c] - H[3][c] < 3:
        problems.append(f"top wall too low at col {c}: {H[2][c]} over {H[3][c]}")
for r in range(3, 30):
    if H[r][0] - H[r][1] < 3:
        problems.append(f"left wall too low at row {r}")
    if H[r][35] - H[r][34] < 3:
        problems.append(f"right wall too low at row {r}")

types = ["".join(row) for row in T]
heights = ["".join(str(h) for h in row) for row in H]
props = ["".join(row) for row in P]


def block(name, rows):
    return f"\t{name} = {{\n" + "".join(f'\t\tTEXT("{row}"),\n' for row in rows) + "\t};\n"


out_inc, out_preview = sys.argv[sys.argv.index("--") + 1:][:2] if "--" in sys.argv else sys.argv[1:3]
with open(out_inc, "w", newline="\r\n") as f:
    f.write(block("TypeRows", types) + block("HeightRows", heights) + block("PropRows", props))
with open(out_preview, "w") as f:
    for r in range(R):
        f.write(f"{r:2d} {types[r]}  {heights[r]}  {props[r]}\n")
    f.write("\n".join(problems) or "no problems")
print("MAP", "no problems" if not problems else f"{len(problems)} problems")
for p in problems:
    print("MAP", p)
