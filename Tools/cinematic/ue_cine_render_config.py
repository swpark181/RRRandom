# Makes the Movie Render Queue preset for the blocking review render (2026-11-19): 1280x720, 24 fps from the sequence,
# JPG frames into char-pipeline/output/cinematic_blocking/render/<sequence>/, long warm-up so Dice Town has built itself.
# Run with UnrealEditor-Cmd -run=pythonscript. Env RR_CINE_RES=WxH overrides the size, RR_CINE_OUT the folder.
import os

import unreal

FOLDER = "/Game/Cinematics/FoxVsTiger"
NAME = os.environ.get("RR_CINE_PRESET", "MRQ_Blocking720")   # (2026-11-23: the storyboard draft renders with its own preset)
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist(f"{FOLDER}/{NAME}"):
    eal.delete_asset(f"{FOLDER}/{NAME}")
cls = getattr(unreal, "MoviePipelinePrimaryConfig", None) or getattr(unreal, "MoviePipelineMasterConfig")
tools = unreal.AssetToolsHelpers.get_asset_tools()
cfg = tools.create_asset(NAME, FOLDER, cls, None)
w, h = (int(v) for v in os.environ.get("RR_CINE_RES", "1280x720").split("x"))
out = os.environ.get("RR_CINE_OUT", "C:/work/char-pipeline/output/cinematic_blocking/render")

o = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
o.set_editor_property("output_resolution", unreal.IntPoint(w, h))
# (the {sequence_name} token came out empty in a command-line render: the names are written out)
take = os.environ.get("RR_CINE_TAKE", "take")
o.set_editor_property("output_directory", unreal.DirectoryPath(path=f"{out}/{take}"))
o.set_editor_property("file_name_format", "fvt.{frame_number}")
o.set_editor_property("override_existing_output", True)
cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_JPG)
aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
aa.set_editor_property("engine_warm_up_count", 90)
aa.set_editor_property("render_warm_up_count", 32)
aa.set_editor_property("use_camera_cut_for_warm_up", False)
# RR_CINE_FINAL=1 (the final render): 8 temporal samples a frame (smooth motion blur and edges) and every texture at its
# full size (no streaming mips: the close-ups' fur and faces)
if os.environ.get("RR_CINE_FINAL") == "1":
    aa.set_editor_property("override_anti_aliasing", True)
    aa.set_editor_property("temporal_sample_count", 8)
    aa.set_editor_property("spatial_sample_count", 1)
    cv = cfg.find_or_add_setting_by_class(unreal.MoviePipelineConsoleVariableSetting)
    for name, value in (("r.TextureStreaming", 0.0), ("r.MotionBlurQuality", 4.0), ("r.DepthOfFieldQuality", 4.0)):
        try:
            cv.add_or_update_console_variable(name, value)
        except Exception as e:  # noqa: BLE001 - the call's name differs between engine versions
            unreal.log(f"CINECFG console variable {name} not set: {e}")
    unreal.log("CINECFG final quality: 8 temporal samples, texture streaming off")
# (2026-11-20: a material swapped in mid-shot (the expressions) drew grey, the default material, for a frame while its
# pipeline state compiled. Setting r.PSOPrecache.ProxyCreationWhenPSOReady=0 here made it worse (the clones came in
# grey too); cine_blocking.ps1 turns PSO precaching off on the render's command line instead, and removes this one
# from a preset that still has it.)
try:
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineConsoleVariableSetting).remove_console_variable(
        "r.PSOPrecache.ProxyCreationWhenPSOReady", True)
except Exception as e:  # noqa: BLE001
    unreal.log(f"CINECFG PSO precache variable not removed: {e}")
# RR_CINE_CVARS="name=value,..." (checks, e.g. rr.Fur=0 to see the bodies without their fur)
extra = [kv.split("=") for kv in os.environ.get("RR_CINE_CVARS", "").split(",") if "=" in kv]
if extra:
    cv = cfg.find_or_add_setting_by_class(unreal.MoviePipelineConsoleVariableSetting)
    for name, value in extra:
        cv.add_or_update_console_variable(name.strip(), float(value))
        unreal.log(f"CINECFG console variable {name.strip()} = {value}")
eal.save_loaded_asset(cfg)
unreal.log(f"CINECFG saved {FOLDER}/{NAME} {w}x{h} -> {out}")
unreal.log("CINECFG DONE")
