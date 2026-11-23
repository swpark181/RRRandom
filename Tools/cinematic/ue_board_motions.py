# Storyboard draft, motions (2026-11-23, after the user's "지금 임시로 들어가있는 자세를 애니메이션 만들때 어떻게 작업할지" and
# "바꿀점 참고해서 진행해줘"): the stand-in poses of ue_board_poses.py become moving clips, built inside Unreal from clips the
# fox already has. A new clip is sampled bone by bone from source clips at any time mapping (so legs can come from one
# clip and the upper body from another, or stay frozen), then bones are turned in the body's space by curves over time
# (X = the body's side-to-side axis: + raises what points forward / leans back / looks up; Z = up: + turns to the fox's
# right), or given the direction they have in another clip. Clips come out at their source's key rate as A_Fox_Board_<name> (A_Tiger_Board_<name>).
# What the Level Sequence cannot do by cross-fading sections is done here; cutting and joining whole clips stays there.
# Run with UnrealEditor-Cmd -run=pythonscript (board_run.sh py ue_board_motions.py).
import math

import unreal

FPS = 30
OUT = "/Game/Characters/Fox/Anims"
FOX = OUT + "/A_Fox_"
MM = "/Game/Characters/FoxMM/Anims/A_FoxMM_"
AL = unreal.AnimationLibrary
eal = unreal.EditorAssetLibrary
AXES = {"X": (1.0, 0.0, 0.0), "Y": (0.0, 1.0, 0.0), "Z": (0.0, 0.0, 1.0)}
LEGS = ("upleg", "leg", "foot", "toebase", "toe_end")


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(a):
    return (-a[0], -a[1], -a[2], a[3])


def qnorm(q):
    n = math.sqrt(sum(c * c for c in q)) or 1.0
    return tuple(c / n for c in q)


def qof(q):
    return (q.x, q.y, q.z, q.w)


def lower_body(bone):
    """The root, the hips, the legs and the tail (the rest is the upper body)."""
    return bone in ("root", "hips") or bone.startswith("tail") or (bone.endswith(LEGS) and "arm" not in bone)


class Clip:
    def __init__(self, path):
        self.path = path
        self.anim = unreal.load_asset(path)
        self.length = self.anim.get_play_length()
        self.names = {str(t).lower(): str(t) for t in AL.get_animation_track_names(self.anim)}
        self.chains = {}

    def local(self, bone, t):
        return AL.get_bone_pose_for_time(self.anim, self.names[bone], max(0.0, min(self.length, t)), False)

    def chain(self, bone):
        """The bone's ancestors that have tracks, from the top down (without the root: the body's space starts under it)."""
        if bone not in self.chains:
            path = [str(p).lower() for p in AL.find_bone_path_to_root(self.anim, self.names[bone])]
            self.chains[bone] = [b for b in reversed(path[1:]) if b in self.names and b != "root"]
        return self.chains[bone]

    def body_rot(self, bone, t):
        """The bone's rotation in the body's space at a time of this clip."""
        q = (0.0, 0.0, 0.0, 1.0)
        for b in self.chain(bone) + [bone]:
            q = qmul(q, qof(self.local(b, t).rotation))
        return q


def curve(keys):
    """keys [(time, value)] -> value at a time, eased between the keys."""
    def at(t):
        if t <= keys[0][0]:
            return keys[0][1]
        for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
            if t <= t1:
                x = (t - t0) / max(1e-6, t1 - t0)
                return v0 + (v1 - v0) * x * x * (3.0 - 2.0 * x)
        return keys[-1][1]
    return at


def parent_rot(pose, like, bone):
    q = (0.0, 0.0, 0.0, 1.0)
    for b in like.chain(bone):
        q = qmul(q, pose[b][1])
    return q


