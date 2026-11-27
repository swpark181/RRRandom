"""Print a skeletal mesh's material slots and what is in them (2026-11-25, checking the fur after a re-import).
Usage: ue_py_import.sh mats "MATS DONE" "ue_mesh_materials.py /Game/Characters/Fox/SK_FoxFur,..." """
import sys
import unreal
for p in " ".join(sys.argv[1:]).split(","):
    m = unreal.load_asset(p.strip())
    if not m:
        unreal.log_warning(f"MATS {p}: missing")
        continue
    for i, s in enumerate(m.get_editor_property("materials")):
        mi = s.get_editor_property("material_interface")
        unreal.log_warning(f"MATS {p} [{i}] {s.get_editor_property('material_slot_name')} -> {mi.get_path_name() if mi else None}")
unreal.log_warning("MATS DONE")
