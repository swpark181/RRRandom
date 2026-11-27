# Builds the fox vs tiger cinematic: the set, the cast, the cameras and the Level Sequence (Map_FoxVsTiger +
# LS_FoxVsTiger). Run with UnrealEditor-Cmd -run=pythonscript (cine_run.sh build). Safe to run again: it rebuilds
# everything labelled Cine_* and the sequence from scratch.
# 2026-11-24, the re-cut (the user: "콘티 손본거 따라서 작업 진행해줘"): this is the storyboard draft's build
# (ue_board_build.py: 13 cuts, 32 s, shown on the draft page) moved to the real map and sequence and cleaned up. The
# film before it (final v8: 9 cuts, 17 s) is ue_cine_build_v8_1123.py. The fox's and the tiger's new clips come from
# ue_board_motions.py (A_Fox_Board_*, A_Tiger_Board_*). Cut times and paths are written to storyboard/final/layout.json.
#
# The user's cuts (numbers of the storyboard page; old cut 7 is merged into 6, so there is no 7):
#   1 chase: big close-up of the fox, one anxious look back, zoom out until the tiger shows blurred behind
#   2 obstacle (long, one height): hands on it, up, along the top, down with a roll
#   3 dead end: runs into the wall, turns and slumps to the floor
#   4 darkened street: the tiger comes through the shadow, his face comes into the light
#   5 the fox's frightened face; the gold die appears over its head, it slowly looks up, its face brightens
#   6 the fox stands up, its clones surround the tiger in a wide ring; the tiger flinches
#   8 the foxes close in step by step; the tiger drops his hammer
#   9 the grin, as before
import json
import os
import math
import random

import unreal

FPS = 24
# Tension pass (2026-11-23, the user: "이 구도대로 게임 캐릭터 배치해서 초안 만들어줘", after the sketch page page3): between
# cut 3 and cut 6 the tiger does not stop, raises his hammer over the fox, and the blow lands on the gold die that pops
# up over the fox's head; both watch it rock between 1 and 5; its burst pushes him back to the middle of the alley.
#   3-끝 (C3E) straight down: boxed in, his shadow reaches the fox      4 he comes through the dark (as before, shorter)
#   4-2 over the fox's shoulder: he comes right up                      4-3 up at the rising hammer / the fox's eyes (x2)
#   5-1 side on: the blow, the die    5-2 the two faces and the die     5-3 the burst, he skids back
C1, C2, C3 = 0.0, 3.5, 7.5
C3E, C4, C42, C43, C51, C52, C53 = 11.0, 12.25, 14.25, 16.5, 19.0, 20.0, 23.0
C6, C8, C9 = 24.5, 27.0, 30.0
# (2026-11-24, the user: fades. It fades in from black over the first FADE_IN seconds; the grin plays its two seconds
# as before and then fades out over FADE_OUT more. RR_CINE_NOFADE=1 builds without the fades, to try other lengths on
# the frames.)
FADE_IN, FADE_OUT = 0.5, 0.75
END_T = 32.0 + FADE_OUT
S43 = [C43, 17.5, 18.125, 18.5]                     # 4-3's four shots: up at the hammer, the eyes, up, the eyes
LENGTH = END_T
LANE_X = -4980.0
FOX_SCALE = 1.8
MAP_SRC = "/Game/Maps/DiceTown"
FOLDER = "/Game/Cinematics/FoxVsTiger"
MAP = FOLDER + "/Map_FoxVsTiger"
SEQ_NAME = "LS_FoxVsTiger"
OUT_JSON = "C:/work/char-pipeline/output/storyboard/final/layout.json"
ALLEY_Y = -3995.0
WALL_X = -6250.0
TIGER_SPOT = unreal.Vector(-5750.0, ALLEY_Y, 0.0)
SIT = unreal.Vector(-6182.0, ALLEY_Y, 0.0)          # where the fox slumps, its back on the wall (the wall's face is at -6200)
SIT_DROP = 30.0                                    # the sitting pose's hips come down this far
RING = 170.0                                       # the tight ring (as before), where the foxes end up in cut 8
WIDE_A, WIDE_B = 390.0, 140.0                      # the wide ring of cut 6: an oval the length of the alley (it is 365 wide)
FOX_ANGLE = 180.0
CLONE_ANGLES = [0.0, 60.0, 120.0, 240.0, 300.0]
DIE_ACROSS = 80.0
DIE_HEIGHT = 165.0                                 # over the sitting fox's ears
DIE_HOP = 30.0
DIE_FACE = 5
T_SWING = C51 + 0.25                               # the blow starts (the die pops up under it)
T_BONK = T_SWING + 0.12                            # ...and lands on the die
T_DIE = T_SWING
T_ROCK = C52 + 1.3                                 # the die has slowed: it stands on an edge and rocks
T_ONE = C52 + 1.9                                  # leans to the 1
T_FIVE = C52 + 2.55                                # tips over to the 5
T_LAND = C53 + 0.05                                # bursts
DIE_SPIN = T_LAND - T_BONK
T_COWER = S43[1] + 0.1                             # the fox shrinks from the hammer (0.5 s)
T_LOOK = C52 + 0.4                                 # ...peeks, and looks up at the die (1 s)
T_POOF = C6 + 0.25                                 # the fox is on its feet by then
T_FLINCH = T_POOF + 0.1
T_DROP = C8 + 1.3
T_CROUCH = C8 + 1.9
HAMMER_REST = TIGER_SPOT + unreal.Vector(-80.0, 75.0, 18.0)
RUN_SPEED = 285.0
RUN_RATE = 1.35

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
V = unreal.Vector


def log(msg):
    unreal.log(f"CINEBUILD {msg}")


def f(sec):
    return int(round(sec * FPS))


def anim(species, name):
    path = name if name.startswith("/Game/") else f"/Game/Characters/{species}/Anims/A_{species}_{name}"
    a = unreal.load_asset(path)
    if not a:
        log(f"missing animation {path}")
    return a


def look_rot(pos, target):
    return unreal.MathLibrary.find_look_at_rotation(pos, target)


def ring_spot(angle, a=RING, b=RING):
    r = math.radians(angle)
    return V(TIGER_SPOT.x + a * math.cos(r), TIGER_SPOT.y + b * math.sin(r), 0.0)


def yaw_to(src, dst):
    return math.degrees(math.atan2(dst.y - src.y, dst.x - src.x))


# ---------------------------------------------------------------------------------------------------------------- map
if not eal.does_asset_exist(MAP):
    eal.duplicate_asset(MAP_SRC, MAP)
    log(f"map copied to {MAP}")
les.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.GameModeBase)
SUN_PITCH, SUN_YAW = -9.0, 172.0                    # the real cinematic's golden hour
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.DirectionalLight):
        a.set_actor_rotation(unreal.Rotator(0.0, SUN_PITCH, SUN_YAW), False)
        a.get_component_by_class(unreal.DirectionalLightComponent).set_light_color(unreal.LinearColor(1.0, 0.72, 0.48, 1.0))
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith(("Cine_", "Aud_")):
        eas.destroy_actor(a)
if eal.does_asset_exist(f"{FOLDER}/{SEQ_NAME}"):
    eal.delete_asset(f"{FOLDER}/{SEQ_NAME}")


def spawn(cls, label, loc, rot=unreal.Rotator(0, 0, 0)):
    a = eas.spawn_actor_from_class(cls, loc, rot)
    a.set_actor_label(label)
    a.set_folder_path("Cinematic")
    return a


# ---------------------------------------------------------------------------------------------------------------- cast
AWAY = V(0.0, 0.0, -20000.0)
fox = spawn(unreal.RRRandomCineAnimal, "Cine_Fox", AWAY)
fox.set_editor_property("species", 0)
clones = []
for i, ang in enumerate(CLONE_ANGLES):
    c = spawn(unreal.RRRandomCineAnimal, f"Cine_Clone{i + 1}", AWAY)
    c.set_editor_property("species", 0)
    clones.append(c)
tiger = spawn(unreal.RRRandomCineAnimal, "Cine_Tiger", AWAY)
tiger.set_editor_property("species", 3)
tiger.set_editor_property("hold_hammer", True)
tiger.set_editor_property("hold_shield", True)
EXPR = "/Game/Cinematics/FoxVsTiger/Expressions/"
FOX_EXPR = ("Scared", "Worried", "Determined", "Grin", "WorriedBlink", "GrinBlink", "Joy")
TIGER_EXPR = ("Smug", "Startled", "Nervous")
PART_OF = {"WorriedBlink": "Worried", "GrinBlink": "Grin"}


def muzzle_parts(species, names):
    parts = []
    for n in names:
        path = f"/Game/Characters/{species}/SK_{species}Muzzle_{PART_OF.get(n, n)}"
        parts.append(unreal.load_asset(path) if eal.does_asset_exist(path) else None)
    return parts