def turn(bone, axis, how):
    """Turns a bone in the body's space by `how` degrees (a number, or a curve over the new clip's time)."""
    fn = how if callable(how) else (lambda t: how)

    def op(pose, t, like):
        deg = fn(t)
        if abs(deg) < 1e-4:
            return
        ax, h = AXES[axis], math.radians(deg) / 2.0
        dq = (ax[0] * math.sin(h), ax[1] * math.sin(h), ax[2] * math.sin(h), math.cos(h))
        par = parent_rot(pose, like, bone)
        pose[bone][1] = qnorm(qmul(qmul(qmul(qinv(par), dq), par), pose[bone][1]))
    return op


def face_as(bone, src, when):
    """Gives a bone the direction it has in the body's space in another clip (at when(t) of that clip): an upper body
    laid over other legs keeps pointing where it pointed, whatever the hips under it do."""
    def op(pose, t, like):
        pose[bone][1] = qnorm(qmul(qinv(parent_rot(pose, like, bone)), src.body_rot(bone, when(t))))
    return op


def make(name, like, seconds, sample, ops=(), out=OUT, prefix="A_Fox_Board_"):
    """A new clip on `like`'s skeleton and tracks, `seconds` long: sample(bone, t) gives each bone's own transform
    (None: as `like` starts), then the ops change the pose in order."""
    path = f"{out}/{prefix}{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    eal.duplicate_asset(like.path, path)
    anim = unreal.load_asset(path)
    fps = round((AL.get_num_keys(like.anim) - 1) / like.length)   # the copy keeps its source's key rate (30, or 24 for the tiger's game clips)
    n = int(round(seconds * fps)) + 1
    anim.controller.set_number_of_frames(unreal.FrameNumber(n - 1), False)
    bones = list(like.names)
    keys = {b: ([], [], []) for b in bones}
    for i in range(n):
        t = i / fps
        pose = {}
        for b in bones:
            tr = sample(b, t) or like.local(b, 0.0)
            pose[b] = [tr.translation, qof(tr.rotation), tr.scale3d]
        for op in ops:
            op(pose, t, like)
        for b in bones:
            keys[b][0].append(pose[b][0])
            keys[b][1].append(unreal.Quat(*pose[b][1]))
            keys[b][2].append(pose[b][2])
    for b in bones:
        anim.controller.set_bone_track_keys(like.names[b], keys[b][0], keys[b][1], keys[b][2], False)
    anim.set_editor_property("enable_root_motion", False)
    eal.save_asset(path)
    unreal.log(f"BOARD motion {name}: {anim.get_play_length():.2f}s, {AL.get_num_keys(anim)} keys, from {like.path.split('/')[-1]}")


def of(clip, when):
    """sample(): every bone the clip has, at when(t) of it."""
    return lambda b, t: clip.local(b, when(t)) if b in clip.names else None


def legs_upper(legs, legs_when, upper, upper_when):
    """sample(): the root, hips, legs and tail from one clip, the rest from another."""
    def sample(b, t):
        src, when = (legs, legs_when) if lower_body(b) else (upper, upper_when)
        return src.local(b, when(t)) if b in src.names else None
    return sample


CINE = "/Game/Characters/FoxMM/Cine/A_FoxMM_"
run = Clip(FOX + "Run")
terrified = Clip(FOX + "Cine_Terrified")
aim = Clip(FOX + "Cine_PistolAim")
aim_low = Clip(FOX + "Cine_PistolAimLow")
step_a = Clip(MM + "Walk_Start_F_Rfoot")
step_b = Clip(MM + "Walk_Stop_F_Lfoot")
bench_in = Clip(CINE + "BenchInto")
bench_out = Clip(CINE + "BenchOut")

# Cut 1: three run cycles with one look back over the right shoulder: the head leads, the neck and chest follow and
# come back before it (70 degrees in all: more shows the back of the head to the camera in front). Times are of the
# clip; the sequence plays it 1.35 times as fast.
L = run.length
make("RunLookBack", run, 3 * L, of(run, lambda t: t % L if t < 3 * L - 1e-4 else L), [
    turn("spine2", "Z", curve([(0.20, 0), (0.62, 11), (1.50, 10), (1.86, 0)])),
    turn("neck", "Z", curve([(0.16, 0), (0.56, 21), (1.47, 20), (1.88, 0)])),
    turn("head", "Z", curve([(0.12, 0), (0.50, 42), (0.70, 39), (1.45, 40), (1.90, 0)])),
    turn("head", "X", curve([(0.12, 0), (0.50, -5), (1.45, -5), (1.90, 0)])),
])

