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


def subject(pc):
    # Whose face and fur the fur commands work on: the character picked with target=, or my animal friend
    t = state.get("target")
    if t is not None and unreal.SystemLibrary.is_valid(t):
        return t
    return pc.get_controlled_pawn()


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
        elif cmd.startswith("walkyaw="):
            # walkyaw=<degrees to the right of where my animal friend faces now>,<seconds>
            deg, secs = (float(v) for v in cmd.split("=")[1].split(","))
            r = pc.get_controlled_pawn().get_actor_rotation()
            f = unreal.MathLibrary.get_forward_vector(unreal.Rotator(0.0, 0.0, r.yaw + deg))
            state["walk_dir"] = unreal.Vector(f.x, f.y, 0.0)
            state["walk_until"] = t + secs
        elif cmd.startswith("walkfwd="):
            # walkfwd=<seconds>: like walk=, the way my animal friend faces now (2026-11-20: toward a cover it was put in front of)
            f = pc.get_controlled_pawn().get_actor_forward_vector()
            state["walk_dir"] = unreal.Vector(f.x, f.y, 0.0)
            state["walk_until"] = t + float(cmd.split("=")[1])
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
        elif cmd.startswith("facecam="):
            # 2026-11-20: a fixed camera looking at my animal friend's face: facecam=<distance>,<yaw round from its front>
            # [,<up from the head bone, 14>[,<camera height over the face, 0>]] (fur close-ups at set distances)
            vals = [float(v) for v in cmd.split("=")[1].split(",")]
            dist, yaw_off = vals[0], vals[1]
            up = vals[2] if len(vals) > 2 else 14.0
            rise = vals[3] if len(vals) > 3 else 0.0
            pawn = subject(pc)
            unreal.SystemLibrary.execute_console_command(w, "RRStudioCam 0", pc)
            unreal.SystemLibrary.execute_console_command(w, "RRStudioCamOff", pc)
            cams = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
            head = None
            front_yaw = pawn.get_actor_rotation().yaw
            for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
                if c.is_visible() and c.get_num_bones() > 0:
                    for bi in range(c.get_num_bones()):
                        if str(c.get_bone_name(bi)).endswith("Head"):
                            head = c.get_socket_location(c.get_bone_name(bi))
                            break
                if head is not None:
                    # (for a target= character, the front its shoulders face as posed now: a villain's body isn't always
                    # turned the way its actor is; front = up x (left - right). My own animal friend: its actor's front,
                    # the shoulders twist with the aim)
                    sh = {}
                    for bi in range(c.get_num_bones()):
                        bn = str(c.get_bone_name(bi))
                        if bn.endswith("LeftShoulder") or bn.endswith("RightShoulder"):
                            sh[bn[-13:-8] if bn.endswith("RightShoulder") else "Left"] = c.get_socket_location(c.get_bone_name(bi))
                    if "Left" in sh and "Right" in sh and pawn != pc.get_controlled_pawn():
                        side = sh["Left"] - sh["Right"]
                        f = unreal.Vector(-side.y, side.x, 0.0)
                        if f.length() > 0.01:
                            front_yaw = unreal.MathLibrary.make_rot_from_x(f).yaw
                    break
            if cams and head is not None:
                # (a plain camera: on the cinematic's map the first ones are its cine cameras, long lenses with depth of field)
                cam = ([c for c in cams if not isinstance(c, unreal.CineCameraActor)] or cams)[0]
                if isinstance(cam, unreal.CineCameraActor):
                    # (only cine cameras there: a wide lens, everything sharp)
                    lens = cam.get_cine_camera_component()
                    lens.set_editor_property("current_focal_length", 16.0)
                    lens.set_editor_property("current_aperture", 22.0)
                    focus = lens.get_editor_property("focus_settings")
                    focus.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE)
                    lens.set_editor_property("focus_settings", focus)
                face = head + unreal.Vector(0.0, 0.0, up)
                way = unreal.MathLibrary.get_forward_vector(unreal.Rotator(0.0, 0.0, front_yaw + yaw_off))
                spot = face + way * dist + unreal.Vector(0.0, 0.0, rise)
                cam.set_actor_location(spot, False, True)
                cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(spot, face), True)
                pc.set_view_target_with_blend(cam, 0.0)
                unreal.log(f"GAMESEQ facecam at {spot} looking at {face}")
            else:
                unreal.log("GAMESEQ facecam: no head bone or camera")
        elif cmd.startswith("cineface="):
            # 2026-11-23: cineface=<species index>,<expression name or -> puts that expression on the cinematic animal
            # friend of that species standing in the level (ARRRandomCineAnimal; run on the cinematic's own map,
            # /Game/Cinematics/FoxVsTiger/Map_FoxVsTiger: Python cannot spawn an actor in a -game run), the way the
            # cinematic swaps it: body M_<Sp>_Expr_<X> and fur M_<Sp>Fur_Expr_<X> ("-" = the game's own face), and makes
            # it the target of facecam / furparam. To see each expression's fur mask at work.
            vals = cmd.split("=")[1].split(",")
            idx, want = int(vals[0]), vals[1] if len(vals) > 1 else "-"
            actor = state.get("cine_actor")
            if actor is None or not unreal.SystemLibrary.is_valid(actor):
                there = [x for x in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCineAnimal)
                         if x.get_editor_property("species") == idx]
                there.sort(key=lambda x: ("Clone" in x.get_name(), x.get_name()))
                actor = there[0] if there else None
                state["cine_actor"] = actor
            if actor is None:
                unreal.log("GAMESEQ cineface: no cinematic animal friend of that species in this level")
            else:
                names = [m.get_name().split("_Expr_")[-1] if m else "" for m in actor.get_editor_property("expressions")]
                actor.set_editor_property("expression", names.index(want) if want in names else -1)
                state["target"] = actor
                unreal.log(f"GAMESEQ cineface {actor.get_name()} {want} of {names} at {actor.get_actor_location()}")
        elif cmd.startswith("furparam="):
            # furparam=<name>,<value>[,<name>,<value>...]: sets scalar parameters on my animal friend's shell fur material
            # (through a dynamic instance, this run only), to compare values in one run: furparam=TidyNear,-1,TidyFar,-1
            vals = cmd.split("=")[1].split(",")
            pawn = subject(pc)
            for c in pawn.get_components_by_class(unreal.RRRandomFurComponent):
                # (on every slot: the fur mesh keeps the body's material slots, and the game has instances on them all)
                for k in range(0, len(vals) - 1, 2):
                    c.set_scalar_parameter_value_on_materials(vals[k], float(vals[k + 1]))
                unreal.log(f"GAMESEQ furparam {vals} on {c.get_name()}")
        elif cmd.startswith("furvec="):
            # furvec=<name>,<x>,<y>,<z>: a vector parameter on the fur material, like furparam (the face ellipsoid)
            vals = cmd.split("=")[1].split(",")
            pawn = subject(pc)
            for c in pawn.get_components_by_class(unreal.RRRandomFurComponent):
                c.set_vector_parameter_value_on_materials(vals[0], unreal.Vector(float(vals[1]), float(vals[2]), float(vals[3])))
                unreal.log(f"GAMESEQ furvec {vals} on {c.get_name()}")
        elif cmd.startswith("target="):
            # target=<part of a body mesh name>: the nearest character wearing that mesh (target=SK_Cat) becomes the one
            # facecam, furparam, furvec, faceguess, furinfo and headinfo work on (a defense villain); target= alone: me again
            want = cmd.split("=")[1]
            me = pc.get_controlled_pawn()
            best = None
            if want:
                for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomCharacter):
                    names = [m.get_skeletal_mesh_asset().get_name() for m in c.get_components_by_class(unreal.SkeletalMeshComponent)
                             if m.get_skeletal_mesh_asset() and m.is_visible()]
                    if c != me and any(want in nm for nm in names):
                        d = (c.get_actor_location() - me.get_actor_location()).length()
                        if best is None or d < best[0]:
                            best = (d, c)
            state["target"] = best[1] if best else None
            unreal.log(f"GAMESEQ target {best[1].get_name() if best else None} {('%.0f' % best[0]) if best else ''}")
        elif cmd.startswith("tplace="):
            # tplace=<dx>,<dy>,<dz>,<yaw>: puts the target= character at my animal friend + (dx, dy, dz), turned to that
            # yaw (a villain lifted out of the crowd into the open sky; with RRSlomo 0.01 it hangs there)
            dx, dy, dz, yaw = (float(v) for v in cmd.split("=")[1].split(","))
            who = subject(pc)
            me = pc.get_controlled_pawn()
            if who != me:
                who.set_actor_location(me.get_actor_location() + unreal.Vector(dx, dy, dz), False, True)
                who.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), True)
                unreal.log(f"GAMESEQ tplace {who.get_name()} at {who.get_actor_location()}")
        elif cmd == "bones":
            # Logs every bone of the subject's body with its place in the mesh's own space, as it stands now
            pawn = subject(pc)
            for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
                if not c.is_visible() or c.get_num_bones() == 0 or isinstance(c, unreal.RRRandomFurComponent):
                    continue
                inv = c.get_world_transform().inverse()
                unreal.log(f"GAMESEQ bones {c.get_skeletal_mesh_asset().get_name()} scale {c.get_world_scale()}")
                for bi in range(c.get_num_bones()):
                    here = unreal.MathLibrary.transform_location(inv, c.get_socket_location(c.get_bone_name(bi)))
                    unreal.log(f"GAMESEQ bone {c.get_bone_name(bi)} {here.x:.1f},{here.y:.1f},{here.z:.1f}")
                break
        elif cmd.startswith("faceguess"):
            # faceguess[=<ahead>,<up>,<rx>,<ry>,<rz>] (shares of the head's height, Head bone to HeadTop_End; the fox's:
            # 0.63,0.38,0.585,0.43,0.41): a first inner-face ellipsoid for the fur material, from the head bones as they
            # stand now, set on the fur and logged as "x,y,z,rx,ry,rz" (then look at it with furparam=TidyShow,1)
            k = [float(v) for v in cmd.split("=")[1].split(",")] if "=" in cmd else [0.63, 0.38, 0.585, 0.43, 0.41]
            pawn = subject(pc)
            head = top = None
            for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
                if not c.is_visible() or c.get_num_bones() == 0 or isinstance(c, unreal.RRRandomFurComponent):
                    continue
                inv = c.get_world_transform().inverse()
                for bi in range(c.get_num_bones()):
                    bn = str(c.get_bone_name(bi))
                    if not (bn.endswith("HeadTop_End") or bn.endswith("Head")):
                        continue
                    # (the material's ellipsoid is in the reference pose: a running villain's head leans well ahead of it)
                    try:
                        here = c.get_ref_pose_position(bi)
                        state["guess_pose"] = "reference pose"
                    except Exception:
                        here = unreal.MathLibrary.transform_location(inv, c.get_socket_location(c.get_bone_name(bi)))
                        state["guess_pose"] = "the pose now"
                    if bn.endswith("HeadTop_End"):
                        top = here
                    else:
                        head = here
                break
            if head is not None and top is not None:
                hgt = (top - head).length()
                g = [head.x, head.y + k[0] * hgt, head.z + k[1] * hgt, k[2] * hgt, k[3] * hgt, k[4] * hgt]
                for c in pawn.get_components_by_class(unreal.RRRandomFurComponent):
                    c.set_vector_parameter_value_on_materials("TidyFaceAt", unreal.Vector(g[0], g[1], g[2]))
                    c.set_vector_parameter_value_on_materials("TidyFaceSize", unreal.Vector(g[3], g[4], g[5]))
                unreal.log("GAMESEQ faceguess (%s) head %.1f,%.1f,%.1f height %.1f -> %s" % (state.get("guess_pose"), head.x, head.y, head.z, hgt, ",".join("%.1f" % v for v in g)))
            else:
                unreal.log("GAMESEQ faceguess: no Head / HeadTop_End bones")
        elif cmd == "furinfo":
            # Logs my animal friend's fur mesh, its material on every slot and that material's scalar parameters
            pawn = subject(pc)
            for c in pawn.get_components_by_class(unreal.RRRandomFurComponent):
                unreal.log(f"GAMESEQ furinfo {c.get_name()} mesh {c.get_skeletal_mesh_asset().get_name() if c.get_skeletal_mesh_asset() else None} slots {c.get_num_materials()}")
                for si in range(c.get_num_materials()):
                    m = c.get_material(si)
                    base = m.get_base_material() if m else None
                    unreal.log(f"GAMESEQ furinfo slot {si} {m.get_path_name() if m else None} base {base.get_path_name() if base else None}")
                    if base and si == 0:
                        names = [str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(base)]
                        unreal.log(f"GAMESEQ furinfo scalars {names}")
        elif cmd == "headinfo":
            # Logs where my animal friend's head, neck and eye bones are in its body mesh's own space (now, and in the
            # reference pose where the engine tells), to place the fur's face ellipsoid
            pawn = subject(pc)
            for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
                if not c.is_visible() or c.get_num_bones() == 0 or isinstance(c, unreal.RRRandomFurComponent):
                    continue
                inv = c.get_world_transform().inverse()
                for bi in range(c.get_num_bones()):
                    bn = str(c.get_bone_name(bi))
                    if any(k in bn for k in ("Head", "Neck", "Eye", "Jaw")):
                        here = unreal.MathLibrary.transform_location(inv, c.get_socket_location(c.get_bone_name(bi)))
                        unreal.log(f"GAMESEQ headinfo {c.get_name()} {bn} local {here.x:.1f},{here.y:.1f},{here.z:.1f}")
                break
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
