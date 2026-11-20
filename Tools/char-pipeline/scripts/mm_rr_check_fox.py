"""Motion matching test, step 4 (run inside Unreal, in RRRandom): check that the fox test set copied from the Game
Animation Sample project (/Game/Characters/FoxMM: SK_FoxMM with a root bone and its retargeted clips) loads here, and
give the test mesh the fox's material."""
import unreal

DEST = "/Game/Characters/FoxMM"
eal = unreal.EditorAssetLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg):
    unreal.log(f"MM {msg}")


mesh = unreal.load_asset(f"{DEST}/SK_FoxMM")
skel = mesh.skeleton
log(f"mesh {mesh.get_name()} skeleton {skel.get_name()} materials {[str(m.material_slot_name) for m in mesh.materials]}")
fox = unreal.load_asset("/Game/Characters/Fox/SK_Fox")
log(f"game fox materials {[(str(m.material_slot_name), m.material_interface.get_name() if m.material_interface else None) for m in fox.materials]}")
mats = list(mesh.materials)
src = list(fox.materials)
for i, m in enumerate(mats):
    if i < len(src) and src[i].material_interface:
        m.material_interface = src[i].material_interface
mesh.materials = mats
eal.save_asset(f"{DEST}/SK_FoxMM")
ok, bad, secs = 0, [], 0.0
for ad in reg.get_assets_by_path(f"{DEST}/Anims", recursive=False):
    seq = ad.get_asset()
    if seq and seq.get_skeleton() == skel:
        ok += 1
        secs += seq.get_play_length()
    else:
        bad.append(str(ad.asset_name))
log(f"clips on the test skeleton {ok}, {secs:.0f} s in all, not loading {bad}")
log("MMDONE check")
