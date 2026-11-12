# Runs inside a -game session (-ExecCmds="py <this>"): RR_SEQ is "seconds:command;..." from the start; "shot" takes a
# HighResShot, "quit" quits, anything else is a console command for the player. Logs each step as GAMESEQ.
# Driven by capture_game_seq.ps1 (round 71: riding checks for every animal friend, see the animal-friend skill).
# Extras: nearmesh=<mesh part>[#n] (beside the nth vehicle with that mesh, 0 first), goto=x,y,yaw, gotoz=x,y,z,yaw, walk=, bring=, place=,
# dump (brawlers and vehicles), mv, speed, z; "sidecam" fails in this engine version: use RRStudioCam instead.
# Round 90-91: guncam=right|left|top|front,<dist> / guncam=body,<dist>,<yaw>,<height> (cameras on my held gun),
# vehcam=<mesh part>,<yaw>,<dist>,<height> (round a parked vehicle), guncamoff / vehcamoff, fpsmark + fps=<label>.
import os
import time

import unreal

steps = []
for part in os.environ.get("RR_SEQ", "").split(";"):
    if part.strip():
        at, cmd = part.split(":", 1)
        steps.append((float(at), cmd.strip()))
start = time.time()
state = {"i": 0}


def world():
    # A world with a player; with RR_NETWORLD=1 only a networked one (after joining a host, the old solo world may linger)
    found = None
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if unreal.GameplayStatics.get_player_controller(w, 0):
                if os.environ.get("RR_NETWORLD") != "1" or not unreal.SystemLibrary.is_standalone(w):
                    found = w
        except Exception:
            pass
    return found


def veh_cam(w, pc):
    # vehcam=<mesh part>,<yaw round from its front>,<distance>,<height>: every frame the studio camera's actor sits round
    # the first vehicle with that mesh, looking at its middle (round 91: its lamps and paint)
    v = state.get("vehcam")
    if not v:
        return
    cam = state.get("cam_actor")
    if cam is None:
        unreal.SystemLibrary.execute_console_command(w, "RRStudioCam 0", pc)
        unreal.SystemLibrary.execute_console_command(w, "RRStudioCamOff", pc)
        cams = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
        cam = cams[0] if cams else None
        state["cam_actor"] = cam
    target = state.get("vehcam_actor")
    if target is None or not unreal.SystemLibrary.is_valid(target):
        target = None
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomVehicle):
            if any(c.static_mesh and v[0] in c.static_mesh.get_name() for c in a.get_components_by_class(unreal.StaticMeshComponent)):
                target = a
                break
        state["vehcam_actor"] = target
    if not cam or not target:
        return
    middle = target.get_actor_location() + unreal.Vector(0.0, 0.0, 60.0)
    yaw = target.get_actor_rotation().yaw + v[1]
    pos = middle + unreal.MathLibrary.get_forward_vector(unreal.Rotator(0.0, 0.0, yaw)) * v[2] + unreal.Vector(0.0, 0.0, v[3])
    cam.set_actor_location(pos, False, False)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos, middle), False)
    if pc.get_view_target() != cam:
        pc.set_view_target_with_blend(cam, 0.0)


def gun_cam(w, pc):
    # guncam=<side>,<distance>: every frame the studio camera's actor sits beside my held gun (side right, left, top or
    # front of the gun itself), looking at the gun's middle, so the grip and the hand round it show (round 90 grip fitting)
    g = state.get("guncam")
    pawn = pc.get_controlled_pawn()
    if not g or not pawn:
        return
    cam = state.get("cam_actor")
    if cam is None:
        unreal.SystemLibrary.execute_console_command(w, "RRStudioCam 0", pc)
        unreal.SystemLibrary.execute_console_command(w, "RRStudioCamOff", pc)
        cams = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
        cam = cams[0] if cams else None
        state["cam_actor"] = cam
    guns = [c for c in pawn.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and ("GunsR90" in c.static_mesh.get_path_name() or "Weapons97" in c.static_mesh.get_path_name())]
    if not cam or not guns:
        return
    gun = guns[0]
    xf = gun.get_world_transform()
    middle = unreal.MathLibrary.transform_location(xf, unreal.Vector(30.0, 0.0, 25.0))
    if g[0] == "body":
        # guncam=body,<distance>,<yaw round from the front>,<height>: steady beside my animal friend (turning with it),
        # looking at the gun
        yaw = pawn.get_actor_rotation().yaw + g[2]
        pos = pawn.get_actor_location() + unreal.MathLibrary.get_forward_vector(unreal.Rotator(0.0, 0.0, yaw)) * g[1] + unreal.Vector(0.0, 0.0, g[3])
        # the camera stays level with the body, not the swinging gun: aim at a point between the gun and the chest
        middle = middle * 0.7 + (pawn.get_actor_location() + unreal.Vector(0.0, 0.0, 10.0)) * 0.3
        cam.set_actor_location(pos, False, False)
        cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos, middle), False)
        if pc.get_view_target() != cam:
            pc.set_view_target_with_blend(cam, 0.0)
        return
    way = {"right": gun.get_right_vector(), "left": gun.get_right_vector() * -1.0, "top": gun.get_up_vector(),
           "front": gun.get_forward_vector()}[g[0]]
    pos = middle + way * g[1] + gun.get_up_vector() * (g[1] * 0.25 if g[0] != "top" else 0.0)
    cam.set_actor_location(pos, False, False)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos, middle), False)
    if pc.get_view_target() != cam:
        pc.set_view_target_with_blend(cam, 0.0)


