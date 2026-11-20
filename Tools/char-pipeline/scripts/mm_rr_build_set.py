"""Motion matching test, step 5 (run inside Unreal, in RRRandom): make an animal friend's motion matching set from the
clips already in /Game/Characters/<Name>MM/Anims (mm_gasp_retarget_fox.py, copied over from the sample project).

Calls the game's editor tool URRRandomMotionMatchLibrary::BuildSet, which makes PSS_<Name>MM (schema), PSD_<Name>MM
(database with every clip) and ABP_<Name>MM (Motion Matching -> Pose History -> output). Delete old ones on disk first.

Usage: UnrealEditor-Cmd RRRandom.uproject -run=pythonscript "-script=mm_rr_build_set.py <Name>"   e.g. Fox
"""
import sys

import unreal

name = sys.argv[1].strip() + "MM"
DEST = f"/Game/Characters/{name}"
eal = unreal.EditorAssetLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg):
    unreal.log(f"MM {msg}")


# Old ones must be gone from the disk before Unreal starts: deleting the old blueprint here and making a new one of the
# same name in the same run crashed the editor at the blueprint's compile (2026-11-20, twice)
stale = [old for old in (f"ABP_{name}", f"PSD_{name}", f"PSS_{name}") if eal.does_asset_exist(f"{DEST}/{old}")]
if stale:
    log(f"ERROR {stale} exist: delete their .uasset files in Content/Characters/{name} first, then run this again")
    log("MMDONE build (nothing made)")
    raise SystemExit(0)
mesh = unreal.load_asset(f"{DEST}/SK_{name}")
clips = [ad.get_asset() for ad in reg.get_assets_by_path(f"{DEST}/Anims", recursive=False)]
clips = [c for c in clips if isinstance(c, unreal.AnimSequence)]
loops = [c.get_name() for c in clips if c.get_editor_property("loop")]
log(f"{len(clips)} clips, {len(loops)} of them marked looping: {loops[:12]}")
result = unreal.RRRandomMotionMatchLibrary.build_set(mesh, clips, DEST, name)
log(f"build: {result}")
for made in (f"ABP_{name}", f"PSD_{name}", f"PSS_{name}"):
    log(f"{made} exists {eal.does_asset_exist(f'{DEST}/{made}')}")
db = unreal.load_asset(f"{DEST}/PSD_{name}")
if db:
    log(f"database holds {db.get_num_animation_assets()} animations")
log("MMDONE build")
