"""Print the file each animal friend's game meshes were last imported from, with bone counts (2026-11-25, ear bones:
which rig FBX is in the game now). Usage: ue_py_import.sh srcs "SOURCES DONE" "ue_mesh_sources.py Fox,RedFox,..."."""
import sys
import unreal
names = " ".join(sys.argv[1:]).split(",")
for n in names:
    for a in (f"SK_{n}", f"SK_{n}Fur", f"SK_{n}Muzzle", f"SK_{n}FurCine"):
        p = f"/Game/Characters/{n}/{a}"
        if not unreal.EditorAssetLibrary.does_asset_exist(p):
            continue
        m = unreal.load_asset(p)
        src = m.get_editor_property("asset_import_data").get_first_filename() if m.get_editor_property("asset_import_data") else "?"
        skel = m.get_editor_property("skeleton")
        bones = unreal.AnimPoseExtensions.get_bone_names(unreal.AnimPoseExtensions.get_reference_pose(skel)) if skel else []
        ear = [str(b) for b in bones if "ear" in str(b).lower() or "head" in str(b).lower()]
        unreal.log_warning(f"SRC {p} | {src} | skel {skel.get_path_name() if skel else None} | bones {len(bones)} | {ear}")
unreal.log_warning("SOURCES DONE")