def tick(dt):
    t = time.time() - start
    w = world()
    if not w or state["i"] >= len(steps):
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if not pc or (not pc.get_controlled_pawn() and os.environ.get("RR_NOPAWN") != "1"):
        return
    gun_cam(w, pc)
    veh_cam(w, pc)
    # fpsmark / fps=<label>: the average frame time since the mark (round 91, lighting costs)
    state["fps_dt"] = state.get("fps_dt", 0.0) + dt
    state["fps_n"] = state.get("fps_n", 0) + 1
    # walk=<x>,<y>,<seconds>: holds a movement input along world X/Y for a while
    if state.get("walk_until", 0) > t and pc.get_controlled_pawn():
        pc.get_controlled_pawn().add_movement_input(state["walk_dir"], 1.0, False)
    while state["i"] < len(steps) and t >= steps[state["i"]][0]:
        cmd = steps[state["i"]][1]
        if cmd == "shot":
            unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720")
        elif cmd == "dump":
            for o in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomOre):
                unreal.log(f"GAMESEQ ore {o.get_name()} at {o.get_actor_location()}")
            for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCharacter):
                unreal.log(f"GAMESEQ brawler {c.get_name()} at {c.get_actor_location()}")
            for v in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomVehicle):
                meshes = [c.static_mesh.get_name() for c in v.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
                unreal.log(f"GAMESEQ vehicle {meshes} at {v.get_actor_location()}")
        elif cmd in ("jump", "unjump"):
            # Presses / lets go of jump on my brawler (its Jump override runs)
            pawn = pc.get_controlled_pawn()
            pawn.jump() if cmd == "jump" else pawn.stop_jumping()
            loc = pawn.get_actor_location()
            unreal.log(f"GAMESEQ z {loc.z:.0f}")
        elif cmd == "lineup":
            # Puts every other brawler in a row across my view, 600 ahead
            pawn = pc.get_controlled_pawn()
            base = pawn.get_actor_location()
            rot = pc.get_control_rotation()
            fwd = unreal.MathLibrary.get_forward_vector(unreal.Rotator(0.0, 0.0, rot.yaw))
            right = unreal.MathLibrary.get_right_vector(unreal.Rotator(0.0, 0.0, rot.yaw))
            others = [c for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCharacter) if c != pawn]
            for i, c in enumerate(others):
                spot = base + fwd * float(os.environ.get("RR_LINEUP_AHEAD", "650")) + right * ((i - (len(others) - 1) / 2.0) * 170.0) + unreal.Vector(0.0, 0.0, 10.0)
                c.set_actor_location(spot, False, True)
                c.set_actor_rotation(unreal.Rotator(0.0, 0.0, rot.yaw + 180.0), True)
                unreal.log(f"GAMESEQ lineup {c.get_name()}")
        elif cmd.startswith("sidecam"):
            # A still camera beside my brawler (sidecam=<distance>,<up>,<yaw offset>), looking at it; "maincam" goes back
            dist, up, yaw_off = (float(v) for v in (cmd.split("=")[1].split(",") if "=" in cmd else ("450", "60", "90")))
            pawn = pc.get_controlled_pawn()
            base = pawn.get_actor_location()
            yaw = pawn.get_actor_rotation().yaw + yaw_off
            offset = unreal.MathLibrary.get_forward_vector(unreal.Rotator(0.0, 0.0, yaw)) * dist + unreal.Vector(0.0, 0.0, up)
            spot = base + offset
            look = unreal.MathLibrary.find_look_at_rotation(spot, base)
            cam = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, spot, look) if False else w.spawn_actor(unreal.CameraActor, spot, look) if hasattr(w, "spawn_actor") else unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(w, unreal.CameraActor, unreal.Transform(spot, look, unreal.Vector(1, 1, 1)))
            if not isinstance(cam, unreal.CameraActor):
                cam = unreal.GameplayStatics.finish_spawning_actor(cam, unreal.Transform(spot, look, unreal.Vector(1, 1, 1)))
            pc.set_view_target_with_blend(cam, 0.0)
            state["cam"] = cam
        elif cmd.startswith("nearmesh="):
            # Puts my animal friend beside the vehicle whose mesh name has this in it (nearmesh=StarChest)
            pawn = pc.get_controlled_pawn()
            want, _, skip = cmd.split("=")[1].partition("#")
            skip = int(skip or 0)
            for v in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomVehicle):
                comps = [c for c in v.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and want in c.static_mesh.get_name()]
                if comps and skip > 0:
                    skip -= 1
                    continue
                if comps:
                    loc = v.get_actor_location()
                    right = v.get_actor_right_vector()
                    pawn.set_actor_location(loc + right * 120.0 + unreal.Vector(0.0, 0.0, 100.0), False, True)
                    pawn.set_actor_rotation(v.get_actor_rotation(), True)
                    pc.set_control_rotation(v.get_actor_rotation())
                    unreal.log(f"GAMESEQ near {v.get_name()} {loc}")
                    break
        elif cmd.startswith("facecover="):
            # Puts my animal friend this far from the nearest cover, facing it (facecover=1300): the star cars then park
            # between, and the robot they make turns to face the cover
            dist = float(cmd.split("=")[1])
            pawn = pc.get_controlled_pawn()
            here = pawn.get_actor_location()
            covers = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCover)
            if covers:
                near = min(covers, key=lambda c: (c.get_actor_location() - here).length())
                at = near.get_actor_location()
                way = unreal.Vector(at.x - here.x, at.y - here.y, 0.0)
                way.normalize()
                spot = unreal.Vector(at.x - way.x * dist, at.y - way.y * dist, here.z + 50.0)
                yaw = unreal.MathLibrary.make_rot_from_x(way).yaw
                pawn.set_actor_location(spot, False, True)
                pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), True)
                pc.set_control_rotation(unreal.Rotator(0.0, -15.0, yaw))
                unreal.log(f"GAMESEQ facing cover {near.get_name()} at {at}")
        elif cmd.startswith("front="):
            # Stands the character with this name this far in front of the star robot, this far to its right
            # (front=RRRandomCharacter_3,500,0)
            name, ahead, side = (cmd.split("=")[1].split(",") + ["0"])[:3]
            for r in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomStarRobot):
                spot = r.get_actor_location() + r.get_actor_forward_vector() * float(ahead) + r.get_actor_right_vector() * float(side)
                for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCharacter):
                    if c.get_name() == name:
                        c.set_actor_location(spot + unreal.Vector(0.0, 0.0, 100.0), False, True)
                        unreal.log(f"GAMESEQ in front {name}")
        elif cmd.startswith("moveveh="):
            # Moves the vehicle whose mesh name has this in it by (dx, dy), e.g. to split the star cars up so F rides one
            # instead of combining them (moveveh=StarLegs,0,1500)
            want, dx, dy = cmd.split("=")[1].split(",")
            for v in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomVehicle):
                if any(c.static_mesh and want in c.static_mesh.get_name() for c in v.get_components_by_class(unreal.StaticMeshComponent)):
                    v.set_actor_location(v.get_actor_location() + unreal.Vector(float(dx), float(dy), 50.0), False, True)
                    unreal.log(f"GAMESEQ moved {v.get_name()}")
                    break
        elif cmd.startswith("walk="):
            x, y, secs = (float(v) for v in cmd.split("=")[1].split(","))
            state["walk_dir"] = unreal.Vector(x, y, 0.0)
            state["walk_until"] = t + secs
        elif cmd.startswith("bring="):
            # Puts the character with this name 300 beside my animal friend (bring=RRRandomCharacter_1)
            pawn = pc.get_controlled_pawn()
            for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCharacter):
                if c.get_name() == cmd.split("=")[1]:
                    c.set_actor_location(pawn.get_actor_location() + unreal.Vector(0.0, 300.0, 50.0), False, True)
                    unreal.log(f"GAMESEQ brought {c.get_name()}")
        elif cmd.startswith("goto="):
            # Puts my animal friend at x, y (feet on the floor below 1500), facing yaw: goto=-3600,347,0
            x, y, yaw = (float(v) for v in cmd.split("=")[1].split(","))
            pawn = pc.get_controlled_pawn()
            pawn.set_actor_location(unreal.Vector(x, y, 400.0), False, True)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), True)
            pc.set_control_rotation(unreal.Rotator(0.0, -15.0, yaw))
        elif cmd.startswith("gotoz="):
            # Like goto, at a height (round 95: onto a hill top): gotoz=x,y,z,yaw
            x, y, z, yaw = (float(v) for v in cmd.split("=")[1].split(","))
            pawn = pc.get_controlled_pawn()
            pawn.set_actor_location(unreal.Vector(x, y, z), False, True)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), True)
            pc.set_control_rotation(unreal.Rotator(0.0, -15.0, yaw))
        elif cmd.startswith("place="):
            # Puts the character with this name at my animal friend + (dx, dy) (place=RRRandomCharacter_3,1400,0)
            name, dx, dy = cmd.split("=")[1].split(",")
            base = pc.get_controlled_pawn().get_actor_location()
            for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCharacter):
                if c.get_name() == name:
                    c.set_actor_location(base + unreal.Vector(float(dx), float(dy), 60.0), False, True)
                    unreal.log(f"GAMESEQ placed {name}")
        elif cmd.startswith("watchfrom="):
            # Stands the character with this name at my animal friend + (dx, dy), turned toward it, and looks through
            # its camera (watchfrom=RRRandomCharacter_3,1800,400)
            name, dx, dy = cmd.split("=")[1].split(",")
            pawn = pc.get_controlled_pawn()
            base = pawn.get_actor_location()
            for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCharacter):
                if c.get_name() == name:
                    spot = base + unreal.Vector(float(dx), float(dy), 60.0)
                    look = unreal.MathLibrary.find_look_at_rotation(spot, base)
                    c.set_actor_location(spot, False, True)
                    c.set_actor_rotation(unreal.Rotator(0.0, 0.0, look.yaw), True)
                    if c.get_controller():
                        c.get_controller().set_control_rotation(unreal.Rotator(0.0, -8.0, look.yaw))
                    pc.set_view_target_with_blend(c, 0.0)
                    unreal.log(f"GAMESEQ watching from {name} at {spot}")
        elif cmd.startswith("frontcam="):
            # Round 97: a fixed camera in front of my animal friend (ahead, to the side, up), looking back at it: its front,
            # with what comes at it from ahead flying in past the camera (frontcam=520,180,110)
            ahead, side, height = (float(v) for v in cmd.split("=")[1].split(","))
            pawn = pc.get_controlled_pawn()
            unreal.SystemLibrary.execute_console_command(w, "RRStudioCam 0", pc)
            unreal.SystemLibrary.execute_console_command(w, "RRStudioCamOff", pc)
            cams = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
            if cams and pawn:
                cam = cams[0]
                base = pawn.get_actor_location()
                spot = base + pawn.get_actor_forward_vector() * ahead + pawn.get_actor_right_vector() * side + unreal.Vector(0.0, 0.0, height)
                cam.set_actor_location(spot, False, True)
                cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(spot, base + unreal.Vector(0.0, 0.0, 20.0)), True)
                pc.set_view_target_with_blend(cam, 0.0)
                unreal.log(f"GAMESEQ frontcam at {spot}")
        elif cmd.startswith("pathcam="):
            # Round 97: a fixed camera beside the line ahead of my animal friend (side, height, ahead), looking at the spot that
            # far ahead: frames me and a target in front of me together (pathcam=1100,250,450)
            side, height, ahead = (float(v) for v in cmd.split("=")[1].split(","))
            pawn = pc.get_controlled_pawn()
            unreal.SystemLibrary.execute_console_command(w, "RRStudioCam 0", pc)
            unreal.SystemLibrary.execute_console_command(w, "RRStudioCamOff", pc)
            cams = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
            if cams and pawn:
                cam = cams[0]
                base = pawn.get_actor_location()
                fwd = pawn.get_actor_forward_vector()
                right = pawn.get_actor_right_vector()
                look = base + fwd * ahead
                spot = look + right * side + unreal.Vector(0.0, 0.0, height)
                cam.set_actor_location(spot, False, True)
                cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(spot, look), True)
                pc.set_view_target_with_blend(cam, 0.0)
                unreal.log(f"GAMESEQ pathcam at {spot}")
        elif cmd.startswith("guncam="):
            parts = cmd.split("=")[1].split(",")
            state["guncam"] = (parts[0],) + tuple(float(v) for v in parts[1:])
        elif cmd.startswith("vehcam="):
            parts = cmd.split("=")[1].split(",")
            state["vehcam"] = (parts[0],) + tuple(float(x) for x in parts[1:])
            state.pop("vehcam_actor", None)
        elif cmd == "vehcamoff":
            state.pop("vehcam", None)
            pc.set_view_target_with_blend(pc.get_controlled_pawn(), 0.0)
        elif cmd == "fpsmark":
            state["fps_dt"], state["fps_n"] = 0.0, 0
        elif cmd.startswith("fps="):
            n = max(1, state.get("fps_n", 0))
            ms = state.get("fps_dt", 0.0) / n * 1000.0
            unreal.log(f"GAMESEQ fps {cmd.split('=')[1]} {1000.0 / max(ms, 0.001):.1f} fps ({ms:.2f} ms over {n} frames)")
        elif cmd == "guncamoff":
            state.pop("guncam", None)
            pc.set_view_target_with_blend(pc.get_controlled_pawn(), 0.0)
        elif cmd == "maincam":
            pc.set_view_target_with_blend(pc.get_controlled_pawn(), 0.0)
        elif cmd == "mv":
            # My animal friend's height, velocity, movement mode and jump settings
            pawn = pc.get_controlled_pawn()
            m = pawn.character_movement
            loc = pawn.get_actor_location()
            unreal.log(f"GAMESEQ mv {pawn.get_name()} server={unreal.SystemLibrary.is_server(w)} at {loc.x:.0f},{loc.y:.0f} z {loc.z:.0f} vel {pawn.get_velocity()} mode {m.movement_mode} jumpz {m.jump_z_velocity:.0f} grav {m.gravity_scale:.2f}")
        elif cmd == "speed":
            pawn = pc.get_controlled_pawn()
            v = pawn.get_velocity()
            unreal.log(f"GAMESEQ speed {(v.x * v.x + v.y * v.y) ** 0.5:.0f} max {pawn.character_movement.max_walk_speed:.0f}")
        elif cmd == "z":
            unreal.log(f"GAMESEQ z {pc.get_controlled_pawn().get_actor_location().z:.0f}")
        elif cmd == "proj":
            # Logs where each meteor's body and crust are, on the screen and in the world, and the camera
            cam = unreal.GameplayStatics.get_player_camera_manager(w, 0)
            unreal.log(f"GAMESEQ camera {cam.get_camera_location()} {cam.get_camera_rotation()} fov {cam.get_fov_angle()}")
            for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomMeteor):
                for c in m.get_components_by_class(unreal.SceneComponent):
                    if c.get_name() in ("Body", "Crust"):
                        loc = c.get_world_location()
                        ok, scr = unreal.GameplayStatics.project_world_to_screen(pc, loc, False)
                        vis = c.is_visible() if hasattr(c, "is_visible") else None
                        unreal.log(f"GAMESEQ {c.get_name()} {loc} screen {ok} {scr} visible {vis} scale {c.get_world_scale()}")
        elif cmd.startswith("turn="):
            # Turns the player's view by this yaw
            r = pc.get_control_rotation()
            pc.set_control_rotation(unreal.Rotator(r.roll, r.pitch, r.yaw + float(cmd.split("=")[1])))
        elif cmd.startswith("crust="):
            # Turns every meteor's crust model by this yaw inside its body (finding which way its face looks)
            yaw = float(cmd.split("=")[1])
            for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomMeteor):
                for c in m.get_components_by_class(unreal.StaticMeshComponent):
                    if c.get_name() == "Crust":
                        c.set_relative_rotation(unreal.Rotator(0.0, 0.0, yaw), False, False)
        else:
            unreal.SystemLibrary.execute_console_command(w, cmd, pc)
        unreal.log(f"GAMESEQ {t:.1f} {cmd}")
        state["i"] += 1


unreal.register_slate_post_tick_callback(tick)
unreal.log("GAMESEQ armed")
