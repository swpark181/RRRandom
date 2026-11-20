# Cinematic (2026-11-19): an audition of the tiger's new motions. In Map_FoxVsTiger, a second sequence LS_TigerAudition plays
# every /Game/Characters/Tiger/Anims/A_Tiger_Cine_* for 2.5 s in a row on a tiger with hammer and shield, on the ring road,
# seen from the front-left. Logs the order (CINEAUDITION <index> <start s> <name>). Run with UnrealEditor-Cmd -run=pythonscript.
import unreal

FPS = 24
SLOT = 2.5
FOLDER = "/Game/Cinematics/FoxVsTiger"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary
les.load_level(FOLDER + "/Map_FoxVsTiger")
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("Aud_"):
        eas.destroy_actor(a)
if eal.does_asset_exist(f"{FOLDER}/LS_TigerAudition"):
    eal.delete_asset(f"{FOLDER}/LS_TigerAudition")

import os
WANT = [w for w in os.environ.get("RR_AUD_ANIMS", "").split(",") if w]
anims = sorted(p for p in eal.list_assets("/Game/Characters/Fox/Anims", recursive=False) + eal.list_assets("/Game/Characters/Tiger/Anims", recursive=False)
               if ("A_Tiger_Cine_" in p and not WANT) or any(p.endswith("." + w) for w in WANT))
spot = unreal.Vector(-4930.0, -2950.0, 0.0)   # (2026-11-20: clear of the chase barricades)
tiger = eas.spawn_actor_from_class(unreal.RRRandomCineAnimal, spot, unreal.Rotator(0, 0, -90))
tiger.set_actor_label("Aud_Tiger")
AUD_SPECIES = int(os.environ.get("RR_AUD_SPECIES", "3"))
tiger.set_editor_property("species", AUD_SPECIES)
tiger.set_editor_property("hold_hammer", AUD_SPECIES == 3)
tiger.set_editor_property("hold_shield", AUD_SPECIES == 3)
cam_pos = spot + unreal.Vector(330.0, -330.0, 120.0)
cam = eas.spawn_actor_from_class(unreal.CineCameraActor, cam_pos, unreal.MathLibrary.find_look_at_rotation(cam_pos, spot + unreal.Vector(0, 0, 80)))
cam.set_actor_label("Aud_Cam")
cam.get_cine_camera_component().set_editor_property("current_focal_length", 28.0)

seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset("LS_TigerAudition", FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
end = int(round(len(anims) * SLOT * FPS))
seq.set_playback_start(0)
seq.set_playback_end(end)
body = seq.add_possessable(tiger.get_editor_property("body"))
track = body.add_track(unreal.MovieSceneSkeletalAnimationTrack)
for i, path in enumerate(anims):
    s = track.add_section()
    s.set_range(int(round(i * SLOT * FPS)), int(round((i + 1) * SLOT * FPS)))
    p = s.get_editor_property("params")
    p.set_editor_property("animation", unreal.load_asset(path))
    s.set_editor_property("params", p)
    unreal.log(f"CINEAUDITION {i} {i * SLOT:.1f} {path.split('.')[-1]}")
cb = seq.add_possessable(cam)
cut = seq.add_track(unreal.MovieSceneCameraCutTrack)
cs = cut.add_section()
cs.set_range(0, end)
cs.set_camera_binding_id(seq.get_binding_id(cb))
eal.save_loaded_asset(seq)
les.save_current_level()
unreal.log("CINEAUDITION DONE")
