"""The battle royale's levels (round 92; run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_battle_levels.py [--force] [BattleRoyale BRTest]"

Each level is the engine's Template_Default (sky, sun, fog, floor, player start) with the battle map's actor placed in the
middle of the floor under the player start (ARRRandomGameMode::GetArenaCenter), set to its size (Reach: tiles each way
from the middle one: 3 for the full 7 x 7 map, 1 for the 3 x 3 test map), a NavMeshBoundsVolume over all of it, and the
World Settings' navigation system set to URRRandomNavigationSystem: the navmesh is made only round the animal friends
(navigation invokers) as they go, never all at once. The map itself is built in code as play begins (its tiles,
ARRRandomDiceTown in battle tile mode), so nothing of it is saved in the level.
URRRandomSessionSubsystem opens these for the battle royale (BattleRoyaleMap, BattleRoyaleTestMap).
An existing level is left alone unless --force.
"""
import sys

import unreal

TEMPLATE = "/Engine/Maps/Templates/Template_Default"
TILE = 108 * 100.0
# name: tiles each way from the middle
MAPS = {
    "BattleRoyale": 3,
    "BRTest": 1,
}
NAV_SYSTEM = "/Script/RRRandom.RRRandomNavigationSystem"


def arena_center(actors):
    start = next((a for a in actors if isinstance(a, unreal.PlayerStart)), None)
    floor = next((a for a in actors if isinstance(a, unreal.StaticMeshActor) and "floor" in a.get_actor_label().lower()), None)
    if not start or not floor:
        raise RuntimeError("Template_Default without a player start or a floor")
    origin, extent = floor.get_actor_bounds(False)
    location = start.get_actor_location()
    return unreal.Vector(origin.x, origin.y, location.z), origin.z + extent.z


def make(name, force):
    reach = MAPS[name]
    path = "/Game/Maps/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not force:
            unreal.log(f"BRLEVELS {path} exists, left alone (--force to make it again)")
            return
        unreal.EditorAssetLibrary.delete_asset(path)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.new_level_from_template(path, TEMPLATE):
        raise RuntimeError("could not make " + path)
    actors = actors_sub.get_all_level_actors()
    center, floor_z = arena_center(actors)

    battle = actors_sub.spawn_actor_from_class(unreal.RRRandomBattleMap, center)
    battle.set_actor_label(name + "Map")
    battle.set_editor_property("reach", reach)

    # The navmesh's bounds: the whole square, from under the ground to over the tallest structures
    half = (reach * 2 + 1) * TILE * 0.5 + 500.0
    bottom, top = -500.0, 4000.0
    bounds = actors_sub.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(center.x, center.y, floor_z + (bottom + top) * 0.5))
    bounds.set_actor_label(name + "NavBounds")
    bounds.set_actor_scale3d(unreal.Vector(half * 2.0 / 200.0, half * 2.0 / 200.0, (top - bottom) / 200.0))
    origin, extent = bounds.get_actor_bounds(False)
    if extent.x < 100.0:
        raise RuntimeError("the NavMeshBoundsVolume has no brush")

    # The navigation system that makes the navmesh round the invokers only
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    config = settings.get_editor_property("navigation_system_config")
    if config is None:
        raise RuntimeError("no navigation system config in the World Settings")
    config.set_editor_property("navigation_system_class", unreal.SoftClassPath(NAV_SYSTEM))
    unreal.log(f"BRLEVELS {path}: battle map (reach {reach}) at {center}, floor {floor_z:.0f}, nav bounds {origin} +- {extent}, "
               f"nav system {config.get_editor_property('navigation_system_class')}")
    fogs = [a for a in actors if isinstance(a, unreal.ExponentialHeightFog)]
    unreal.log(f"BRLEVELS {path}: {len(fogs)} height fog, {len(actors)} template actors")
    if not levels.save_current_level():
        raise RuntimeError("could not save " + path)
    unreal.log(f"BRLEVELS saved {path}")


def main():
    args = [a for a in sys.argv[1:] if a]
    force = "--force" in args
    names = [a for a in args if not a.startswith("--")] or list(MAPS)
    for name in names:
        make(name, force)


main()