for a in [fox] + clones:
    a.set_editor_property("expressions", [unreal.load_asset(EXPR + f"M_Fox_Expr_{n}") for n in FOX_EXPR])
    a.set_editor_property("expression_furs", [unreal.load_asset(EXPR + f"M_FoxFur_Expr_{n}") for n in FOX_EXPR])
    a.set_editor_property("expression_parts", muzzle_parts("Fox", FOX_EXPR))
tiger.set_editor_property("expressions", [unreal.load_asset(EXPR + f"M_Tiger_Expr_{n}") for n in TIGER_EXPR])
tiger.set_editor_property("expression_furs", [unreal.load_asset(EXPR + f"M_TigerFur_Expr_{n}") for n in TIGER_EXPR])
tiger.set_editor_property("expression_parts", muzzle_parts("Tiger", TIGER_EXPR))
# (2026-11-24, the user: the fur in the close-ups) the fox and the tiger wear a denser fur here: 36 layers where the
# game's has 12, which showed as rows of dashes along the faces' edges (ue_cine_import_fur.py). The clones, seen from
# far and above, keep the game's. RR_CINE_GAMEFUR=1 builds with the game's fur on everybody.
if not os.environ.get("RR_CINE_GAMEFUR"):
    for who, name in ((fox, "Fox"), (tiger, "Tiger")):
        dense = unreal.load_asset(f"/Game/Cinematics/FoxVsTiger/Fur/SK_{name}FurCine")
        if dense:
            who.set_editor_property("fur_mesh", dense)
        else:
            log(f"no dense fur for the {name}: the game's fur")
fox_skel = unreal.load_asset("/Game/Characters/Fox/SK_Fox_Skeleton")
try:
    fox_skel.add_compatible_skeleton(unreal.load_asset("/Game/Characters/FoxMM/SK_FoxMM_Skeleton"))
    eal.save_loaded_asset(fox_skel)
except Exception as e:  # noqa: BLE001
    log(f"compatible skeleton failed: {e}")

dropped = spawn(unreal.StaticMeshActor, "Cine_HammerDropped", AWAY, unreal.Rotator(0, 0, 35))
dropped.static_mesh_component.set_static_mesh(unreal.load_asset("/Game/Props/Weapons98/SM_Hammer"))
dropped.set_actor_scale3d(V(0.86, 0.86, 0.86))

die = spawn(unreal.StaticMeshActor, "Cine_GoldDie", AWAY)
die_mesh = unreal.load_asset("/Game/Props/Dice/SM_Dice")
die.static_mesh_component.set_static_mesh(die_mesh)
gold = unreal.load_asset("/Game/Props/Dice/M_DiceGold")
for i, slot in enumerate(die.static_mesh_component.get_material_slot_names()):
    if "Pip" not in str(slot) and gold:
        die.static_mesh_component.set_material(i, gold)
die.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
bounds = die_mesh.get_bounds()
die_scale = DIE_ACROSS / max(max(bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z) * 2.0, 1.0)
die.set_actor_scale3d(V(die_scale, die_scale, die_scale))

WHITE = unreal.Color(r=252, g=250, b=245, a=255)
PINK = unreal.Color(r=255, g=118, b=170, a=255)
LEMON = unreal.Color(r=255, g=214, b=70, a=255)
MINT = unreal.Color(r=88, g=214, b=160, a=255)
SKY = unreal.Color(r=92, g=182, b=255, a=255)
LAVENDER = unreal.Color(r=176, g=132, b=255, a=255)
PIP_DARK = unreal.Color(r=40, g=40, b=62, a=255)
PIP_BROWN = unreal.Color(r=110, g=70, b=30, a=255)
DIE_LOOKS = [(WHITE, PIP_DARK), (PINK, WHITE), (SKY, WHITE), (LEMON, PIP_BROWN), (MINT, WHITE), (LAVENDER, WHITE)]
PLAIN_BODIES = [LAVENDER, PINK, LAVENDER, SKY, MINT, LAVENDER]
rng = random.Random(1119)


def block(x, y, z, die_chance=0.33):
    b = unreal.RRCineBlock()
    b.set_editor_property("location", V(x, y, z))
    if rng.random() < die_chance:
        body, accent = rng.choice(DIE_LOOKS)
        b.set_editor_property("die", True)
    else:
        body, accent = rng.choice(PLAIN_BODIES), WHITE
    b.set_editor_property("body", body)
    b.set_editor_property("accent", accent)
    return b


# The dead end (the real cinematic's wall)
wall = []
for i in range(7):
    for lv in range(7):
        wall.append(block(WALL_X, -4300.0 + 100.0 * i, 50.0 + 100.0 * lv))
for j in range(1, 4):
    for t in range(2):
        for lv in range(7):
            wall.append(block(WALL_X + 100.0 * j, -4250.0 - 100.0 * t, 50.0 + 100.0 * lv))
walls = spawn(unreal.RRRandomCineBlocks, "Cine_DeadEnd", V(WALL_X, ALLEY_Y, 0))
walls.set_editor_property("blocks", wall)