# Cut 3: into the wall. The hands are up as it arrives, the chest folds on to the wall and the head nods after it,
# then both snap back and settle (the frightened stand moving underneath)
make("WallHit", terrified, 1.0, of(terrified, lambda t: 1.0 + 0.3 * t), [
    turn("spine", "X", curve([(0.0, -6), (0.07, -12), (0.24, 8), (0.50, 2)])),
    turn("leftarm", "X", curve([(0.0, 58), (0.08, 64), (0.30, 44), (0.70, 30)])),
    turn("rightarm", "X", curve([(0.0, 58), (0.08, 64), (0.30, 44), (0.70, 30)])),
    turn("head", "X", curve([(0.0, -4), (0.09, -14), (0.26, 14), (0.55, 5)])),
])

# Cut 3: round and down. The sample's "sit on a bench in front of you" from where it starts to turn: it turns its back
# to the wall and sits (the sequence turns and moves the fox by the clip's root, and takes it on down to the floor).
# These and the steps below are copies on the game fox's own skeleton (no root, and the tail as every other clip of
# the fox leaves it: the sample's clips hang it straight down).
TURN_T0, STAND_T0 = 2.15, 0.4
# (the clip bows deep over its knees as it sits: here the back stays up, as one that slides down a wall)
make("TurnSit", terrified, 2.3, of(bench_in, lambda t: TURN_T0 + t), [
    turn("spine", "X", curve([(0.7, 0), (1.3, 28), (1.6, 24), (2.1, 6)])),
    turn("head", "X", curve([(0.7, 0), (1.35, 12), (2.1, 0)])),
])

# Cuts 3-5: slumped on the floor. The hips and legs hold still (legs out in front); the frightened upper body only
# sways a little over them (the whole clip looks about: the face has to stay to the camera in cut 5). The second
# clip is the same looking up at the die: the sequence cross-fades to it.
# (the frightened stand looks away to its left: the chest, neck and head are turned back to face the tiger)
FACE = [turn("spine2", "Z", 10), turn("neck", "Z", 15), turn("head", "Z", 22)]
LEGS_OUT = [turn("leftupleg", "X", 78), turn("rightupleg", "X", 78)]
SIT = LEGS_OUT + [turn("spine", "X", 8)] + FACE


def sitting(b, t):
    return terrified.local(b, 1.0 if lower_body(b) else 1.0 + 0.16 * (1.0 - math.cos(2.0 * math.pi * t / 0.9)))


make("Sit", terrified, 9.0, sitting, SIT + [turn("head", "X", -8)])
make("SitLookUp", terrified, 9.0, sitting, SIT + [turn("neck", "X", 14), turn("head", "X", 30)])
# 4-3 (tension pass): shrinking from the hammer: curled forward, chin down, the hands a little higher
make("Cower", terrified, 6.0, sitting, LEGS_OUT + FACE + [turn("spine", "X", -5), turn("head", "X", -17), turn("leftarm", "X", 22), turn("rightarm", "X", 22)])

# Cut 6: up from the floor (the sample's "up from the bench": leans forward, pushes up, a small step forward)
make("StandUp", terrified, 2.2, of(bench_out, lambda t: STAND_T0 + t))

# Cuts 6 and 8: one step forward and stop: the motion matching walk start (right foot out) to 0.55 s, then the walk
# stop from 1.0 s (the left foot comes up beside it) - the two meet with the right foot down in front and the left
# lifting. Plain for cut 6; aiming for cut 8: the aim clip's upper body over those legs, its chest pointing where the
# aim clip points it, and the head up as in the lowered aim (the aim clip's own bows over the pistol).
make("StepA", terrified, 0.9, of(step_a, lambda t: t))
make("StepB", terrified, step_b.length, of(step_b, lambda t: t))
make("AimStepA", terrified, 0.9, legs_upper(step_a, lambda t: t, aim, lambda t: 0.4 + t),
     [face_as("spine", aim, lambda t: 0.4 + t), face_as("neck", aim_low, lambda t: 0.4 + t), face_as("head", aim_low, lambda t: 0.4 + t)])
