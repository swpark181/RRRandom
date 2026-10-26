# Round 89 check (run in a -game session with "py <this file>"): is there navmesh at spots in and round a lava pool dug by
# RRLava at (-1940, 85)? Logs NAVCHECK <name> <hit or none> for each.
import unreal

world = None
for w in unreal.ObjectIterator(unreal.World):
    try:
        if unreal.GameplayStatics.get_player_controller(w, 0):
            world = w
    except Exception:
        pass
spots = {
    "road_before": unreal.Vector(-2700.0, 0.0, 0.0),
    "crater_rim": unreal.Vector(-2200.0, 85.0, -100.0),
    "lava_middle": unreal.Vector(-1950.0, 50.0, -200.0),
    "road_after": unreal.Vector(-1300.0, 0.0, 0.0),
}
for name, spot in spots.items():
    try:
        hit = unreal.NavigationSystemV1.project_point_to_navigation(world, spot, None, None, unreal.Vector(40.0, 40.0, 300.0))
        unreal.log(f"NAVCHECK {name} {hit}")
    except Exception as e:
        unreal.log(f"NAVCHECK {name} error {e}")
