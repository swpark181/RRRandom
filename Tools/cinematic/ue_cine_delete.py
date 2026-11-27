# Deletes the assets named in RR_DELETE (comma separated /Game paths): test clips and the like.
import os

import unreal

for path in (w for w in os.environ.get("RR_DELETE", "").split(",") if w):
    ok = unreal.EditorAssetLibrary.delete_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    unreal.log(f"CINEBUILD delete {path}: {ok}")
unreal.log("CINEBUILD DONE")
