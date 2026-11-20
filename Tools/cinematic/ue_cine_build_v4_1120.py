# Builds the fox vs tiger blocking (2026-11-19): the set, the cast, seven cameras and the Level Sequence.
# Run with UnrealEditor-Cmd -run=pythonscript -script=<this>. Safe to run again: it rebuilds everything labelled Cine_*
# and the sequence from scratch. Plan: RRRandom/Docs/CinematicSequencePlan.md (sections 8-9).
#
# The set is the game's Dice Town (a copy of /Game/Maps/DiceTown, same town), candidate "??: the alley between the two purple
# square buildings off the south ring road. World: X up the minimap, the ring road runs along Y at X about -4930, the alley
# goes in at Y -3905 (between Y -4120 and -3690) toward -X; a wall of die blocks closes it at X -6250. Feet at Z 0.
import math
import random

import unreal

FPS = 24
# 2026-11-20 (sixth pass, the user: the remade fox and tiger go in, and in the chase the fox vaults the obstacles
# (parkour) while the tiger smashes through them as before): the chase is 5 s instead of 2, everything after it is
# S seconds later than in the fifth pass (times below written as S + the old time)
S = 3.0
LENGTH = 14.0 + S
LANE_X = -4980.0                                   # the middle of the ring road the two run down (toward -Y)
FOX_SCALE = 1.8                                    # the fox's MeshScale (RRRandomSpecies), for the hurdle clips' root motion
MAP_SRC = "/Game/Maps/DiceTown"
FOLDER = "/Game/Cinematics/FoxVsTiger"
MAP = FOLDER + "/Map_FoxVsTiger"
SEQ_NAME = "LS_FoxVsTiger"
ALLEY_Y = -3995.0                                   # re-measured: clear between Y -4177 and -3812
WALL_X = -6250.0
TIGER_SPOT = unreal.Vector(-5680.0, ALLEY_Y, 0.0)   # where the tiger ends up, the ring's middle
DIE_SPOT = unreal.Vector(-5870.0, ALLEY_Y, 0.0)
RING = 170.0
FOX_ANGLE = 180.0                                   # the real fox's place in the ring (between the tiger and the wall)
CLONE_ANGLES = [0.0, 60.0, 120.0, 240.0, 300.0]
DIE_ACROSS = 40.0
HAMMER_REST = unreal.Vector(-5760.0, ALLEY_Y + 75.0, 18.0)   # where the dropped hammer lies (in front of the tiger)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


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


def ring_spot(angle):
    r = math.radians(angle)
    return unreal.Vector(TIGER_SPOT.x + RING * math.cos(r), TIGER_SPOT.y + RING * math.sin(r), 0.0)


def yaw_to(src, dst):
    return math.degrees(math.atan2(dst.y - src.y, dst.x - src.x))


# ---------------------------------------------------------------------------------------------------------------- map
if not eal.does_asset_exist(MAP):
    eal.duplicate_asset(MAP_SRC, MAP)
    log(f"map copied to {MAP}")
les.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
# No game rules here: no bots, no HUD, no match (the effects still run, they need only a game world)
world.get_world_settings().set_editor_property("default_game_mode", unreal.GameModeBase)

# Golden hour (final pass, the recommended look): one warm, low sun for the whole short, set in the alley's mouth (+X), so
# cut 3 is backlit as in the storyboard and the sky (SkyAtmosphere) turns to sunset on its own
SUN_PITCH, SUN_YAW = -9.0, 172.0
# (fifth pass: a lower sun for cut 3's long shadow was tried, pitch -4: the buildings across the ring road then shade the
# whole alley floor, so the shadow comes from a low light in the alley's mouth instead, Cine_ShadowLight below)
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.DirectionalLight):
        a.set_actor_rotation(unreal.Rotator(0.0, SUN_PITCH, SUN_YAW), False)
        sun = a.get_component_by_class(unreal.DirectionalLightComponent)
        sun.set_light_color(unreal.LinearColor(1.0, 0.72, 0.48, 1.0))
        log(f"sun {a.get_actor_label()}: pitch {SUN_PITCH}, yaw {SUN_YAW}, intensity {sun.get_editor_property('intensity')}")

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
fox = spawn(unreal.RRRandomCineAnimal, "Cine_Fox", unreal.Vector(-4980, -2500, 0))
fox.set_editor_property("species", 0)
clones = []
for i, ang in enumerate(CLONE_ANGLES):
    c = spawn(unreal.RRRandomCineAnimal, f"Cine_Clone{i + 1}", ring_spot(ang))
    c.set_editor_property("species", 0)
    clones.append(c)