make("AimStepB", terrified, step_b.length, legs_upper(step_b, lambda t: t, aim, lambda t: 0.4 + 0.9 * t),
     [face_as("spine", aim, lambda t: 0.4 + 0.9 * t), face_as("neck", aim_low, lambda t: 0.4), face_as("head", aim_low, lambda t: 0.4)])

# ------------------------------------------------------------------------------------------------- the tiger (tension pass)
# From the game's overhead hammer swing (wind-up to 0.38 s of the clip, the blow lands at 0.56): the same clip played
# on other time maps. Raise: 0.3 s at rest, 1.5 s up, then held at the top with a small shake. Bonk: down for 0.12 s
# until the hammer meets the die (0.50 s of the clip), thrown back up, hangs there shaking, then lowered to rest.
# Watch: his alley idle, leaning in and looking up at the die.
TIGER = "/Game/Characters/Tiger/Anims"
swing = Clip(TIGER + "/A_Tiger_HammerSwing1")
tiger_idle = Clip(TIGER + "/A_Tiger_Cine_SsIdleC")
REST, TOP, MEET = 0.05, 0.43, 0.54      # (held at 0.43: the hammer straight up over his head; at 0.38 it points back)


def smooth(x):
    x = min(1.0, max(0.0, x))
    return x * x * (3.0 - 2.0 * x)


def raise_when(t):
    return REST + (TOP - REST) * smooth((t - 0.3) / 1.5) + (0.005 * math.sin((t - 1.8) * 26.0) if t > 1.8 else 0.0)


def bonk_when(t):
    if t < 0.12:
        return TOP + (MEET - TOP) * (t / 0.12) ** 2
    if t < 0.32:
        return MEET - (MEET - 0.33) * (1.0 - (1.0 - (t - 0.12) / 0.2) ** 2)
    if t < 0.8:
        return 0.33 + 0.012 * math.sin((t - 0.32) * 15.0) * (1.0 - (t - 0.32) / 0.48)
    return 0.33 - (0.33 - REST) * smooth((t - 0.8) / 0.7)


# (the game lays its swing and shield clips over other legs: their own hips lie on their back. So here too: hips and
# legs of the game's idle, which faces straight ahead as those clips expect - the alley idle stands side on, and the
# swing's chest over its hips faced the side wall - and the clip from the spine up)
tiger_stand = Clip(TIGER + "/A_Tiger_Idle")
make("Raise", swing, 3.0, legs_upper(tiger_stand, lambda t: t, swing, raise_when), out=TIGER, prefix="A_Tiger_Board_")
make("Bonk", swing, 1.9, legs_upper(tiger_stand, lambda t: 3.0 + t, swing, bonk_when), [
    turn("spine", "X", curve([(0.12, 0), (0.30, 13), (0.75, 6), (1.4, 0)])),
    turn("head", "X", curve([(0.12, 0), (0.28, 15), (0.8, 5), (1.5, 0)])),
], out=TIGER, prefix="A_Tiger_Board_")
shield_block = Clip(TIGER + "/A_Tiger_ShieldBlock")
make("Shield", shield_block, 1.25, legs_upper(tiger_stand, lambda t: 5.0 + t, shield_block, lambda t: t), [turn("spine", "X", 9)], out=TIGER, prefix="A_Tiger_Board_")
# (the game's idle, not the alley idle: that one holds the hammer out in front, into the fox's face from here)
make("Watch", tiger_stand, 4.0, of(tiger_stand, lambda t: t), [turn("spine", "X", -6), turn("neck", "X", 8), turn("head", "X", 12)], out=TIGER, prefix="A_Tiger_Board_")
unreal.log("BOARD DONE")
