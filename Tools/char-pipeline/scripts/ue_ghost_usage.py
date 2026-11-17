# Round 100: M_FXGhost is drawn on instanced static meshes too (the results camera's ghosts of what it puts away), so the
# usage flag is saved in the asset (a packaged game can't add it at run time).
import unreal

mat = unreal.load_asset("/Game/Effects/DiceFX/M_FXGhost")
changed = unreal.MaterialEditingLibrary.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
unreal.MaterialEditingLibrary.recompile_material(mat)
saved = unreal.EditorAssetLibrary.save_asset("/Game/Effects/DiceFX/M_FXGhost", only_if_is_dirty=False)
unreal.log(f"GHOSTUSAGE DONE set={changed} saved={saved}")
