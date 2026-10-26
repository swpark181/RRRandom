"""The third person maps as levels (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_tps_levels.py [--force] [DiceTown DiceLand Studio]"

Each level is the engine's Template_Default (sky, sun, floor, player start) with the map's actor placed where the game
mode used to spawn it (ARRRandomGameMode::GetArenaCenter: the middle of the floor under the player start, at the start's
height) and a NavMeshBoundsVolume over the map, so the navigation system makes the bots' navmesh (RecastNavMesh in
DefaultEngine.ini: made at runtime, mended as blocks are smashed). The map actor builds itself in the editor too (never
saved), so opening the level shows the map, and P its navmesh. URRRandomSessionSubsystem opens these levels for the
third person maps picked on the title (DiceTownMap, DiceLandMap, StudioMap in DefaultGame.ini).
An existing level is left alone unless --force.
"""
import sys

import unreal

TEMPLATE = "/Engine/Maps/Templates/Template_Default"
# name: (map actor class, half width of the navmesh bounds, its bottom and top over the floor)
MAPS = {
    # The island's rim is about 8450 out (MapHalf 8800); the dice mountain and the keeps reach about 3000 up
    "DiceTown": (unreal.RRRandomDiceTown, 9000.0, -500.0, 3500.0),
    # MapHalf 3400
    "DiceLand": (unreal.RRRandomDiceLand, 3700.0, -400.0, 1500.0),
    # The studio's grid is 6000 each way from the middle (its floor slab goes much further)
    "Studio": (unreal.RRRandomStudio, 6500.0, -300.0, 1500.0),
}


def arena_center(actors):
    start = next((a for a in actors if isinstance(a, unreal.PlayerStart)), None)
    floor = next((a for a in actors if isinstance(a, unreal.StaticMeshActor) and "floor" in a.get_actor_label().lower()), None)
    if not start or not floor:
        raise RuntimeError("Template_Default without a player start or a floor")
    origin, extent = floor.get_actor_bounds(False)
    location = start.get_actor_location()
    return unreal.Vector(origin.x, origin.y, location.z), origin.z + extent.z


def make(name, force):
    cls, half, bottom, top = MAPS[name]
    path = "/Game/Maps/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not force:
            unreal.log(f"TPSLEVELS {path} exists, left alone (--force to make it again)")
            return
        unreal.EditorAssetLibrary.delete_asset(path)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.new_level_from_template(path, TEMPLATE):
        raise RuntimeError("could not make " + path)
    center, floor_z = arena_center(actors_sub.get_all_level_actors())

    actor = actors_sub.spawn_actor_from_class(cls, center)
    actor.set_actor_label(name)
    bounds = actors_sub.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(center.x, center.y, floor_z + (bottom + top) * 0.5))
    bounds.set_actor_label(name + "NavBounds")
    # The volume's brush is a 200 cube
    bounds.set_actor_scale3d(unreal.Vector(half * 2.0 / 200.0, half * 2.0 / 200.0, (top - bottom) / 200.0))
    origin, extent = bounds.get_actor_bounds(False)
    unreal.log(f"TPSLEVELS {path}: {name} at {center}, floor {floor_z:.0f}, nav bounds {origin} +- {extent}")
    if extent.x < 100.0:
        raise RuntimeError("the NavMeshBoundsVolume has no brush")
    if not levels.save_current_level():
        raise RuntimeError("could not save " + path)
    unreal.log(f"TPSLEVELS saved {path}")


def main():
    args = [a for a in sys.argv[1:] if a]
    force = "--force" in args
    names = [a for a in args if not a.startswith("--")] or list(MAPS)
    for name in names:
        make(name, force)


main()