# ---------------------------------------------------------------------------------------------------------------- sequence
seq = tools.create_asset(SEQ_NAME, FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_playback_start(0)
seq.set_playback_end(f(LENGTH))
END = f(LENGTH)
LIN = unreal.MovieSceneKeyInterpolation.LINEAR
STEP = unreal.MovieSceneKeyInterpolation.CONSTANT
ticks = seq.get_tick_resolution()
TICKS = ticks.numerator / max(1, ticks.denominator)


def transform_keys(binding, keys):
    """keys: [(sec, location, yaw or Rotator, interpolation[, scale])]"""
    track = binding.add_track(unreal.MovieScene3DTransformTrack)
    sec = track.add_section()
    sec.set_range(0, END)
    ch = sec.get_all_channels()
    for key in keys:
        t, loc, rot, interp = key[:4]
        if not isinstance(rot, unreal.Rotator):
            rot = unreal.Rotator(0, 0, rot)
        frame = unreal.FrameNumber(f(t))
        for c, v in zip(ch[:6], (loc.x, loc.y, loc.z, rot.roll, rot.pitch, rot.yaw)):
            c.add_key(frame, v, interpolation=interp)
        if len(key) > 4:
            for c in ch[6:9]:
                c.add_key(frame, key[4], interpolation=interp)


def set_play_rate(params, rate):
    if abs(rate - 1.0) < 1e-3:
        return
    try:
        params.set_editor_property("play_rate", rate)
        return
    except Exception:  # noqa: BLE001
        pass
    tw = params.get_editor_property("play_rate")
    for way in ("set_fixed_play_rate", "set_fixed"):
        if hasattr(tw, way):
            getattr(tw, way)(rate)
            params.set_editor_property("play_rate", tw)
            return
    for lib in ("MovieSceneTimeWarpExtensions", "MovieSceneSectionExtensions"):
        fn = getattr(getattr(unreal, lib, None), "set_fixed_play_rate", None) if hasattr(unreal, lib) else None
        if fn:
            fn(tw, rate)
            params.set_editor_property("play_rate", tw)
            return
    log(f"play rate {rate} not set")


OFFSET_STATE = {"way": None}


def anim_sections(actor, species, parts):
    """parts: [(start, end, anim name, play rate[, ease in, ease out[, clip start offset in seconds]])] on the actor's
    Body; parts with ease times overlap their neighbours and cross-fade (alternate rows)."""
    b = seq.add_possessable(actor.get_editor_property("body"))
    track = b.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    for i, part in enumerate(parts):
        start, end, name, rate = part[:4]
        a = anim(species, name)
        if not a:
            continue
        s = track.add_section()
        s.set_range(f(start), f(end))
        p = s.get_editor_property("params")
        p.set_editor_property("animation", a)
        set_play_rate(p, rate)
        if len(part) > 6 and part[6] > 0:
            for prop in ("first_loop_start_frame_offset", "start_frame_offset"):
                try:
                    p.set_editor_property(prop, unreal.FrameNumber(int(round(part[6] * TICKS))))
                    if OFFSET_STATE["way"] != prop:
                        OFFSET_STATE["way"] = prop
                        log(f"clip start offset set with {prop}")
                    break
                except Exception as err:  # noqa: BLE001
                    log(f"clip start offset: {prop} failed: {err}")
        s.set_editor_property("params", p)
        s.set_row_index(i % 2)
        if len(part) > 4 and (part[4] > 0 or part[5] > 0):
            unreal.RRRandomCineLibrary.set_section_easing(s, part[4], part[5])
    return b


def channel_keys(binding, cls, prop, path, keys, default, interp=None):
    track = binding.add_track(cls)
    track.set_property_name_and_path(prop, path)
    s = track.add_section()
    s.set_range(0, END)
    ch = s.get_all_channels()[0]
    ch.set_default(default)
    if not keys or keys[0][0] > 0:
        keys = [(0.0, default)] + list(keys)
    for t, v in keys:
        if interp is None:
            ch.add_key(unreal.FrameNumber(f(t)), v)
        else:
            ch.add_key(unreal.FrameNumber(f(t)), v, interpolation=interp)


def bool_keys(binding, prop, keys, default):
    channel_keys(binding, unreal.MovieSceneBoolTrack, prop, prop, keys, default)


def int_keys(binding, prop, keys, default):
    channel_keys(binding, unreal.MovieSceneIntegerTrack, prop, prop, keys, default)


def visible_keys(binding, keys, start_visible):
    channel_keys(binding, unreal.MovieSceneVisibilityTrack, "bHiddenInGame", "bHiddenInGame", keys, start_visible)


# ------------------------------------------------------------------------------------------------- cut 2: the obstacle
MANTLE = "/Game/Characters/FoxMM/Traversal/A_FoxMM_MantleRun"
ROLL = "/Game/Characters/FoxMM/Traversal/A_FoxMM_LandRoll"
MANTLE_RATE, ROLL_RATE, ROLL_T0 = 1.35, 1.45, 0.42
TOP = 100.0                                         # the obstacle's height: one town block
ext = unreal.AnimPoseExtensions
pose_opt = unreal.AnimPoseEvaluationOptions()
pose_opt.extract_root_motion = False


def smooth_step(x):
    x = min(1.0, max(0.0, x))
    return x * x * (3.0 - 2.0 * x)


def clip_root(clip, t):
    r = ext.get_bone_pose(ext.get_anim_pose_at_time(clip, t, pose_opt), "Root", unreal.AnimPoseSpaces.WORLD).translation
    return r.y, r.z


T_M = C2 + 0.3                                      # the mantle clip starts
Y_M = -3083.0                                       # ...here
mantle = unreal.load_asset(MANTLE)
m_len = mantle.get_play_length()
m_f0, _ = clip_root(mantle, 0.0)
m_up = max(clip_root(mantle, m_len * k / 40.0)[1] for k in range(41))
fox_keys = []
y_edge = None
n = int(round(m_len / MANTLE_RATE * FPS))
for i in range(n + 1):
    ct = min(m_len, i / FPS * MANTLE_RATE)
    fwd, up = clip_root(mantle, ct)
    y = Y_M - (fwd - m_f0) * FOX_SCALE
    z = max(0.0, up) / m_up * TOP
    if y_edge is None and up > 0.5 * m_up:
        y_edge = y
    fox_keys.append((T_M + i / FPS, V(LANE_X, y, z), -90, LIN))
T_ME = T_M + n / FPS
y_me = fox_keys[-1][1].y
OB_FRONT = y_edge + 35.0                            # the face the fox climbs
OB_ROWS = 4
OB_BACK = OB_FRONT - 100.0 * OB_ROWS
T_RO = T_ME + max(0.0, (y_me - (OB_BACK + 15.0)) / RUN_SPEED)   # runs to the far edge, then off it
y_off = OB_BACK + 15.0
if T_RO > T_ME:
    fox_keys.append((T_RO, V(LANE_X, y_off, TOP), -90, LIN))
roll = unreal.load_asset(ROLL)
r_len = roll.get_play_length()
r_f0, r_up0 = clip_root(roll, ROLL_T0)
n = int(round((C3 - T_RO) * FPS)) - 1
T_ROLL_DOWN = T_RO
for i in range(1, n + 1):
    ct = min(r_len, ROLL_T0 + i / FPS * ROLL_RATE)
    fwd, up = clip_root(roll, ct)
    z = max(0.0, up) / r_up0 * TOP
    if z > 0.5:
        T_ROLL_DOWN = T_RO + i / FPS
    fox_keys.append((T_RO + i / FPS, V(LANE_X, y_off - (fwd - r_f0) * FOX_SCALE, z), -90, LIN if i < n else STEP))
# cut 1's run: ends 620 short of the obstacle (cut 2 picks the fox up 0.3 s from it: a jump in place at the cut)
FOX_START = V(LANE_X, OB_FRONT + 620.0 + RUN_SPEED * C2, 0.0)
fox_keys = [(0.0, FOX_START, -90, LIN), (C2 - 0.04, FOX_START + V(0, -RUN_SPEED * (C2 - 0.04), 0), -90, STEP),
            (C2, V(LANE_X, Y_M + RUN_SPEED * (T_M - C2), 0), -90, LIN)] + fox_keys
log(f"obstacle: Y {OB_FRONT:.0f} .. {OB_BACK:.0f}, {TOP:.0f} high; mantle {T_M:.2f}-{T_ME:.2f} s (root up {m_up:.1f}), off the far edge at {T_RO:.2f} s, "
    f"on the floor at {T_ROLL_DOWN:.2f} s, fox at Y {fox_keys[-1][1].y:.0f} when the cut ends")
ob = []
for row in range(OB_ROWS):
    for ix in range(-2, 3):
        ob.append(block(LANE_X + 100.0 * ix, OB_FRONT - 50.0 - 100.0 * row, 50.0, 0.4))
clutter = spawn(unreal.RRRandomCineBlocks, "Cine_Clutter", V(LANE_X, OB_FRONT, 0))
clutter.set_editor_property("blocks", ob)
OB_MID = V(LANE_X, (OB_FRONT + OB_BACK) / 2.0, 50.0)

# ------------------------------------------------------------------------------------------------- the fox
# Motions (2026-11-23, ue_board_motions.py): in the alley the fox plays real clips, and is moved and turned by the roots
# of the motion matching sample's clips they come from (the copies on the game fox's own skeleton have no root bone).
SAMPLE = "/Game/Characters/FoxMM/"
bench_in = unreal.load_asset(SAMPLE + "Cine/A_FoxMM_BenchInto")
bench_out = unreal.load_asset(SAMPLE + "Cine/A_FoxMM_BenchOut")
walk_start = unreal.load_asset(SAMPLE + "Anims/A_FoxMM_Walk_Start_F_Rfoot")
walk_stop = unreal.load_asset(SAMPLE + "Anims/A_FoxMM_Walk_Stop_F_Lfoot")


def root_at(clip, t):
    """(forward in the fox's own units, yaw) of a sample clip's root at a time of the clip."""
    r = ext.get_bone_pose(ext.get_anim_pose_at_time(clip, t, pose_opt), "Root", unreal.AnimPoseSpaces.WORLD)
    q = r.rotation
    return r.translation.y, math.degrees(math.atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z)))


P = anim("Fox", "Run").get_play_length() / RUN_RATE   # one run cycle: sections start on whole cycles, to cross-fade in step
fox_spot = ring_spot(FOX_ANGLE)
# cut 3: runs in at the chase's speed, into the wall, off it; then "sit on the bench in front of you" from where that
# clip starts to turn: round (by the camera's side) with its back to the wall and down, and on down to the floor
T_HIT = C3 + 1.0
HIT_X, OFF_X = -6168.0, -6150.0
T_TURN = T_HIT + 0.3
TURN_RATE, TURN_T0 = 1.5, 2.15                      # (TURN_T0, STAND_T0: where ue_board_motions.py cuts the clips)
T_SIT0 = T_TURN + (3.75 - TURN_T0) / TURN_RATE      # half way down to the bench's height: from here on down to the floor
T_FLOOR = T_SIT0 + 0.45
alley_keys = [(C3, V(HIT_X + RUN_SPEED * (T_HIT - C3), ALLEY_Y, 0), 180, LIN),
              (T_HIT, V(HIT_X, ALLEY_Y, 0), 180, LIN), (T_TURN, V(OFF_X, ALLEY_Y, 0), 180, LIN)]
f0, _ = root_at(bench_in, TURN_T0)
for i in range(1, int(round((T_SIT0 - T_TURN) * FPS)) + 1):
    fwd, yaw = root_at(bench_in, TURN_T0 + i / FPS * TURN_RATE)
    alley_keys.append((T_TURN + i / FPS, V(OFF_X - (f0 - fwd) * FOX_SCALE, ALLEY_Y, 0), yaw, LIN))
# (2026-11-24: the last of the way down was one straight line that stopped dead. It drops faster and faster, lands a
# little deep and comes back up: a slump with some weight.)
sit_from = alley_keys[-1][1]
n_drop = int(round((T_FLOOR - T_SIT0) * FPS))
for i in range(1, n_drop + 1):
    u = i / n_drop
    alley_keys.append((T_SIT0 + i / FPS, V(sit_from.x + (SIT.x - sit_from.x) * u, ALLEY_Y, sit_from.z + (-SIT_DROP - 4.0 - sit_from.z) * u ** 1.7), 0, LIN))
for i in range(1, 6):
    alley_keys.append((T_FLOOR + i / FPS, SIT + V(0, 0, -SIT_DROP - 4.0 * (1.0 - smooth_step(i / 5.0))), 0, LIN))