tiger = spawn(unreal.RRRandomCineAnimal, "Cine_Tiger", unreal.Vector(-5150, -2450, 0))
tiger.set_editor_property("species", 3)
tiger.set_editor_property("hold_hammer", True)
tiger.set_editor_property("hold_shield", True)
# The faces (sixth pass): the remade fox and tiger are the game's own blank-face models, so no face masks any more: an
# expression is a copy of the body material with the old expression pictures carried onto the new texture
# (char-pipeline: blender_image_reframe.py / blender_feature_transfer.py / blender_expression.py bake for the fox,
# the old expression textures through blender_face_carry3d.py for the tiger; Tools/cinematic/ue_cine_expressions.py)
EXPR = "/Game/Cinematics/FoxVsTiger/Expressions/"
FOX_FACES = [unreal.load_asset(EXPR + f"M_Fox_Expr_{n}") for n in ("Scared", "Worried", "Determined", "Grin", "WorriedBlink", "GrinBlink")]
TIGER_FACES = [unreal.load_asset(EXPR + f"M_Tiger_Expr_{n}") for n in ("Smug", "Startled", "Nervous")]
log(f"faces: fox {[bool(m) for m in FOX_FACES]}, tiger {[bool(m) for m in TIGER_FACES]}")
FOX_FURS = [unreal.load_asset(EXPR + f"M_FoxFur_Expr_{n}") for n in ("Scared", "Worried", "Determined", "Grin", "WorriedBlink", "GrinBlink")]
TIGER_FURS = [unreal.load_asset(EXPR + f"M_TigerFur_Expr_{n}") for n in ("Smug", "Startled", "Nervous")]
log(f"fur faces: fox {[bool(m) for m in FOX_FURS]}, tiger {[bool(m) for m in TIGER_FURS]}")
for a in [fox] + clones:
    a.set_editor_property("expressions", FOX_FACES)
    a.set_editor_property("expression_furs", FOX_FURS)
tiger.set_editor_property("expressions", TIGER_FACES)
tiger.set_editor_property("expression_furs", TIGER_FURS)
# The fox's parkour: the Game Animation Sample's running hurdles, retargeted onto SK_FoxMM (the fox's rig + a root
# bone, char-pipeline/scripts/mm_gasp_retarget_extra.py). The game fox's skeleton takes them as a compatible skeleton
# (same bone names; the root bone's track has no bone here, so the clip plays in place and the actor is keyed along
# the clip's root motion, hurdle_keys below)
fox_skel = unreal.load_asset("/Game/Characters/Fox/SK_Fox_Skeleton")
mm_skel = unreal.load_asset("/Game/Characters/FoxMM/SK_FoxMM_Skeleton")
try:
    fox_skel.add_compatible_skeleton(mm_skel)
    eal.save_loaded_asset(fox_skel)
    log("fox skeleton: SK_FoxMM_Skeleton added as compatible")
except Exception as e:  # noqa: BLE001
    log(f"compatible skeleton failed: {e}")

# The hammer lying where the tiger drops it (cut 6)
dropped = spawn(unreal.StaticMeshActor, "Cine_HammerDropped", HAMMER_REST, unreal.Rotator(0, 0, 35))
dropped.static_mesh_component.set_static_mesh(unreal.load_asset("/Game/Props/Weapons98/SM_Hammer"))
dropped.set_actor_scale3d(unreal.Vector(0.86, 0.86, 0.86))

# The golden ultimate die (the game's die mesh with the gold body, OverheadDieComponent::SetGold)
die = spawn(unreal.StaticMeshActor, "Cine_GoldDie", DIE_SPOT + unreal.Vector(0, 0, 900))
die_mesh = unreal.load_asset("/Game/Props/Dice/SM_Dice")
die.static_mesh_component.set_static_mesh(die_mesh)
gold = unreal.load_asset("/Game/Props/Dice/M_DiceGold")
for i, slot in enumerate(die.static_mesh_component.get_material_slot_names()):
    if "Pip" not in str(slot) and gold:
        die.static_mesh_component.set_material(i, gold)
die.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
bounds = die_mesh.get_bounds()
die_size = max(bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z) * 2.0
# (the user: half the first blocking's 80 across; the landing flash keeps its size)
die_scale = DIE_ACROSS / max(die_size, 1.0)
die.set_actor_scale3d(unreal.Vector(die_scale, die_scale, die_scale))
log(f"die mesh size {die_size:.1f}, scale {die_scale:.2f}")

# The dead end, in the town's own blocks and colors (ARRRandomCineBlocks, like ARRRandomDiceTown::AddColored): a back wall
# 7 blocks high across the alley and wider than it, and a side wall on the left where the left building stops short and
# the grass behind showed. Candy plain blocks with white tops, a third of them dice (the town's die colors and pips).
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
wall = []


def add_block(x, y, level):
    b = unreal.RRCineBlock()
    b.set_editor_property("location", unreal.Vector(x, y, 50.0 + 100.0 * level))
    if rng.random() < 0.33:
        body, accent = rng.choice(DIE_LOOKS)
        b.set_editor_property("die", True)
    else:
        body, accent = rng.choice(PLAIN_BODIES), WHITE
    b.set_editor_property("body", body)
    b.set_editor_property("accent", accent)
    wall.append(b)


WALL_LEVELS = 7
for i in range(7):                                  # back wall: Y -4300 .. -3700
    for lv in range(WALL_LEVELS):
        add_block(WALL_X, -4300.0 + 100.0 * i, lv)
for j in range(1, 4):                               # left side: X -5950 .. -6150, two blocks thick
    for t in range(2):
        for lv in range(WALL_LEVELS):
            add_block(WALL_X + 100.0 * j, -4250.0 - 100.0 * t, lv)
