# Lists what /Game/Maps/DiceTown holds (cinematic set-up, 2026-11-19). Run with UnrealEditor-Cmd -run=pythonscript.
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Maps/DiceTown")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    loc = a.get_actor_location()
    unreal.log(f"CINEINSPECT {a.get_class().get_name()} '{a.get_actor_label()}' at {loc.x:.0f},{loc.y:.0f},{loc.z:.0f}")
ws = world.get_world_settings()
unreal.log(f"CINEINSPECT gamemode override {ws.get_editor_property('default_game_mode')}")
hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(-5800, -3905, 3000), unreal.Vector(-5800, -3905, -3000),
                                             unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
unreal.log(f"CINEINSPECT floor trace {hit}")
unreal.log("CINEINSPECT DONE")