# cut 6: "up from the bench": off the floor as it leans forward, then the clip's own small step forward
T_UP = C53 + 6.0 / FPS
UP_RATE, STAND_T0 = 1.3, 0.4
T_STEP6 = C6 + 6.0 / FPS
g0, _ = root_at(bench_out, STAND_T0)
alley_keys.append((T_UP, SIT + V(0, 0, -SIT_DROP), 0, LIN))
for i in range(1, int(round((T_STEP6 - T_UP) * FPS)) + 1):
    tau = i / FPS
    fwd, _ = root_at(bench_out, min(bench_out.get_play_length(), STAND_T0 + tau * UP_RATE))
    alley_keys.append((T_UP + tau, SIT + V((fwd - g0) * FOX_SCALE, 0, -SIT_DROP * max(0.0, 1.0 - tau / 0.4)), 0, LIN))
stood = alley_keys[-1][1]
# One step and stop: the walk start to 0.55 s of its clip (the right foot goes out), then the walk stop from 1.0 s of
# its clip (the left foot comes up beside it); the fox goes forward by the two clips' roots.
STEP_RATE = 1.25
STEP_A = 0.44
STEP_MOVE = STEP_A + 0.4 / STEP_RATE                # the root is still again
a0, a1 = root_at(walk_start, 0.0)[0], root_at(walk_start, STEP_A * STEP_RATE)[0]
b0, b1 = root_at(walk_stop, 1.0)[0], root_at(walk_stop, 1.4)[0]
STEP_LEN = ((a1 - a0) + (b1 - b0)) * FOX_SCALE      # what the feet cover
BEATS = [C8 + 0.25, C8 + 1.15, C8 + 2.05]           # cut 8's three steps
STEPS = [(t, t + STEP_MOVE) for t in BEATS]
# the fox: one step after it stands (into its place in the wide ring), then three to its place in the tight one
STEP_CM = (fox_spot.x - stood.x) / 4.0
fox_wide = stood + V(STEP_CM, 0, 0)
WIDE_END = RING + 3.0 * STEP_CM                     # the clone opposite the fox stands as far out as the fox
log(f"steps: the clips cover {STEP_LEN:.1f} cm in {STEP_MOVE:.2f} s; the fox stands up at X {stood.x:.0f} and takes four of {STEP_CM:.1f} cm; wide ring {WIDE_END:.0f}")


def step_part(tau):
    """How far along its step (0..1) a fox is, tau seconds after it starts."""
    if tau <= STEP_A:
        d = root_at(walk_start, tau * STEP_RATE)[0] - a0
    else:
        d = (a1 - a0) + root_at(walk_stop, min(1.4, 1.0 + (tau - STEP_A) * STEP_RATE))[0] - b0
    return d * FOX_SCALE / STEP_LEN


def steps(start, end, times, aiming, face, hold_to):
    """Equal steps from start to end, one starting at each of `times`, the last one's stand held until hold_to:
    (transform keys, animation parts). The fox faces the point `face`, or the way it goes."""
    keys, parts = [], []
    a_name, b_name = ("Board_AimStepA", "Board_AimStepB") if aiming else ("Board_StepA", "Board_StepB")
    n = len(times)
    for k, t0 in enumerate(times):
        p0, p1 = start + (end - start) * (k / n), start + (end - start) * ((k + 1) / n)
        for i in range(int(round(STEP_MOVE * FPS)) + 1):
            pos = p0 + (p1 - p0) * step_part(min(STEP_MOVE, i / FPS))
            keys.append((t0 + i / FPS, pos, yaw_to(pos, face) if face else yaw_to(start, end), LIN))
        last = k + 1 == n
        parts.append((t0, t0 + STEP_A + 0.05, a_name, STEP_RATE, 0.1, 0.1))
        parts.append((t0 + STEP_A - 0.05, hold_to if last else times[k + 1] + 0.1, b_name, STEP_RATE, 0.1, 0.0 if last else 0.1, 1.0 - 0.05 * STEP_RATE))
    return keys, parts


def aim_hold(until):
    """Standing and aiming from the cut to a fox's first step (the aiming stop's still end)."""
    return (C8, until + 0.1, "Board_AimStepB", 0.9, 0.0, 0.1, 1.45)


def tidy(keys):
    """One key a frame (the first), in the order given."""
    out, seen = [], set()
    for key in keys:
        if f(key[0]) not in seen:
            seen.add(f(key[0]))
            out.append(key)
    return out


k6, p6 = steps(stood, fox_wide, [T_STEP6], False, None, T_STEP6 + 1.4)
k8, p8 = steps(fox_wide, fox_spot, BEATS, True, TIGER_SPOT, C9)
fb = seq.add_possessable(fox)
fox_path = tidy(fox_keys + alley_keys + k6 + k8 + [(END_T, fox_spot, yaw_to(fox_spot, TIGER_SPOT), LIN)])
log("fox keys in the alley: " + " ".join(f"{k[0]:.2f}:{k[1].x:.0f}/{k[1].z:.0f}/{(k[2] if not isinstance(k[2], unreal.Rotator) else k[2].yaw):.0f}" for k in fox_path if C3 <= k[0] <= C3 + 3.2 or C6 <= k[0] <= C6 + 0.6))
transform_keys(fb, fox_path)
anim_sections(fox, "Fox", [
    (0.0, P + 0.05, "Run", RUN_RATE, 0.0, 0.05),
    (P, 4 * P + 0.05, "Board_RunLookBack", RUN_RATE, 0.05, 0.05),   # the one look back (three cycles, the look built in)
    (4 * P, T_M + 0.1, "Run", RUN_RATE, 0.05, 0.1),
    (T_M, T_ME + 0.1, MANTLE, MANTLE_RATE, 0.1, 0.1),
    (T_ME, T_RO + 0.1, "Run", RUN_RATE, 0.1, 0.1),
    (T_RO, C3, ROLL, ROLL_RATE, 0.1, 0.0, ROLL_T0),
    (C3, T_HIT + 0.02, "Run", RUN_RATE, 0.0, 0.06),
    (T_HIT - 0.04, T_TURN + 0.15, "Board_WallHit", 1.0, 0.06, 0.15),
    (T_TURN, T_FLOOR, "Board_TurnSit", TURN_RATE, 0.15, 0.45),
    (T_SIT0, T_COWER + 0.5, "Board_Sit", 1.0, 0.45, 0.5),
    (T_COWER, T_LOOK + 1.0, "Board_Cower", 1.0, 0.5, 1.0),         # shrinks as the hammer goes up; peeks after the blow
    (T_LOOK, T_UP + 0.4, "Board_SitLookUp", 1.0, 1.0, 0.4),
    (T_UP, T_STEP6 + 0.1, "Board_StandUp", UP_RATE, 0.4, 0.1),
] + p6 + [(T_STEP6 + 1.3, C8, "Idle", 1.0, 0.1, 0.0), aim_hold(BEATS[0])] + p8 + [(C9, LENGTH, "Cine_PistolAimLow", 1.0)])
bool_keys(fb, "bHoldPistol", [(C8, True)], False)
# faces: scared all the way (worried while it looks back, changed in the head's turns), joy as it looks up at the die, set when it stands, the grin from the clones on (a blink in the close-up)
# (tension pass: eyes squeezed shut as the hammer tops, worried as it peeks, joy at the 5, set as it stands)
# (2026-11-24, the user: the faces. A face is a picture swapped whole, so a change of expression is a pop. Now a change
# the lens sees goes through two frames of closed eyes (the worried face's blink), or happens at a cut; and the fox
# blinks more: looking back, slumped at the wall, watching the die. The tiger has no closed-eyes picture: he stares.)
BLINK = 2.0 / FPS
fox_faces = [
    (P + 0.2, 1),                                              # cut 1: worried as the head turns to look back
    (P + 0.75, 4), (P + 0.75 + 0.125, 1),                      # ...a blink while it looks
    (4 * P - 0.25 - BLINK, 4), (4 * P - 0.25, 0),              # ...eyes shut as the head comes back, and scared again
    (T_FLOOR - 0.3 - BLINK, 4), (T_FLOOR - 0.3, 1),            # cut 3: sliding down the wall, worried
    (S43[1], 0),                                               # 4-3: wide-eyed in its first face shot (changed at the cut)
    (S43[3] + 0.12, 4),                                        # ...squeezed shut in the second
    (T_LOOK + 0.3, 1),                                         # 5-2: peeks
    (T_ROCK + 0.15, 4), (T_ROCK + 0.15 + 0.125, 1),            # ...a blink at the rocking die
    (T_FIVE + 0.06, 6),                                        # ...joy at the 5 (a pop on purpose: the two faces swap)
    (C53 + 0.3, 2), (T_POOF, 3),                               # set as it stands, the grin from the clones on
    (C9 + 0.9, 5), (C9 + 1.05, 3)]