walls = spawn(unreal.RRRandomCineBlocks, "Cine_DeadEnd", unreal.Vector(WALL_X, ALLEY_Y, 0))
walls.set_editor_property("blocks", wall)
walls.rerun_construction_scripts() if hasattr(walls, "rerun_construction_scripts") else None
n = len(wall)

# The chase (sixth pass): two barricades of small dice across the road. The fox hurdles each (the barricade stands
# where the hurdle clip's root is highest), the tiger behind smashes it with a forward blow (Smash cues,
# ARRRandomCineBlocks::SmashAt) and runs on through the pieces.
HURDLES = ["/Game/Characters/FoxMM/Traversal/A_FoxMM_HurdleL", "/Game/Characters/FoxMM/Traversal/A_FoxMM_HurdleR"]
HURDLE_RATE = 1.3
HURDLE_LIFT = 1.6                                   # the clip's jump height x this (the barricade is higher than the sample's, for the fox's legs)
FOX_START = unreal.Vector(LANE_X, -2500.0, 0.0)
ext = unreal.AnimPoseExtensions
pose_opt = unreal.AnimPoseEvaluationOptions()
pose_opt.extract_root_motion = False


def clip_root(clip, t):
    r = ext.get_bone_pose(ext.get_anim_pose_at_time(clip, t, pose_opt), "Root", unreal.AnimPoseSpaces.WORLD).translation
    return r.y, r.z


def hurdle_keys(path, t0, start):
    """The actor's keys along one hurdle clip's root motion (running toward -Y), the obstacle's Y and time, the end."""
    clip = unreal.load_asset(path)
    length = clip.get_play_length()
    f0, _ = clip_root(clip, 0.0)
    keys, over_y, over_t, best = [], start.y, t0, -1.0
    n = int(round(length / HURDLE_RATE * FPS))
    for i in range(n + 1):
        ct = min(length, i / FPS * HURDLE_RATE)
        fwd, up = clip_root(clip, ct)
        y = start.y - (fwd - f0) * FOX_SCALE
        z = max(0.0, up) * FOX_SCALE * HURDLE_LIFT
        keys.append((t0 + i / FPS, unreal.Vector(start.x, y, z)))
        if up > best:
            best, over_y, over_t = up, y, t0 + i / FPS
    return keys, over_y, over_t, t0 + n / FPS, unreal.Vector(start.x, keys[-1][1].y, 0.0)


PILES, PILE_HITS = [], []
wall = []
# (the barricades' places come from the clips: built with the fox's keys, see "barricades" below)

# A soft fill over the ring: the alley is in shadow (blocking only; the real lighting comes later)
fill = spawn(unreal.PointLight, "Cine_FillLight", unreal.Vector(-5720, ALLEY_Y, 420))
fill_comp = fill.get_component_by_class(unreal.PointLightComponent)
fill_comp.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
fill_comp.set_intensity(120.0)
fill_comp.set_light_color(unreal.LinearColor(1.0, 0.82, 0.66, 1.0))
# ...and one over the ring road for cut 1: the low sun leaves the fox's face in the buildings' shadow
road = spawn(unreal.PointLight, "Cine_RoadFill", unreal.Vector(-4900, -3500, 450))
road_comp = road.get_component_by_class(unreal.PointLightComponent)
road_comp.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
road_comp.set_intensity(150.0)
road_comp.set_attenuation_radius(1800.0)
road_comp.set_cast_shadows(False)
road_comp.set_light_color(unreal.LinearColor(1.0, 0.8, 0.62, 1.0))
# (sixth pass: the chase starts further up the road, in the buildings' shade: a second one there)
road2 = spawn(unreal.PointLight, "Cine_RoadFill2", unreal.Vector(-4860, -2600, 450))
road2_comp = road2.get_component_by_class(unreal.PointLightComponent)
road2_comp.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
road2_comp.set_intensity(170.0)
road2_comp.set_attenuation_radius(1800.0)
road2_comp.set_cast_shadows(False)
road2_comp.set_light_color(unreal.LinearColor(1.0, 0.8, 0.62, 1.0))
fill_comp.set_attenuation_radius(1500.0)
fill_comp.set_cast_shadows(False)

