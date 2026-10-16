"""Dump the UE Python API surface the import/retarget script depends on (run inside Unreal)."""
import unreal

out = []


def dump(name):
    cls = getattr(unreal, name, None)
    if cls is None:
        out.append(f"## {name}: MISSING")
        return
    out.append(f"## {name}")
    for m in sorted(dir(cls)):
        if m.startswith("_"):
            continue
        doc = (getattr(cls, m).__doc__ or "").strip().splitlines()
        out.append(f"  {m}: {doc[0] if doc else ''}")


for n in ["IKRigController", "IKRetargeterController", "IKRetargetBatchOperation",
          "IKRetargetFactory", "IKRigDefinitionFactory", "RetargetSourceOrTarget",
          "AutoMapChainType", "RetargetAutoAlignMethod", "SkeletalMeshEditorSubsystem",
          "InterchangeGenericAssetsPipeline", "FbxImportUI", "SkeletalMeshLODSettings"]:
    dump(n)

out.append("## assets")
for p in ["/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple",
          "/Game/Characters/Mannequins/Meshes/SK_Mannequin",
          "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"]:
    a = unreal.load_asset(p)
    out.append(f"  {p}: {type(a).__name__ if a else 'NOT LOADED'}")
out.append(f"## engine {unreal.SystemLibrary.get_engine_version()}")

open(r"C:\work\char-pipeline\output\ue_api_probe.txt", "w", encoding="utf-8").write("\n".join(out))
unreal.log("PROBE DONE")