int_keys(fb, "Expression", fox_faces, 0)
# the camera stands where the fox's eyes are in 4-3's two looks up at the hammer: the fox is out of them
visible_keys(fb, [(S43[0], False), (S43[1], True), (S43[2], False), (S43[3], True)], True)

# The clones: the one opposite the fox takes all three steps; the four along the walls stand nearer (the alley is too
# narrow to stand wide of them) and take two, on different beats; each a little after the fox
BEAT_OF = {0.0: (0, 1, 2), 60.0: (0, 2), 120.0: (1, 2), 240.0: (1, 2), 300.0: (0, 2)}
LAG = [0.06, 0.12, 0.03, 0.09, 0.15]
clone_wide = []
for c, ang, lag in zip(clones, CLONE_ANGLES, LAG):
    wide, tight = ring_spot(ang, WIDE_END if ang == 0.0 else WIDE_A, WIDE_B), ring_spot(ang)
    clone_wide.append(wide)
    times = [BEATS[k] + lag for k in BEAT_OF[ang]]
    ck, cp = steps(wide, tight, times, True, TIGER_SPOT, C9)
    cb = seq.add_possessable(c)
    transform_keys(cb, tidy([(0.0, wide, yaw_to(wide, TIGER_SPOT), LIN)] + ck + [(END_T, tight, yaw_to(tight, TIGER_SPOT), LIN)]))
    # (cut 9: the lowered aim, from 1.5 s into the clip, where the hand is lowest. The grin's camera stands half a
    # metre from the clone at 60 degrees: with the aim clip's first seconds its pistol was right in front of the lens.
    # In the film before, the clones were 2.5 s into the aim clip by then, the hand already coming down.)
    anim_sections(c, "Fox", [(0.0, C8, "Idle", 1.0), aim_hold(times[0])] + cp + [(C9, LENGTH, "Cine_PistolAimLow", 0.55, 0.0, 0.0, 1.5)])   # (slowed: past 3 s of the clip the hand swings back)
    bool_keys(cb, "bHoldPistol", [(C8, True)], False)
    int_keys(cb, "Expression", [], 3)
    # (three frames after its puff of smoke starts: at the same frame the clone stood there before the smoke had grown)
    visible_keys(cb, [(T_POOF + 0.125, True)], False)

# ------------------------------------------------------------------------------------------------- the tiger
# The chase: one speed down the lane, a little to the fox's left, so his hammer comes down on the obstacle after the fox
# is over it and rolling away
T_SMASH = T_ROLL_DOWN + 0.45
TIGER_SPEED = 276.0
TIGER_X = LANE_X - 70.0
tiger_y0 = OB_FRONT + 110.0 + TIGER_SPEED * T_SMASH
WALK = 130.0                                        # the game's walk
NEAR = V(SIT.x + 140.0, ALLEY_Y, 0.0)               # where he stands over the fox
X4 = -5530.0                                        # cut 4 picks him up here (the pool of light at TIGER_SPOT is on his way)
T_NEAR = C4 + (X4 - NEAR.x) / WALK
T_R0 = C43 - 0.2                                    # the raise clip starts (0.3 s at rest, 1.5 s up: at the top C43 + 1.6)
T_STOP = T_LAND + 0.95                              # pushed back by the burst to TIGER_SPOT
tb = seq.add_possessable(tiger)
skid = []
for i in range(1, int(round((T_STOP - T_LAND - 0.05) * FPS)) + 1):
    u = min(1.0, i / FPS / (T_STOP - T_LAND - 0.05))
    skid.append((T_LAND + 0.05 + i / FPS, V(NEAR.x + (TIGER_SPOT.x - NEAR.x) * (1.0 - (1.0 - u) ** 2.2), ALLEY_Y, 0), 180, LIN))
tiger_keys = [
    (0.0, V(LANE_X - 200.0, FOX_START.y + 650.0, 0), -90, LIN),       # cut 1: 6.5 m behind the fox, closing a little
    (C2 - 0.04, V(LANE_X - 200.0, FOX_START.y + 580.0 - RUN_SPEED * (C2 - 0.04), 0), -90, STEP),
    (C2, V(TIGER_X, tiger_y0 - TIGER_SPEED * C2, 0), -90, LIN),
    (C3 - 0.05, V(TIGER_X, tiger_y0 - TIGER_SPEED * (C3 - 0.05), 0), -90, STEP),
    (C3, V(-5150, -3400, 0), -90, STEP),                            # out of sight during cut 3
    (C3E, V(-5300, ALLEY_Y, 0), 180, LIN),                          # 3-끝: at the alley's mouth, walking in
    (C4 - 0.04, V(-5300 - WALK * (C4 - 0.04 - C3E), ALLEY_Y, 0), 180, STEP),
    (C4, V(X4, ALLEY_Y, 0), 180, LIN),                              # 4 and 4-2: on without stopping, right up to the fox
    (T_NEAR, NEAR, 180, LIN),
    (T_LAND + 0.05, NEAR, 180, LIN),
] + skid + [
    (T_POOF + 0.6, TIGER_SPOT, 180, LIN),
    (T_POOF + 1.1, TIGER_SPOT, 140, LIN),                           # looks round at the foxes
    (C8 + 0.4, TIGER_SPOT, 225, LIN),
    (C8 + 1.2, TIGER_SPOT, 180, LIN),
    (END_T, TIGER_SPOT, 180, LIN),
]
transform_keys(tb, tiger_keys)
HIT_AT, BLEND = 0.56, 0.15
SWING_LEN = anim("Tiger", "Cine_RunHammer1").get_play_length()
A1 = T_SMASH - HIT_AT
anim_sections(tiger, "Tiger", [
    (0.0, A1 + BLEND, "Cine_SsRunB", 1.0, 0.0, BLEND),
    (A1, A1 + SWING_LEN, "Cine_RunHammer1", 1.0, BLEND, BLEND),
    (A1 + SWING_LEN - BLEND, C3E, "Cine_SsRunB", 1.0, BLEND, 0.0),
    (C3E, T_R0 + 0.25, "Walk", 0.87, 0.0, 0.25),
    (T_R0, T_SWING, "Board_Raise", 1.0, 0.25, 0.0),                 # the hammer up, slowly, and held
    (T_SWING, T_SWING + 1.7, "Board_Bonk", 1.0, 0.0, 0.3),          # down on the die, thrown back, lowered
    (T_SWING + 1.4, T_LAND + 0.1, "Board_Watch", 1.0, 0.3, 0.1),    # leaning in, watching the die
    (T_LAND, T_LAND + 0.45, "Cine_SsImpactA", 1.0, 0.1, 0.15),
    (T_LAND + 0.3, T_STOP + 0.2, "Board_Shield", 1.0, 0.15, 0.2),    # skidding back behind his shield
    (T_STOP, T_POOF + 0.2, "Cine_SsIdleC", 1.0, 0.2, 0.2),
    (T_POOF, T_CROUCH, "Cine_LookNervous", 1.3, 0.2, 0.0),
    (T_CROUCH, T_CROUCH + 0.5, "Cine_SsCrouchblock", 1.0),
    (T_CROUCH + 0.5, LENGTH, "Cine_SsCrouchblockIdle", 1.0)])
bool_keys(tb, "bHoldHammer", [(T_DROP, False)], True)
# smug; startled as his hammer bounces; nervous watching; smug again as the die leans to the 1; startled at the 5
int_keys(tb, "Expression", [(T_BONK + 0.05, 1), (T_BONK + 0.9, 2), (T_ONE + 0.1, 0), (T_FIVE + 0.06, 1), (T_POOF + 0.6, 2)], 0)
# 4-3's two looks at the fox's face are from where he stands: he is out of them
visible_keys(tb, [(S43[1], False), (S43[2], True), (S43[3], False), (C51, True)], True)
log(f"tiger: from Y {tiger_y0:.0f} at {TIGER_SPEED:.0f} cm/s, blow lands at {T_SMASH:.2f} s; over the fox at {T_NEAR:.2f}, hammer up {T_R0 + 1.8:.2f}, blow {T_SWING:.2f}, "
    f"on the die {T_BONK:.2f}; pushed back to {T_STOP:.2f}; drop {T_DROP:.2f}, crouch {T_CROUCH:.2f}")

db = seq.add_possessable(dropped)
transform_keys(db, [(T_DROP, HAMMER_REST + V(30, -40, 80), unreal.Rotator(40, 20, 80), LIN),
                    (T_DROP + 0.3, HAMMER_REST, unreal.Rotator(0, 0, 35), LIN), (END_T, HAMMER_REST, unreal.Rotator(0, 0, 35), LIN)])
visible_keys(db, [(T_DROP, True)], False)

