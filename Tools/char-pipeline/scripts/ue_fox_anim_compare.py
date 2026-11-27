"""Record the same five gameplay inputs from the game viewport, at -benchmark -fps=30."""
import csv
import os
import traceback
from pathlib import Path

import unreal

output = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / "FoxComparison" / os.environ["RR_FOX_TAKE"]
output.mkdir(parents=True, exist_ok=True)
vec = unreal.Vector
gs = unreal.GameplayStatics
frame = -1
world = pawn = controller = camera = walker = None
windows = {"strafe": (128, 222), "pivot": (285, 343), "ears": (407, 477),
           "tail": (551, 622), "climb": (698, 751)}
table = (output / "poses.csv").open("w", newline="", encoding="utf-8")
rows = csv.writer(table)
bones = ["Hips", "LeftFoot", "RightFoot", "Head", "LeftHand", "RightHand", "Tail1", "Tail8", "EarL1", "EarR1",
         "LeftHandIndex1", "RightHandIndex1"]
rows.writerow(["frame", "x", "y", "z", "vx", "vy"] + [b + a for b in bones for a in "xyz"])

def command(s):
    unreal.SystemLibrary.execute_console_command(world, s, controller)

def place(actor, x, y, z, yaw=0):
    actor.character_movement.stop_movement_immediately()
    actor.set_actor_location(vec(x, y, z), False, True)
    actor.set_actor_rotation(unreal.Rotator(0, 0, yaw), True)
    if actor.get_controller():
        actor.get_controller().set_control_rotation(unreal.Rotator(0, 0, yaw))

def run(dt):
    global frame, world, pawn, controller, walker, camera
    if pawn is None:
        for w in unreal.ObjectIterator(unreal.World):
            pc = gs.get_player_controller(w, 0)
            if pc and isinstance(pc.get_controlled_pawn(), unreal.RRRandomCharacter):
                world, controller, pawn = w, pc, pc.get_controlled_pawn()
                break
        if pawn is None:
            return
    frame += 1
    if frame == 50:
        species = os.environ.get("RR_CAPTURE_SPECIES")
        if species:
            body = pawn.mesh.get_editor_property("skeletal_mesh_asset").get_name()
            unreal.log(f"ROSTER TAKE {species} mesh {body}")
            assert species.lower() in body.lower(), (species, body)
        command("RRGod")
        command("r.MotionBlurQuality 0")
        if controller.get_hud() and controller.get_hud().get_editor_property("show_hud"):
            command("showhud")
        command("RRStudioCam 0")
        command("RRStudioCamOff")
        camera = gs.get_all_actors_of_class(world, unreal.CameraActor)[0]
        camera.camera_component.set_field_of_view(58)
        camera.camera_component.set_editor_property("constrain_aspect_ratio", False)
        walker = next(c for c in gs.get_all_actors_of_class(world, unreal.RRRandomCharacter) if c != pawn)
    if frame in (64, 234, 351, 483, 630):
        place(pawn, -1600, -2450, 150)
        place(walker, -2700, -2450, 150)
    if frame == 105:
        command("RRAim 1")
        command("rr.MotionMatch.Log 1")
    if frame == 223:
        command("RRAim 0")
    if frame in (262, 510):
        command("RRSprint 1")
    if frame == 510 and os.environ.get("RR_CAPTURE_SPECIES"):
        command("rr.Tail.Log 1")
    if frame in (344, 623):
        command("RRSprint 0")
    if frame == 382:
        here = pawn.get_actor_location()
        place(walker, here.x-75, here.y+240, here.z, 180)
    if 420 <= frame < 438:
        walker.add_movement_input(vec(-1, 0, 0), 1, True)
    if 139 <= frame < 215:
        pawn.add_movement_input(vec(0, 1, 0), 1, True)
    if 264 <= frame < 305 or 515 <= frame < 574:
        pawn.add_movement_input(vec(1, 0, 0), 1, True)
    if 305 <= frame < 341:
        pawn.add_movement_input(vec(-1, 0, 0), 1, True)
    if frame == 672:
        here = pawn.get_actor_location()
        ground = here.z - pawn.capsule_component.get_scaled_capsule_half_height()
        prior = set(gs.get_all_actors_of_class(world, unreal.StaticMeshActor))
        command("summon /Script/Engine.StaticMeshActor")
        block = next(a for a in gs.get_all_actors_of_class(world, unreal.StaticMeshActor) if a not in prior)
        comp = block.static_mesh_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        block.set_actor_location(vec(here.x+157, here.y, ground+90), False, True)
        block.set_actor_scale3d(vec(2, 2, 1.8))
        comp.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
        comp.set_collision_profile_name("BlockAll")
        comp.set_collision_object_type(unreal.CollisionChannel.ECC_WORLD_STATIC)
        unreal.log(f"FOXTAKE ledge front={here.x+57} top={ground+180}")
    if frame == 708:
        command("RRJump")
    if frame == 721:
        command("RRJumpStop")
    if camera:
        pos = pawn.get_actor_location()
        if frame < 234:
            offset, lift = vec(360, 48, 30), 0
        elif frame < 351:
            offset, lift = vec(22, -380, 30), 0
        elif frame < 483:
            offset, lift = vec(275, -70, 45), 38
        elif frame < 630:
            offset, lift = vec(-115, -350, 90), 0
        else:
            offset, lift = vec(-75, -490, 65), 15
        at = pos + offset
        camera.set_actor_location(at, False, True)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(at, pos+vec(0,0,lift)), False)
        if controller.get_view_target() != camera:
            controller.set_view_target_with_blend(camera, 0)
    for name, (first, end) in windows.items():
        if first <= frame < end:
            command(f"Shot filename={(output / f'{name}_{frame-first:03d}.png').as_posix()} -nosuffix")
            p, v = pawn.get_actor_location(), pawn.get_velocity()
            pts = [pawn.mesh.get_socket_location(b) for b in bones]
            rows.writerow([frame,p.x,p.y,p.z,v.x,v.y]+[round(n,3) for pt in pts for n in (pt.x,pt.y,pt.z)])
    if frame == 757:
        table.close()
        unreal.log("FOXTAKE complete")
        command("quit")

def tick(dt):
    try:
        run(dt)
    except Exception:
        unreal.log_error(traceback.format_exc())
        table.close()
        if world:
            command("quit")

handle = unreal.register_slate_post_tick_callback(tick)
