# Storyboard draft (2026-11-23): an audition of clips for the new cuts' poses. In the draft's own map (a copy of Dice Town,
# /Game/Cinematics/FoxVsTiger/Board/Map_FvT_Board: the real Map_FoxVsTiger is not touched) LS_BoardAudition plays each
# clip of RR_AUD_ANIMS (asset paths, comma separated) for SLOT seconds on a fox (RR_AUD_SPECIES), from the front-left.
import os

import unreal

FPS = 24
SLOT = float(os.environ.get("RR_AUD_SLOT", "2.5"))
FOLDER = "/Game/Cinematics/FoxVsTiger/Board"
MAP = FOLDER + "/Map_FvT_Board"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary
if not eal.does_asset_exist(MAP):
    eal.duplicate_asset("/Game/Maps/DiceTown", MAP)
les.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.GameModeBase)
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("Aud_"):
        eas.destroy_actor(a)
if eal.does_asset_exist(f"{FOLDER}/LS_BoardAudition"):
    eal.delete_asset(f"{FOLDER}/LS_BoardAudition")
species = int(os.environ.get("RR_AUD_SPECIES", "0"))
if species == 0:
    fox_skel = unreal.load_asset("/Game/Characters/Fox/SK_Fox_Skeleton")
    fox_skel.add_compatible_skeleton(unreal.load_asset("/Game/Characters/FoxMM/SK_FoxMM_Skeleton"))
anims = [w for w in os.environ.get("RR_AUD_ANIMS", "").split(",") if w]
# (RR_AUD_SLOTS: seconds for each clip, comma separated, when they differ; otherwise SLOT for all)
slots = [float(w) for w in os.environ.get("RR_AUD_SLOTS", "").split(",") if w] or [SLOT] * len(anims)
starts = [sum(slots[:i]) for i in range(len(anims) + 1)]
spot = unreal.Vector(-4930.0, -2950.0, 0.0)
# (RR_AUD_YAW: the way the animal faces; RR_AUD_CAM: the camera's offset from it "x,y,z"; RR_AUD_LENS: focal length)
who = eas.spawn_actor_from_class(unreal.RRRandomCineAnimal, spot, unreal.Rotator(0, 0, float(os.environ.get("RR_AUD_YAW", "-90"))))
who.set_actor_label("Aud_Animal")
who.set_editor_property("species", species)
who.set_editor_property("hold_hammer", species == 3)
who.set_editor_property("hold_shield", species == 3)


def camera(label, spec):
    """spec "x,y,z[,target x,y,z[,focal length]]", offsets from the animal's feet."""
    v = [float(w) for w in spec.split(",")]
    pos = spot + unreal.Vector(*v[:3])
    target = spot + (unreal.Vector(*v[3:6]) if len(v) >= 6 else unreal.Vector(0, 0, 80))
    c = eas.spawn_actor_from_class(unreal.CineCameraActor, pos, unreal.MathLibrary.find_look_at_rotation(pos, target))
    c.set_actor_label(label)
    c.get_cine_camera_component().set_editor_property("current_focal_length", v[6] if len(v) > 6 else float(os.environ.get("RR_AUD_LENS", "28")))
    return c


# (RR_AUD_CAM2: a second camera; the clips then play twice, the second time seen from it)
cam_list = [camera("Aud_Cam", os.environ.get("RR_AUD_CAM", "330,-330,120"))]
if os.environ.get("RR_AUD_CAM2"):
    cam_list.append(camera("Aud_Cam2", os.environ["RR_AUD_CAM2"]))
seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset("LS_BoardAudition", FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
once = int(round(starts[-1] * FPS))
end = once * len(cam_list)
seq.set_playback_start(0)
seq.set_playback_end(end)
body = seq.add_possessable(who.get_editor_property("body"))
track = body.add_track(unreal.MovieSceneSkeletalAnimationTrack)
cut = seq.add_track(unreal.MovieSceneCameraCutTrack)
for k, cam in enumerate(cam_list):
    for i, path in enumerate(anims):
        a = unreal.load_asset(path)
        if not a:
            unreal.log(f"BOARD missing {path}")
            continue
        s = track.add_section()
        s.set_range(k * once + int(round(starts[i] * FPS)), k * once + int(round(starts[i + 1] * FPS)))
        p = s.get_editor_property("params")
        p.set_editor_property("animation", a)
        s.set_editor_property("params", p)
        if k == 0:
            unreal.log(f"BOARD audition {i} {starts[i]:.1f}s (frame {int(round(starts[i] * FPS))}) {path.split('/')[-1]} {a.get_play_length():.2f}s")
    cs = cut.add_section()
    cs.set_range(k * once, (k + 1) * once)
    cs.set_camera_binding_id(seq.get_binding_id(seq.add_possessable(cam)))
eal.save_loaded_asset(seq)
les.save_current_level()
unreal.log("BOARD DONE")