# ------------------------------------------------------------------------------------------------- cameras
# (name, [(time, position, target, focal length)], aperture, exposure bias)
fox_at = lambda t: V(LANE_X, FOX_START.y - RUN_SPEED * t, 0.0)   # noqa: E731 - the fox on the road in cut 1
CAM2_X = LANE_X + 415.0                             # (the town's big pink die beside the road has its face at X -4499)
FACE_MID = SIT + V(48.0, 10.0, 50.0)                # the middle of the sitting fox's face as it shrinks (its eyes are about 68 high sitting up, 55 bowed)
CAMS = [
    # 1: ahead of the fox, backing away at its speed. A long lens on the face; after the look back it zooms out and
    # drops back, and the tiger shows behind, out of focus
    ("Cam1_Chase", [(0.0, fox_at(0.0) + V(115, -315, 104), fox_at(0.0) + V(0, 0, 102), 55.0),
                    (1.7, fox_at(1.7) + V(115, -315, 104), fox_at(1.7) + V(0, 0, 102), 55.0),
                    (3.3, fox_at(3.3) + V(35, -470, 112), fox_at(3.3) + V(-60, 60, 90), 30.0),
                    (C2, fox_at(C2) + V(35, -470, 112), fox_at(C2) + V(-60, 60, 90), 30.0)], 1.4, 0.0),
    # 2: side on from across the road, low, going along with the fox as it runs the obstacle's top: the one height
    # reads as a line; it widens at the end to take in the tiger's blow on the obstacle behind
    # (2026-11-24: it used to wait and then rise over the pink die and whip down: half a second of blur where the
    # fox runs the top. The die's face is 65 behind this line: it stays low and starts with the fox.)
    ("Cam2_Obstacle", [(C2, V(CAM2_X, OB_FRONT + 130, 175), V(LANE_X, OB_FRONT + 20, 80), 16.0),
                       (T_M + 0.2, V(CAM2_X, OB_FRONT + 130, 175), V(LANE_X, OB_FRONT + 20, 80), 16.0),
                       (T_RO, V(CAM2_X, OB_BACK + 60, 175), V(LANE_X, OB_BACK + 60, 80), 16.0),
                       (T_SMASH - 0.2, V(CAM2_X, OB_MID.y - 120, 175), V(LANE_X, OB_MID.y - 120, 80), 11.5),
                       (C3, V(CAM2_X, OB_MID.y - 120, 175), V(LANE_X, OB_MID.y - 120, 80), 11.5)], 4.0, 0.0),
    # 3: low in the alley, beside the fox's way to the wall
    ("Cam3_DeadEnd", [(C3, V(-5840, ALLEY_Y + 150, 62), V(-6150, ALLEY_Y - 10, 86), 18.0)], 2.8, -0.6),
    # 3-끝: straight down (a little off, for the camera's up): the fox boxed in at the left, the alley's mouth at the right
    ("Cam3E_Top", [(C3E, V(-5800, ALLEY_Y + 150, 900), V(-5800, ALLEY_Y, 0), 18.0)], 5.6, -1.0),
    # 4: from beside the sitting fox (the tiger's hammer side: his shield hides his face from the other), low, up the
    # alley at the tiger coming out of the dark
    ("Cam4_Tiger", [(C4, V(-6080, ALLEY_Y - 85, 55), V(-5500, ALLEY_Y + 5, 120), 26.0)], 2.8, -1.7),
    # 4-2: over the fox's head from behind and low, pushing in (the camera is where the dead-end wall is: the wall is
    # taken away for this cut, as in cut 6)
    ("Cam42_Loom", [(C42, V(-6345, ALLEY_Y - 112, 108), V(-5960, ALLEY_Y + 30, 98), 20.0),
                    (C43, V(-6325, ALLEY_Y - 108, 112), V(-5960, ALLEY_Y + 30, 110), 20.0)], 2.8, -1.2),
    # 4-3: from the fox's eyes up at him, tilting up with the hammer (the fox itself is hidden in these two shots)...
    # (2026-11-24: the hammer now goes straight up over his head: the tilt goes on up after it)
    ("Cam43_Up", [(S43[0], SIT + V(34, -6, 68), NEAR + V(0, 0, 120), 12.0), (S43[1], SIT + V(34, -6, 68), NEAR + V(0, 0, 150), 12.0),
                  (S43[2], SIT + V(34, -6, 68), NEAR + V(0, 0, 172), 12.0), (C51, SIT + V(34, -6, 68), NEAR + V(0, 0, 180), 12.0)], 2.8, -1.0),
    # ...and the fox's face, a long lens from straight in front and low, up under its bowed head (the camera is behind
    # where the tiger stands: he is hidden in these two shots). Both eyes, brows to nose; it creeps in.
    # The whole face (its head sways as it trembles: tighter than this the eyes wander out of the frame).
    ("Cam43_Eyes", [(S43[1], SIT + V(232, 10, 30), FACE_MID, 42.0), (S43[2], SIT + V(224, 10, 30), FACE_MID, 42.0),
                    (S43[3], SIT + V(214, 10, 30), FACE_MID + V(0, 0, -6), 37.0), (C51, SIT + V(204, 10, 30), FACE_MID + V(0, 0, -6), 37.0)], 5.6, -0.4),
    # 5-1: side on from the alley's wall (it is only 3.65 m wide: a short lens): hammer - die - fox in a line. From his
    # hammer side, as cut 4: from the other his shield covers him (so the fox is at the right, the sketches' mirror)
    ("Cam51_Bonk", [(C51, SIT + V(66, -168, 96), SIT + V(62, 0, 126), 10.5)], 4.0, -0.8),
    # 5-2: the same side, lower, looking up: the fox's face, the tiger's, the die over them
    ("Cam52_Watch", [(C52, SIT + V(70, -168, 46), SIT + V(66, 0, 112), 12.0)], 4.0, -0.6),
    # 5-3: from the dead end's corner (the wall is away again), the fox and the die near, the tiger skidding off down the alley
    ("Cam53_Burst", [(C53, V(-6300, ALLEY_Y - 130, 150), V(-5900, ALLEY_Y + 20, 90), 18.0)], 4.0, -0.6),
    # 6: high over the fox's shoulder from behind (the dead-end wall is taken away for this cut: the camera hangs
    # where it is), down the length of the alley: the oval of foxes round the tiger, who faces the lens
    ("Cam6_Clones", [(C6, V(-6610, ALLEY_Y + 150, 585), V(-5890, ALLEY_Y, 0), 17.0)], 5.6, -0.6),
    # 8: straight down, the alley's length across the frame: the oval closing to the ring
    ("Cam8_CloseIn", [(C8, V(TIGER_SPOT.x - 20, ALLEY_Y + 150, 880), V(TIGER_SPOT.x - 20, ALLEY_Y, 0), 18.0)], 5.6, -0.8),
    # 9: as the real cinematic's last cut
    ("Cam9_Grin", [(C9, V(TIGER_SPOT.x + 135, ALLEY_Y + 140, 92), V(TIGER_SPOT.x - 170, ALLEY_Y, 112), 40.0),
                   (END_T, V(TIGER_SPOT.x + 90, ALLEY_Y + 126, 90), V(TIGER_SPOT.x - 170, ALLEY_Y, 112), 40.0)], 2.8, -0.6),
]
SHOTS = [(C1, "Cam1_Chase"), (C2, "Cam2_Obstacle"), (C3, "Cam3_DeadEnd"), (C3E, "Cam3E_Top"), (C4, "Cam4_Tiger"), (C42, "Cam42_Loom"),
         (S43[0], "Cam43_Up"), (S43[1], "Cam43_Eyes"), (S43[2], "Cam43_Up"), (S43[3], "Cam43_Eyes"), (C51, "Cam51_Bonk"), (C52, "Cam52_Watch"),
         (C53, "Cam53_Burst"), (C6, "Cam6_Clones"), (C8, "Cam8_CloseIn"), (C9, "Cam9_Grin"), (END_T, None)]
# (RR_CINE_TOP=1: a check of where everybody really stands - the alley part from straight above, close, the lights on)
DEBUG_TOP = bool(os.environ.get("RR_CINE_TOP"))
if DEBUG_TOP:
    CAMS.append(("CamDebug_Top", [(C42, SIT + V(170, 60, 520), SIT + V(170, 0, 0), 18.0)], 5.6, 0.0))
    SHOTS = [sh for sh in SHOTS if not C42 <= sh[0] < C6] + [(C42, "CamDebug_Top")]
    SHOTS.sort(key=lambda sh: sh[0])
layout = {"fps": FPS, "cuts": {"1": [C1, C2], "2": [C2, C3], "3": [C3, C3E], "3e": [C3E, C4], "4": [C4, C42], "4-2": [C42, C43], "4-3": [C43, C51], "5-1": [C51, C52],
                                "5-2": [C52, C53], "5-3": [C53, C6], "6": [C6, C8], "8": [C8, C9], "9": [C9, END_T]}, "shots": [[t, n] for t, n in SHOTS],
          "cams": {}}