# ---------------------------------------------------------------------------------------------------------------- cameras
# ---------------------------------------------------------------------------------------------------------------- sequence
seq = tools.create_asset(SEQ_NAME, FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_playback_start(0)
seq.set_playback_end(f(LENGTH))
END = f(LENGTH)
LIN = unreal.MovieSceneKeyInterpolation.LINEAR
STEP = unreal.MovieSceneKeyInterpolation.CONSTANT


def transform_keys(binding, keys):
    """keys: [(sec, location, yaw or Rotator, interpolation)]"""
    track = binding.add_track(unreal.MovieScene3DTransformTrack)
    sec = track.add_section()
    sec.set_range(0, END)
    ch = sec.get_all_channels()
    for t, loc, rot, interp in keys:
        if not isinstance(rot, unreal.Rotator):
            rot = unreal.Rotator(0, 0, rot)
        frame = unreal.FrameNumber(f(t))
        for c, v in zip(ch[:6], (loc.x, loc.y, loc.z, rot.roll, rot.pitch, rot.yaw)):
            c.add_key(frame, v, interpolation=interp)


RATE_STATE = {"logged": False}


def set_play_rate(params, rate):
    """UE 5.8 made PlayRate a MovieSceneTimeWarpVariant (a fixed rate or a curve); older engines took a float."""
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
    if not RATE_STATE["logged"]:
        RATE_STATE["logged"] = True
        log(f"play rate not set (rate {rate}); MovieSceneTimeWarpVariant has: {[d for d in dir(tw) if not d.startswith('_')]}")


EASE_STATE = {"logged": False}


def anim_sections(actor, binding_parent, species, parts):
    """parts: [(start sec, end sec, anim name, play rate[, ease in sec, ease out sec])] on the actor's Body.
    Parts given ease times overlap their neighbours by that long and cross-fade (2026-11-20: the tiger's swing popped in
    and out of the run); overlapping sections go on alternate rows."""
    body = actor.get_editor_property("body")
    b = seq.add_possessable(body)
    track = b.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    ticks = seq.get_tick_resolution()
    per_sec = ticks.numerator / max(1, ticks.denominator)
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
        s.set_editor_property("params", p)
        if len(part) > 4 and (part[4] > 0 or part[5] > 0):
            try:
                s.set_row_index(i % 2)
                # (the easing settings are not editable from Python: URRRandomCineLibrary::SetSectionEasing)
                unreal.RRRandomCineLibrary.set_section_easing(s, part[4], part[5])
            except Exception as err:  # noqa: BLE001
                if not EASE_STATE["logged"]:
                    EASE_STATE["logged"] = True
                    log(f"section easing not set: {err}")
    return b


def bool_keys(binding, prop, keys, default):
    track = binding.add_track(unreal.MovieSceneBoolTrack)
    track.set_property_name_and_path(prop, prop)
    s = track.add_section()
    s.set_range(0, END)
    ch = s.get_all_channels()[0]
    # (one key alone holds its value over the whole section, so the starting value is keyed at frame 0 too)
    ch.set_default(default)
    ch.add_key(unreal.FrameNumber(0), default)
    for t, v in keys:
        ch.add_key(unreal.FrameNumber(f(t)), v)


def int_keys(binding, prop, keys, default):
    track = binding.add_track(unreal.MovieSceneIntegerTrack)
    track.set_property_name_and_path(prop, prop)
    s = track.add_section()
    s.set_range(0, END)
    ch = s.get_all_channels()[0]
    ch.set_default(default)
    ch.add_key(unreal.FrameNumber(0), default)
    for t, v in keys:
        ch.add_key(unreal.FrameNumber(f(t)), v)


def visible_keys(binding, keys, start_visible):
    """A visibility track: True = shown (the track's channel is visibility, though it drives bHiddenInGame)."""
    track = binding.add_track(unreal.MovieSceneVisibilityTrack)
    track.set_property_name_and_path("bHiddenInGame", "bHiddenInGame")
    s = track.add_section()
    s.set_range(0, END)
    ch = s.get_all_channels()[0]
    ch.set_default(start_visible)
    ch.add_key(unreal.FrameNumber(0), start_visible)
    for t, v in keys:
        ch.add_key(unreal.FrameNumber(f(t)), v)


fox_spot = ring_spot(FOX_ANGLE)

# The fox: runs down the ring road (cut 1), into the alley and skids (cut 2), turns to face the tiger (3-6), pops into the
# ring with the clones (7.6), aims (9.5 on)
# The chase: two hurdles back to back, then the run to the alley's mouth
fox_keys, fox_parts, t, here = [], [], 0.0, FOX_START
for path in HURDLES:
    keys, over_y, over_t, t_end, here_end = hurdle_keys(path, t, here)
    fox_keys += [(kt, kl, -90, LIN) for kt, kl in keys]
    fox_parts.append((t, t_end, path, HURDLE_RATE))
    PILES.append(unreal.Vector(LANE_X, over_y, 0.0))
    PILE_HITS.append(over_t)
    log(f"hurdle {path.split('_')[-1]}: {t:.2f}-{t_end:.2f} s, over the barricade at Y {over_y:.0f} at {over_t:.2f} s, lands at Y {here_end.y:.0f}")
    t, here = t_end, here_end
CHASE_END = 2.0 + S
RUN_SPEED = 285.0
fox_keys.append((CHASE_END - 0.05, unreal.Vector(LANE_X, here.y - RUN_SPEED * (CHASE_END - 0.05 - t), 0), -90, STEP))
HURDLES_END = t

# barricades: a row of small dice across the lane where each hurdle clip is over its obstacle, higher at the sides,
# a candy crate at each end
for pile in PILES:
    for ix in range(-3, 4):
        b = unreal.RRCineBlock()
        b.set_editor_property("location", pile + unreal.Vector(50 * ix, 0, 25))
        body, accent = rng.choice(DIE_LOOKS)
        b.set_editor_property("die", True)
        b.set_editor_property("body", body)
        b.set_editor_property("accent", accent)
        b.set_editor_property("scale", 0.5)
        b.set_editor_property("yaw", rng.uniform(-8, 8))
        wall.append(b)
    for ix in (-3, -2, 2, 3):                       # second layer at the sides (the fox goes over the low middle)
        b = unreal.RRCineBlock()
        b.set_editor_property("location", pile + unreal.Vector(50 * ix, 0, 75))
        body, accent = rng.choice(DIE_LOOKS)
        b.set_editor_property("die", True)
        b.set_editor_property("body", body)
        b.set_editor_property("accent", accent)
        b.set_editor_property("scale", 0.5)
        b.set_editor_property("yaw", rng.uniform(-15, 15))
        wall.append(b)
    for side in (-1, 1):
        b = unreal.RRCineBlock()
        b.set_editor_property("location", pile + unreal.Vector(215 * side, 5, 35))
        b.set_editor_property("body", rng.choice(PLAIN_BODIES))
        b.set_editor_property("accent", WHITE)
        b.set_editor_property("scale", 0.7)
        b.set_editor_property("yaw", rng.uniform(-20, 20))
        wall.append(b)
clutter = spawn(unreal.RRRandomCineBlocks, "Cine_Clutter", PILES[0])
clutter.set_editor_property("blocks", wall)

# cameras: two for the chase (front tracking for the first hurdle and the tiger's first blow behind it, then low and
# still beside the second barricade), the others as in the fifth pass, S later
CAMS = [
    # (from the road's side and a little above, so the tiger shows behind the fox and the second barricade passes under the lens)
    ("Cam1a_Chase", 24.0, [(0.0, unreal.Vector(LANE_X + 210, FOX_START.y - 500, 150), unreal.Vector(LANE_X - 60, FOX_START.y + 150, 85)),
                           (2.4, unreal.Vector(LANE_X + 210, FOX_START.y - 500 - 2.4 * 262, 150), unreal.Vector(LANE_X - 60, FOX_START.y + 150 - 2.4 * 262, 85))]),
    ("Cam1b_Barricade", 24.0, [(2.4, unreal.Vector(LANE_X + 520, PILES[1].y - 330, 80), unreal.Vector(LANE_X - 20, PILES[1].y + 40, 100)),
                               (2.0 + S, unreal.Vector(LANE_X + 520, PILES[1].y - 380, 80), unreal.Vector(LANE_X - 20, PILES[1].y - 90, 100))]),
    ("Cam2_DeadEnd", 18.0, [(S + 2.0, unreal.Vector(-5960, ALLEY_Y + 175, 85), unreal.Vector(-6060, ALLEY_Y - 40, 60))]),
    ("Cam3_TigerArrives", 30.0, [(S + 3.5, unreal.Vector(-6185, ALLEY_Y - 95, 62), unreal.Vector(-5480, ALLEY_Y + 10, 72))]),
    ("Cam4_GoldDie", 18.0, [(S + 5.5, unreal.Vector(-5870, ALLEY_Y + 150, 430), unreal.Vector(-5870, ALLEY_Y - 40, 30))]),
    ("Cam5_Clones", 24.0, [(S + 7.5, unreal.Vector(-5150, ALLEY_Y, 420), unreal.Vector(-5720, ALLEY_Y, 60))]),
    ("Cam6_Surrounded", 18.0, [(S + 9.5, unreal.Vector(-5450, ALLEY_Y, 720), unreal.Vector(-5690, ALLEY_Y, 0))]),
    # (sixth pass: the remade fox's snout is longer and its mouth sits under it: from 138 high the grin hid under the
    # nose, so the lens is down at the fox's chin height, looking a little up)
    ("Cam7_Grin", 40.0, [(S + 12.0, unreal.Vector(-5545, ALLEY_Y + 140, 92), unreal.Vector(-5850, ALLEY_Y, 112)),
                         (S + 14.0, unreal.Vector(-5590, ALLEY_Y + 126, 90), unreal.Vector(-5850, ALLEY_Y, 112))]),
]
CUTS = [0.0, 2.4, S + 2.0, S + 3.5, S + 5.5, S + 7.5, S + 9.5, S + 12.0, S + 14.0]
cams = []
for name, mm, keys in CAMS:
    _, pos, target = keys[0]
    cam = spawn(unreal.CineCameraActor, f"Cine_{name}", pos, look_rot(pos, target))
    comp = cam.get_cine_camera_component()
    comp.set_editor_property("current_focal_length", mm)
    focus = comp.get_editor_property("focus_settings")
    focus.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
    focus.set_editor_property("manual_focus_distance", (target - pos).length())
    comp.set_editor_property("focus_settings", focus)
    comp.set_editor_property("current_aperture", 2.8)
    cams.append((cam, keys))


fb = seq.add_possessable(fox)
transform_keys(fb, fox_keys + [
    (S + 2.0, unreal.Vector(-5800, ALLEY_Y, 0), 180, LIN),
    (S + 2.5, unreal.Vector(-5960, ALLEY_Y, 0), 180, LIN),
    (S + 3.0, unreal.Vector(-6040, ALLEY_Y, 0), 180, LIN),
    (S + 3.15, unreal.Vector(-6055, ALLEY_Y, 0), 180, LIN),
    (S + 3.45, unreal.Vector(-6060, ALLEY_Y, 0), 0, LIN),
    (S + 6.0, unreal.Vector(-6120, ALLEY_Y, 0), 0, LIN),
    (S + 7.55, unreal.Vector(-6120, ALLEY_Y, 0), 0, STEP),
    (S + 7.6, fox_spot, yaw_to(fox_spot, TIGER_SPOT), LIN),
    (S + 14.0, fox_spot, yaw_to(fox_spot, TIGER_SPOT), LIN),
])
anim_sections(fox, fb, "Fox", fox_parts + [
    (HURDLES_END, S + 2.5, "Run", 1.35), (S + 2.5, S + 3.3, "Cine_StopE", 1.0), (S + 3.3, S + 6.3, "Cine_Scared", 1.0),
    (S + 6.3, S + 7.6, "Cine_FistpumpC", 1.0), (S + 7.6, S + 9.5, "Idle", 1.0), (S + 9.5, S + 12.0, "Cine_PistolAim", 1.0),
    (S + 12.0, S + 14.0, "Cine_PistolAimLow", 1.0)])
bool_keys(fb, "bHoldPistol", [(S + 9.5, True)], False)
# (fourth pass, the user: "?쒖젙 ?띿뒪爾먮? 留뚮뱾?댁꽌 遺숈뿬以?: scared on the chase, worried once it turns to the tiger, set
# when the die lands, the sly grin from the clones on)
# (final pass: two blinks, while worried and in the close-up: 4 = worried with eyes shut, 5 = the grin with eyes shut)
int_keys(fb, "Expression", [(S + 3.3, 1), (S + 4.45, 4), (S + 4.6, 1), (S + 6.3, 2), (S + 7.6, 3), (S + 12.9, 5), (S + 13.05, 3)], 0)

for c, ang in zip(clones, CLONE_ANGLES):
    spot = ring_spot(ang)
    cb = seq.add_possessable(c)
    transform_keys(cb, [(0.0, spot, yaw_to(spot, TIGER_SPOT), LIN)])
    anim_sections(c, cb, "Fox", [(0.0, S + 9.5, "Idle", 1.0), (S + 9.5, S + 14.0, "Cine_PistolAim", 1.0)])
    bool_keys(cb, "bHoldPistol", [(S + 9.5, True)], False)
    int_keys(cb, "Expression", [], 3)
    visible_keys(cb, [(S + 7.6, True)], False)

# The tiger: charges along the road beside the buildings swinging the hammer (cut 1), walks into the alley mouth (3),
# stops short at the die (4), looks round at the clones (5), crouches behind the shield with the hammer dropped (6-7)
tb = seq.add_possessable(tiger)
# The chase: he runs down the same lane behind the fox and reaches each barricade when his forward blow lands
# (the hammer comes down about 110 in front of him), knocking it away, and runs on
SMASH_TIMES = [PILE_HITS[0] + 1.15, PILE_HITS[1] + 1.15]
TIGER_SPEED = (PILES[0].y - PILES[1].y) / max(0.1, SMASH_TIMES[1] - SMASH_TIMES[0])
tiger_y0 = PILES[0].y + 110.0 + TIGER_SPEED * SMASH_TIMES[0]
log(f"tiger: {TIGER_SPEED:.0f} cm/s from Y {tiger_y0:.0f}, blows land at {SMASH_TIMES[0]:.2f} and {SMASH_TIMES[1]:.2f} s")
transform_keys(tb, [
    (0.0, unreal.Vector(LANE_X, tiger_y0, 0), -90, LIN),
    (CHASE_END - 0.05, unreal.Vector(LANE_X, tiger_y0 - TIGER_SPEED * (CHASE_END - 0.05), 0), -90, STEP),
    (S + 2.0, unreal.Vector(-5150, -3400, 0), -90, STEP),
    (S + 3.5, unreal.Vector(-5420, ALLEY_Y, 0), 240, LIN),
    (S + 5.5, unreal.Vector(-5600, ALLEY_Y, 0), 240, LIN),
    (S + 6.2, TIGER_SPOT, 180, LIN),
    (S + 7.8, TIGER_SPOT, 180, LIN),
    (S + 8.3, TIGER_SPOT, 135, LIN),
    (S + 8.9, TIGER_SPOT, 225, LIN),
    (S + 9.4, TIGER_SPOT, 180, LIN),
    (S + 14.0, TIGER_SPOT, 180, LIN),
])
# (2026-11-20, the user: "호랑이 망치 제대로 휘두르도록": the sword-and-shield slash (RunAttackC) poked the hammer about
# at his side. Now the game's own squeaky-hammer swing, an overhead smash (A_Tiger_HammerSwing1's upper body over the
# run's legs, ue_cine_merge_anim.py -> Cine_RunHammer1): the hammer goes up over his head, comes down in front 0.56 s
# into the clip (audition, frame by frame) and stays low as he runs on through the pieces. Cross-faded with the run.)
HIT_AT, BLEND = 0.56, 0.15
SWING_LEN = anim("Tiger", "Cine_RunHammer1").get_play_length()
A1, A2 = SMASH_TIMES[0] - HIT_AT, SMASH_TIMES[1] - HIT_AT
log(f"tiger swing: {SWING_LEN:.2f} s long, starts at {A1:.2f} and {A2:.2f} s")
anim_sections(tiger, tb, "Tiger", [(0.0, A1 + BLEND, "Cine_SsRunB", 1.0, 0.0, BLEND),
                                    (A1, A1 + SWING_LEN, "Cine_RunHammer1", 1.0, BLEND, BLEND),
                                    (A1 + SWING_LEN - BLEND, A2 + BLEND, "Cine_SsRunB", 1.0, BLEND, BLEND),
                                    (A2, A2 + SWING_LEN, "Cine_RunHammer1", 1.0, BLEND, BLEND),
                                    (A2 + SWING_LEN - BLEND, S + 3.5, "Cine_SsRunB", 1.0, BLEND, 0.0),
                                    (S + 3.5, S + 6.2, "Cine_WalkShoulder", 1.0), (S + 6.2, S + 6.3, "Cine_SsIdleC", 1.0),
                                    (S + 6.3, S + 7.0, "Cine_SsImpactA", 1.0), (S + 7.0, S + 7.5, "Cine_SsIdleC", 1.0),
                                    (S + 7.5, S + 9.5, "Cine_LookNervous", 1.3), (S + 9.5, S + 10.0, "Cine_SsCrouchblock", 1.0),
                                    (S + 10.0, S + 14.0, "Cine_SsCrouchblockIdle", 1.0)])
bool_keys(tb, "bHoldHammer", [(S + 9.5, False)], True)
int_keys(tb, "Expression", [(S + 6.3, 1), (S + 7.6, 2)], 0)

db = seq.add_possessable(dropped)
# (fourth pass, the user: the dropped hammer looked stuck to the tiger's back: it now falls out of his hand at 9.5 s and
# lands in front of him, on the open floor toward the ring)
transform_keys(db, [(S + 9.5, HAMMER_REST + unreal.Vector(30, -40, 80), unreal.Rotator(40, 20, 80), LIN),
                    (S + 9.8, HAMMER_REST, unreal.Rotator(0, 0, 35), LIN), (S + 14.0, HAMMER_REST, unreal.Rotator(0, 0, 35), LIN)])
visible_keys(db, [(S + 9.5, True)], False)

# The die: falls tumbling from 5.6 and lands at 6.3 between them
gb = seq.add_possessable(die)
transform_keys(gb, [
    (S + 5.5, DIE_SPOT + unreal.Vector(0, 0, 900), unreal.Rotator(0, 0, 0), LIN),
    (S + 6.3, DIE_SPOT + unreal.Vector(0, 0, DIE_ACROSS * 0.5), unreal.Rotator(720, 540, 360), LIN),
    (S + 14.0, DIE_SPOT + unreal.Vector(0, 0, DIE_ACROSS * 0.5), unreal.Rotator(720, 540, 360), LIN),
])
# (gone when the ultimate starts, like the game's die: the fox's place in the ring is right where it lands)
visible_keys(gb, [(S + 5.5, True), (S + 7.6, False)], False)

# (fifth pass, the user: "?몃옉??洹몃┝?먭? 議곌툑 ???섏뼱?몄꽌 臾댁꽌???먮굦": in cut 3 a warm, low light in the alley's mouth,
# just over the tiger's head and behind him, throws his shadow the length of the alley, over the fox and up the wall)
SHADOW_LIGHT = unreal.Vector(-5150, ALLEY_Y + 120, 300)   # (off to the right: the shadow crosses the frame toward the fox, shaped like him)
shadow_light = spawn(unreal.SpotLight, "Cine_ShadowLight", SHADOW_LIGHT,
                     look_rot(SHADOW_LIGHT, unreal.Vector(-5900, ALLEY_Y - 100, 0)))
sl = shadow_light.get_component_by_class(unreal.SpotLightComponent)
sl.set_mobility(unreal.ComponentMobility.MOVABLE)   # (dynamic shadows from the moving tiger)
sl.set_editor_property("cast_dynamic_shadows", True)
sl.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
sl.set_intensity(5000.0)
sl.set_light_color(unreal.LinearColor(1.0, 0.66, 0.42, 1.0))
sl.set_attenuation_radius(2600.0)
sl.set_editor_property("outer_cone_angle", 30.0)
sl.set_editor_property("inner_cone_angle", 14.0)
sl.set_cast_shadows(True)
sl.set_editor_property("source_radius", 2.0)
visible_keys(seq.add_possessable(shadow_light), [(CUTS[3], True), (CUTS[4], False)], False)
# (and the shadowless fill over the ring is off meanwhile, or it lights the shadow away; the tiger comes in backlit)
visible_keys(seq.add_possessable(fill), [(CUTS[3], False), (CUTS[4], True)], True)
# ...and a narrow, shadowless light on the tiger's face from the fox's side, so his look still shows in the backlight
# without lighting the floor round his shadow
FACE_LIGHT = unreal.Vector(-6150, ALLEY_Y - 40, 170)
face_light = spawn(unreal.SpotLight, "Cine_TigerFaceLight", FACE_LIGHT, look_rot(FACE_LIGHT, unreal.Vector(-5510, ALLEY_Y, 150)))
fl = face_light.get_component_by_class(unreal.SpotLightComponent)
fl.set_mobility(unreal.ComponentMobility.MOVABLE)
fl.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
fl.set_intensity(450.0)
fl.set_light_color(unreal.LinearColor(1.0, 0.86, 0.74, 1.0))
fl.set_attenuation_radius(1200.0)
fl.set_editor_property("outer_cone_angle", 7.0)
fl.set_editor_property("inner_cone_angle", 4.0)
fl.set_cast_shadows(False)
visible_keys(seq.add_possessable(face_light), [(CUTS[3], True), (CUTS[4], False)], False)
# Cut 7: a soft key on the fox's face from beside the lens (the sun is behind it here, and the snout shaded its cheeks
# and grin), no shadows, only for the close-up
KEY_LIGHT = unreal.Vector(-5600, ALLEY_Y + 150, 175)
key_light = spawn(unreal.SpotLight, "Cine_FaceKey", KEY_LIGHT, look_rot(KEY_LIGHT, unreal.Vector(-5850, ALLEY_Y, 100)))
kl = key_light.get_component_by_class(unreal.SpotLightComponent)
kl.set_mobility(unreal.ComponentMobility.MOVABLE)
kl.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
kl.set_intensity(38.0)
kl.set_light_color(unreal.LinearColor(1.0, 0.9, 0.8, 1.0))
kl.set_attenuation_radius(900.0)
kl.set_editor_property("outer_cone_angle", 14.0)
kl.set_editor_property("inner_cone_angle", 6.0)
kl.set_editor_property("source_radius", 30.0)
kl.set_cast_shadows(False)
visible_keys(seq.add_possessable(key_light), [(CUTS[7], True)], False)

# Cameras and the cuts
cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
for i, (cam, keys) in enumerate(cams):
    cb = seq.add_possessable(cam)
    if len(keys) > 1:
        transform_keys(cb, [(t, p, look_rot(p, tg), LIN) for t, p, tg in keys])
    s = cut_track.add_section()
    s.set_range(f(CUTS[i]), f(CUTS[i + 1]))
    bid = None
    for getter in (lambda: seq.get_binding_id(cb), lambda: cb.get_binding_id(),
                   lambda: unreal.MovieSceneSequenceExtensions.get_portable_binding_id(seq, seq, cb)):
        try:
            bid = getter()
            break
        except Exception as e:  # noqa: BLE001 - the binding id call differs between engine versions
            log(f"binding id way failed: {e}")
    s.set_camera_binding_id(bid)

# ---------------------------------------------------------------------------------------------------------------- effects
seq_actor = spawn(unreal.LevelSequenceActor, "Cine_SequenceActor", unreal.Vector(-5700, ALLEY_Y, 0))
seq_actor.set_sequence(seq)
director = spawn(unreal.RRRandomCineDirector, "Cine_Director", unreal.Vector(-5700, ALLEY_Y, 0))
director.set_editor_property("sequence", seq_actor)
director.set_editor_property("gold_die", die)


def cue(t, kind, a=unreal.Vector(0, 0, 0), b=unreal.Vector(0, 0, 0), size=1.0, duration=0.0):
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
    # Cut 1: the hammer smashes the corner of the building beside the road
    # (a ball must reach down to the floor to break blocks: the first try from Z 120 broke none)
    # (third pass: the hammer's two forward blows knock the clutter piles between the tiger and the fox flying, away from
    # him toward the fox; blow times to be matched to the swing)
    cue(SMASH_TIMES[0], K.SMASH, PILES[0] + unreal.Vector(0, 0, 40), PILES[0] + unreal.Vector(0, 160, 0), 260.0),
    cue(SMASH_TIMES[1], K.SMASH, PILES[1] + unreal.Vector(0, 0, 40), PILES[1] + unreal.Vector(0, 160, 0), 260.0),
    # Off screen (cut 2): the corners at the alley's mouth, so the tiger stands in rubble in cut 3
    cue(S + 2.6, K.WRECK, unreal.Vector(-5420, -4200, 40), unreal.Vector(-5420, -4200, 200), 150.0),
    cue(S + 2.6, K.WRECK, unreal.Vector(-5540, -3790, 40), unreal.Vector(-5540, -3790, 200), 150.0),
    # Cut 4: the golden die
    cue(S + 5.5, K.DIE_SPIN, DIE_SPOT, unreal.Vector(-6060, ALLEY_Y, 0), 40.0, 0.8),
    cue(S + 6.3, K.DIE_BURST, DIE_SPOT, unreal.Vector(-6060, ALLEY_Y, 0), 40.0),
    # Cut 5: Fox Clones, one big poof and one at every place in the ring
    cue(S + 7.6, K.FOX_POOF, TIGER_SPOT + unreal.Vector(0, 0, 60), size=1.2),
]
for ang in [FOX_ANGLE] + CLONE_ANGLES:
    cues.append(cue(S + 7.6, K.FOX_POOF, ring_spot(ang) + unreal.Vector(0, 0, 60), size=0.7))
director.set_editor_property("cues", cues)

eal.save_loaded_asset(seq)
les.save_current_level()
log(f"saved {MAP} and {FOLDER}/{SEQ_NAME}: {len(cams)} cameras, {len(cues)} cues, {n} wall blocks")
log("DONE")