cams = []
for name, keys, aperture, bias in CAMS:
    _, pos, target, mm = keys[0]
    cam = spawn(unreal.CineCameraActor, f"Cine_{name}", pos, look_rot(pos, target))
    comp = cam.get_cine_camera_component()
    comp.set_editor_property("current_focal_length", mm)
    focus = comp.get_editor_property("focus_settings")
    focus.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
    focus.set_editor_property("manual_focus_distance", (target - pos).length())
    comp.set_editor_property("focus_settings", focus)
    comp.set_editor_property("current_aperture", aperture)
    if abs(bias) > 1e-3:
        pp = comp.get_editor_property("post_process_settings")
        pp.set_editor_property("override_auto_exposure_bias", True)
        pp.set_editor_property("auto_exposure_bias", bias)
        comp.set_editor_property("post_process_settings", pp)
    cams.append((cam, keys))
    layout["cams"][name] = [[t, p.x, p.y, p.z, tg.x, tg.y, tg.z, mm] for t, p, tg, mm in keys]
# ------------------------------------------------------------------------------------------------- the die
gb = seq.add_possessable(die)
die_cam = next(keys for name, keys, _, _ in CAMS if name == "Cam52_Watch")[-1]
die_look = look_rot(die_cam[1], die_cam[2])
die_final = unreal.RRRandomCineLibrary.die_face_rotation(DIE_FACE, die_look)
die_base = die.get_actor_scale3d().x


def ease_out_back(x):
    x = min(1.0, max(0.0, x))
    c1 = 1.70158
    return 1.0 + (c1 + 1.0) * (x - 1.0) ** 3 + c1 * (x - 1.0) ** 2


def smooth(x):
    x = min(1.0, max(0.0, x))
    return x * x * (3.0 - 2.0 * x)


def face_between(u):
    """The die's rotation between showing the camera its 1 (u = 0) and its 5 (u = 1): at 0.5 an edge is to the camera."""
    a = unreal.RRRandomCineLibrary.die_face_rotation(1, die_look).quaternion()
    b = die_final.quaternion()
    dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w
    sign = -1.0 if dot < 0 else 1.0
    th = math.acos(min(1.0, abs(dot)))
    if th < 1e-4:
        return die_final
    wa, wb = math.sin((1.0 - u) * th) / math.sin(th), sign * math.sin(u * th) / math.sin(th)
    return unreal.Quat(wa * a.x + wb * b.x, wa * a.y + wb * b.y, wa * a.z + wb * b.z, wa * a.w + wb * b.w).rotator()


# The die pops up under the blow, a little in front of the fox's head; the hammer knocks it down a hand and sets it
# spinning; it drifts back over the fox's head, slows, stands on an edge and rocks, leans to the 1, tips over to the 5
DIE_POP = SIT + V(66.0, 0, DIE_HEIGHT)
DIE_REST = SIT + V(22.0, 0, DIE_HEIGHT)
edge = face_between(0.5)
die_keys = []
for i in range(int(round((T_LAND + 0.4 - T_DIE) * FPS)) + 1):
    t = T_DIE + i / FPS
    since = t - T_BONK
    loc, rot, scale = DIE_POP, edge, max(0.02, ease_out_back((t - T_DIE) / 0.12))
    if since >= 0.0:
        scale = 1.0
        loc = DIE_POP + (DIE_REST - DIE_POP) * smooth(since / 0.7) + V(0, 0, -26.0 * math.exp(-since * 5.0) * math.cos(since * 13.0))
        if t < T_ROCK:
            left = (1.0 - since / (T_ROCK - T_BONK)) ** 3
            rot = unreal.Rotator(edge.roll + 360.0 * 3.0 * left, edge.pitch + 360.0 * 5.0 * left, edge.yaw + 360.0 * 2.0 * left)
        elif t < T_ONE:
            k = smooth((t - T_ROCK) / (T_ONE - T_ROCK))
            rot = face_between(0.5 + (0.14 - 0.5) * k + 0.2 * (1.0 - k) * math.sin(2.0 * math.pi * (t - T_ROCK) / 0.42))
        elif t < T_FIVE - 0.1:
            rot = face_between(0.14 + 0.04 * math.sin(2.0 * math.pi * (t - T_ONE) / 0.33))
        else:
            rot = face_between(0.14 + 0.86 * ease_out_back((t - T_FIVE + 0.1) / 0.3))
        if t >= T_LAND:
            scale = 1.0 + 0.28 * math.sin(math.pi * min(1.0, (t - T_LAND) / 0.22))
    die_keys.append((t, loc, rot, LIN, die_base * scale))
transform_keys(gb, die_keys)
visible_keys(gb, [(T_DIE, True), (T_LAND + 0.35, False)], False)

# ------------------------------------------------------------------------------------------------- lights (rough)
def point(label, loc, cd, radius):
    a = spawn(unreal.PointLight, label, loc)
    c = a.get_component_by_class(unreal.PointLightComponent)
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_intensity(cd)
    c.set_attenuation_radius(radius)
    c.set_cast_shadows(False)
    c.set_light_color(unreal.LinearColor(1.0, 0.8, 0.62, 1.0))
    return a


def spot(label, loc, target, cd, radius, outer, inner, shadows, color=(1.0, 0.86, 0.74)):
    a = spawn(unreal.SpotLight, label, loc, look_rot(loc, target))
    c = a.get_component_by_class(unreal.SpotLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_intensity(cd)
    c.set_light_color(unreal.LinearColor(color[0], color[1], color[2], 1.0))
    c.set_attenuation_radius(radius)
    c.set_editor_property("outer_cone_angle", outer)
    c.set_editor_property("inner_cone_angle", inner)
    c.set_cast_shadows(shadows)
    return a


fill = point("Cine_FillLight", V(TIGER_SPOT.x - 40, ALLEY_Y, 420), 120.0, 1500.0)
point("Cine_RoadFill", V(-4900, -3500, 450), 150.0, 1800.0)
point("Cine_RoadFill2", V(-4860, -2500, 450), 170.0, 1800.0)
point("Cine_RoadFill3", V(-4860, FOX_START.y - 250.0, 450), 170.0, 1800.0)
point("Cine_RoadFill4", V(-4860, FOX_START.y - 850.0, 450), 170.0, 1800.0)
# cut 4: the alley dark (no fill), a low warm light behind the tiger in the alley's mouth (his outline and long shadow),
# and one pool of light where he stops: he walks out of the dark into it and his face shows
shadow_light = spot("Cine_ShadowLight", V(-5150, ALLEY_Y + 120, 300), V(-5900, ALLEY_Y - 100, 0), 5000.0, 2600.0, 30.0, 14.0, True, (1.0, 0.66, 0.42))
pool = spot("Cine_TigerPool", V(TIGER_SPOT.x - 190, ALLEY_Y + 40, 330), TIGER_SPOT + V(-10, 0, 120), 900.0, 900.0, 15.0, 8.0, False)
# (off in 4-3's two looks at the fox's face: the tiger is hidden in them, and without him in its way it burned the face out)
visible_keys(seq.add_possessable(shadow_light), [(C4, True), (S43[1], False), (S43[2], True), (S43[3], False), (C51, True), (C53, False)], False)
visible_keys(seq.add_possessable(pool), [(C4, True), (C43, False)], False)
# cut 4: a narrow, shadowless light from the fox's side on his chest and face: past the pool of light his shield, held
# to the lens, was a black shape over him (the film before had the same light for his walk in)
tiger_face = spot("Cine_TigerFaceLight", V(-6120, ALLEY_Y - 60, 150), V(-5640, ALLEY_Y, 112), 320.0, 1200.0, 11.0, 6.0, False)
visible_keys(seq.add_possessable(tiger_face), [(C4, True), (C42, False)], False)
visible_keys(seq.add_possessable(fill), [(C3E, False), (C42 if DEBUG_TOP else C53, True)], True)
# 3-끝: a narrow low light from the road straight down the alley: his shadow in a strip of light. It sinks during the
# shot, so the shadow grows along the floor to the fox
LONG_FROM, LONG_TO = V(-4990, ALLEY_Y, 430), V(-6200, ALLEY_Y, 20)
long_light = spot("Cine_LongShadow", LONG_FROM, LONG_TO, 9000.0, 3400.0, 9.0, 5.0, True, (1.0, 0.7, 0.45))
lb = seq.add_possessable(long_light)
transform_keys(lb, [(C3E, LONG_FROM, look_rot(LONG_FROM, LONG_TO), LIN), (C4, V(LONG_FROM.x, ALLEY_Y, 178), look_rot(V(LONG_FROM.x, ALLEY_Y, 178), LONG_TO), LIN)])
visible_keys(lb, [(C3E, True), (C4, False)], False)
spot("Cine_FoxDim", SIT + V(260, 90, 150), SIT + V(0, 0, 95), 45.0, 800.0, 20.0, 10.0, False, (0.8, 0.86, 1.0))
# 4-2 to 5-1: a dim cool light high at the dead end, so the tiger standing over the fox is more than a black shape
near_fill = point("Cine_NearFill", SIT + V(-10, -70, 240), 34.0, 700.0)
near_fill.get_component_by_class(unreal.PointLightComponent).set_light_color(unreal.LinearColor(0.75, 0.82, 1.0, 1.0))
visible_keys(seq.add_possessable(near_fill), [(C42, True), (C53, False)], False)
# cut 5: a soft light on the fox's face (gold, as from the die)
die_light = spot("Cine_DieLight", SIT + V(170, 60, 230), SIT + V(0, 0, 95), 240.0, 700.0, 22.0, 10.0, False, (1.0, 0.85, 0.5))
visible_keys(seq.add_possessable(die_light), [(T_DIE, True), (C6, False)], False)
key_light = spot("Cine_FaceKey", V(TIGER_SPOT.x + 80, ALLEY_Y + 150, 175), V(TIGER_SPOT.x - 170, ALLEY_Y, 100), 38.0, 900.0, 14.0, 6.0, False, (1.0, 0.9, 0.8))
visible_keys(seq.add_possessable(key_light), [(C9, True)], False)
# cut 6: the wall is out of the camera's way
visible_keys(seq.add_possessable(walls), [(C42, False), (C43, True), (C53, False), (C8, True)], True)

# ------------------------------------------------------------------------------------------------- camera cuts
cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
cam_bind = {}
for (name, _, _, _), (cam, keys) in zip(CAMS, cams):
    cb = seq.add_possessable(cam)
    cam_bind[name] = cb
    if len(keys) > 1:
        # (the yaws are unwrapped: a pan from -165 to 180 degrees is 15 degrees, not all the way round the other way -
        # the draft's whip in the middle of cut 2 was that)
        rots, prev = [], None
        for _, p, tg, _ in keys:
            r = look_rot(p, tg)
            if prev is not None:
                r = unreal.Rotator(r.roll, r.pitch, prev + (r.yaw - prev + 180.0) % 360.0 - 180.0)
            prev = r.yaw
            rots.append(r)
        transform_keys(cb, [(t, p, r, LIN) for (t, p, _, _), r in zip(keys, rots)])
        comp_b = seq.add_possessable(cam.get_cine_camera_component())
        channel_keys(comp_b, unreal.MovieSceneFloatTrack, "CurrentFocalLength", "CurrentFocalLength", [(t, mm) for t, _, _, mm in keys], keys[0][3], LIN)
        channel_keys(comp_b, unreal.MovieSceneFloatTrack, "ManualFocusDistance", "FocusSettings.ManualFocusDistance",
                     [(t, (tg - p).length()) for t, p, tg, _ in keys], (keys[0][2] - keys[0][1]).length(), LIN)
for (t0, name), (t1, _) in zip(SHOTS, SHOTS[1:]):
    s = cut_track.add_section()
    s.set_range(f(t0), f(t1))
    s.set_camera_binding_id(seq.get_binding_id(cam_bind[name]))

# ------------------------------------------------------------------------------------------------- fades
if not os.environ.get("RR_CINE_NOFADE"):
    fade = seq.add_track(unreal.MovieSceneFadeTrack)
    fs = fade.add_section()
    fs.set_range(0, END)
    fch = fs.get_all_channels()[0]
    # (a key a frame: the fade darkens the light before the picture is toned for the screen, so a straight ramp looked
    # bright nearly to the end and then fell off a cliff - and the last frame rendered is END - 1, which was not black.
    # The amounts are bent (^2.2) so the picture on the screen dims evenly, to black on the last frame.)
    n_in, n_out = f(FADE_IN), f(FADE_OUT)
    for k in range(n_in + 1):
        fch.add_key(unreal.FrameNumber(k), 1.0 - (k / n_in) ** 2.2, interpolation=LIN)
    for k in range(n_out):
        fch.add_key(unreal.FrameNumber(END - n_out + k), 1.0 - (1.0 - k / (n_out - 1)) ** 2.2, interpolation=LIN)
    log(f"fades: in {FADE_IN} s, out {FADE_OUT} s (ends at {END_T} s)")

# ------------------------------------------------------------------------------------------------- effects
seq_actor = spawn(unreal.LevelSequenceActor, "Cine_SequenceActor", V(-5700, ALLEY_Y, 0))
seq_actor.set_sequence(seq)
director = spawn(unreal.RRRandomCineDirector, "Cine_Director", V(-5700, ALLEY_Y, 0))
director.set_editor_property("sequence", seq_actor)
director.set_editor_property("gold_die", die)


def cue(t, kind, a=V(0, 0, 0), b=V(0, 0, 0), size=1.0, duration=0.0):
    c = unreal.RRCineCue()
    c.set_editor_property("time", t)
    c.set_editor_property("duration", duration)
    c.set_editor_property("kind", kind)
    c.set_editor_property("a", a)
    c.set_editor_property("b", b)
    c.set_editor_property("size", size)
    return c


K = unreal.RRCineEffect
cues = [
    cue(T_SMASH, K.SMASH, OB_MID, OB_MID + V(0, 300, 0), 330.0),
    cue(C3 + 0.6, K.WRECK, V(-5420, -4200, 40), V(-5420, -4200, 200), 150.0),
    cue(C3 + 0.6, K.WRECK, V(-5540, -3790, 40), V(-5540, -3790, 200), 150.0),
    cue(T_BONK, K.FOX_POOF, DIE_POP + V(18, 0, 16), size=0.3),                        # the squeaky hammer's "뿅"
    cue(T_BONK, K.DIE_SPIN, DIE_REST, SIT + V(-45, 0, 0), DIE_ACROSS * 0.5, DIE_SPIN),
    cue(T_LAND, K.DIE_BURST, DIE_REST, SIT + V(-45, 0, 0), DIE_ACROSS * 0.5),
    cue(T_POOF, K.FOX_POOF, stood + V(0, 0, 80), size=1.1),
]
for w in clone_wide:
    cues.append(cue(T_POOF, K.FOX_POOF, w + V(0, 0, 60), size=0.7))
# 5-3: dust at his heels as the burst pushes him back down the alley
for kt, kp, *_ in skid[2::5]:
    cues.append(cue(kt, K.FOX_POOF, kp + V(-30, 0, 14), size=0.22))
director.set_editor_property("cues", cues)

# ------------------------------------------------------------------------------------------------- the paths, for the page
layout["fox"] = [[t, k.x, k.y] for t, k, *_ in fox_path]
layout["tiger"] = [[t, k.x, k.y] for t, k, *_ in tiger_keys]
layout["clones"] = [[w.x, w.y, ring_spot(a).x, ring_spot(a).y] for w, a in zip(clone_wide, CLONE_ANGLES)]
layout["obstacle"] = [LANE_X - 250.0, OB_BACK, LANE_X + 250.0, OB_FRONT]
layout["wall_x"] = WALL_X + 50.0
layout["times"] = {"mantle": [T_M, T_ME], "off": T_RO, "down": T_ROLL_DOWN, "smash": T_SMASH, "hit": T_HIT, "turn": T_TURN, "sit": T_FLOOR, "up": T_UP, "step6": T_STEP6, "tiger_stop": T_STOP,
                   "die": T_DIE, "bonk": T_BONK, "rock": T_ROCK, "one": T_ONE, "five": T_FIVE, "near": T_NEAR, "raise": [T_R0 + 0.3, T_R0 + 1.8], "cower": T_COWER, "look": T_LOOK, "land": T_LAND, "tiger_back": T_STOP, "poof": T_POOF, "flinch": T_FLINCH, "steps": STEPS, "drop": T_DROP, "crouch": T_CROUCH}
layout["spots"] = {"near": [NEAR.x, NEAR.y], "tiger": [TIGER_SPOT.x, TIGER_SPOT.y], "sit": [SIT.x, SIT.y], "fox_ring": [fox_spot.x, fox_spot.y], "fox_wide": [fox_wide.x, fox_wide.y]}
layout["wide"] = [2.0 * WIDE_END, 2.0 * WIDE_B]
import os as _os
_os.makedirs(_os.path.dirname(OUT_JSON), exist_ok=True)
with open(OUT_JSON, "w", encoding="utf-8") as fh:
    json.dump(layout, fh, indent=1)

eal.save_loaded_asset(seq)
les.save_current_level()
log(f"saved {MAP} and {FOLDER}/{SEQ_NAME}: {len(cams)} cameras, {len(cues)} cues")
log("DONE")
