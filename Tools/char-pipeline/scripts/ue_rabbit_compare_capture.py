"""Deterministic paired rabbit capture in a disposable game session, never saves the map.

RR_COMPARE_DIR: frame destination; RR_COMPARE_LIMIT: optional short smoke capture.
Each wide screenshot contains both models. Crop its two halves for exactly matched videos.
"""
import json
import math
import os
import time
from pathlib import Path

import unreal as u

OUT = Path(os.environ.get('RR_COMPARE_DIR', 'C:/work/RRRandom/Tools/char-pipeline/output/rabbit_smart_compare/frames'))
OUT.mkdir(parents=True, exist_ok=True)
FPS = 24
N = int(os.environ.get('RR_COMPARE_LIMIT', str(20 * FPS)))
SAMPLES = [int(n) for n in os.environ.get('RR_COMPARE_SAMPLES','').split(',') if n.strip()]
if SAMPLES:N=len(SAMPLES)
state = {'start': time.time(), 'frame': 0, 'stage': 'init', 'ticks': 0}
actors = []
components = []
animations = {}

def world():
    for w in u.ObjectIterator(u.World):
        try:
            pc = u.GameplayStatics.get_player_controller(w, 0)
            if pc and pc.get_controlled_pawn():
                return w, pc
        except Exception:
            pass
    return None, None

def spawn(w, cls, loc, rot=u.Rotator()):
    # UE excludes BlueprintInternalUseOnly spawning functions from Python. The
    # development-session Summon command uses the same World::SpawnActor path.
    before = set(u.GameplayStatics.get_all_actors_of_class(w,cls))
    pc = u.GameplayStatics.get_player_controller(w,0)
    u.SystemLibrary.execute_console_command(w,'EnableCheats',pc)
    u.SystemLibrary.execute_console_command(w,'Summon /Script/Engine.'+cls.__name__,pc)
    created = [a for a in u.GameplayStatics.get_all_actors_of_class(w,cls) if a not in before]
    assert len(created)==1, 'Could not spawn '+cls.__name__
    actor=created[0]
    for component in actor.get_components_by_class(u.SceneComponent):
        component.set_mobility(u.ComponentMobility.MOVABLE)
    actor.set_actor_location(loc,False,True)
    actor.set_actor_rotation(rot,False)
    return actor

def initialize(w, pc):
    # Everything here exists only in this process; all original map actors remain unsaved.
    for actor in u.GameplayStatics.get_all_actors_of_class(w, u.Actor):
        if not isinstance(actor, (u.PlayerController, u.WorldSettings)):
            actor.set_actor_hidden_in_game(True)
    for cmd in ('showhud', 'r.AntiAliasingMethod 2', 'r.MotionBlurQuality 0',
                'r.ScreenPercentage 100', 'r.EyeAdaptationQuality 0', 'r.VSync 0',
                'r.VolumetricFog 0', 'r.BloomQuality 0', 't.MaxFPS 60'):
        u.SystemLibrary.execute_console_command(w, cmd, pc)
    z = 10000.
    cube = u.load_asset('/Engine/BasicShapes/Cube')
    stage_material = u.load_asset('/Game/Developers/RabbitSmartCompare/M_CompareStage')
    for loc, scale in ((u.Vector(0, 0, z-5), u.Vector(12,12,.1)),
                       (u.Vector(-250,0,z+160),u.Vector(.1,12,7))):
        actor = spawn(w,u.StaticMeshActor,loc)
        comp = actor.static_mesh_component
        comp.set_mobility(u.ComponentMobility.MOVABLE)
        comp.set_static_mesh(cube)
        if stage_material:
            comp.set_material(0,stage_material)
        actor.set_actor_scale3d(scale)
    sun = spawn(w,u.DirectionalLight,u.Vector(400,-100,z+400),u.Rotator(0,-35,180))
    light = sun.light_component
    light.set_intensity(10.)
    light.set_light_color(u.LinearColor(1.,.96,.90,1.))
    light.set_cast_shadows(True)
    fill = spawn(w,u.DirectionalLight,u.Vector(200,300,z+250),u.Rotator(0,-20,-135))
    fill.light_component.set_intensity(3.)
    fill.light_component.set_light_color(u.LinearColor(.84,.91,1.,1.))
    fill.light_component.set_cast_shadows(False)
    camera = spawn(w,u.CameraActor,u.Vector(650,0,z+112),u.Rotator(0,0,180))
    cc = camera.get_component_by_class(u.CameraComponent)
    cc.set_projection_mode(u.CameraProjectionMode.ORTHOGRAPHIC)
    cc.set_ortho_width(250.)
    cc.set_aspect_ratio(2.)
    cc.set_constraint_aspect_ratio(False)
    pc.set_view_target_with_blend(camera,0.)
    paths = ['/Game/Characters/Rabbit/SK_Rabbit',
             os.environ.get('RR_COMPARE_SMART','/Game/Developers/RabbitSmartCompare/SK_RabbitSmart')]
    for y,path in zip((125.,-125.),paths):
        mesh = u.load_asset(path)
        assert isinstance(mesh,u.SkeletalMesh),path
        actor = spawn(w,u.SkeletalMeshActor,u.Vector(0,y,z),u.Rotator(0,0,-90))
        actor.set_actor_scale3d(u.Vector(2,2,2))
        comp = actor.skeletal_mesh_component
        comp.set_skeletal_mesh_asset(mesh)
        material_path='/Game/Characters/Rabbit/M_Rabbit' if path==paths[0] else '/Game/Developers/RabbitSmartCompare/M_RabbitSmart'
        material=u.load_asset(material_path)
        assert material,material_path
        comp.set_material(0,material)
        comp.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
        comp.set_editor_property('visibility_based_anim_tick_option',u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        actors.append(actor)
        components.append(comp)
    for name in ('Idle','Walk','Run','Fire','JumpUp','FallIdle','HardLanding'):
        animations[name] = u.load_asset('/Game/Characters/Rabbit/Anims/A_Rabbit_'+name)
        assert animations[name], name
    # The imported game mesh faces +Y; -90 yaw points it toward the +X camera.
    u.log('RABBITCOMPARE initialized '+str([(m.get_name(),m.get_editor_property('skeleton').get_name()) for m in [u.load_asset(p) for p in paths]]))
    state['warm_until']=time.time()+12
    state['stage']='warm'
    state['ticks']=0

def pose(frame):
    t=frame/FPS
    name='Idle'; offset=t; yaw=-90.; lift=0.
    if t<6 or t>=17:name='Rest'
    if 2<=t<6:
        yaw=-90.+(t-2)/4*360
    elif 6<=t<9:
        name='Walk';offset=t-6
    elif 9<=t<12:
        name='Run';offset=t-9;yaw=-65.
    elif 12<=t<14:
        name='Fire';offset=t-12
    elif 14<=t<15.2:
        name='JumpUp';offset=(t-14)*.65
        lift=25*math.sin((t-14)/1.2*math.pi)
    elif 15.2<=t<16:
        name='HardLanding';offset=t-15.2
    elif 17<=t:
        yaw=90.+(t-17)/3*90
    animation=animations.get(name)
    at=offset%max(.01,animation.get_play_length()) if animation else 0
    for actor,comp in zip(actors,components):
        actor.set_actor_rotation(u.Rotator(0,0,yaw),False)
        loc=actor.get_actor_location();loc.z=10000.+lift
        actor.set_actor_location(loc,False,True)
        if state.get('animation')!=name:
            comp.set_animation(animation)
            if animation:
                comp.play(True)
                comp.set_play_rate(0.)
            else:comp.stop()
        if animation:comp.set_position(at,False)
    state['animation']=name

def tick(dt):
    try:
        w,pc=world()
        if not w or time.time()-state['start']<6:return
        if state['stage']=='init':
            initialize(w,pc)
            return
        index=state['frame']
        frame=SAMPLES[index] if SAMPLES else index
        path=OUT/f'frame{frame:04d}.png'
        if state['stage']=='warm':
            if time.time()<state['warm_until']:return
            state['stage']='pose'
        if state['stage']=='pose':
            pose(frame)
            state['ticks']=0
            state['stage']='settle'
        elif state['stage']=='settle':
            state['ticks']+=1
            if state['ticks']>=3:
                u.SystemLibrary.execute_console_command(w,f'HighResShot 1920x960 filename="{path.as_posix()}"',pc)
                state['stage']='save'
        elif state['stage']=='save' and path.exists() and path.stat().st_size>10000:
            if frame%24==0:u.log(f'RABBITCOMPARE frame {frame}/{N}')
            state['frame']+=1
            if state['frame']>=N:
                (OUT.parent/'capture.json').write_text(json.dumps({'fps':FPS,'frames':N,'size':[1920,960],'left':'HD','right':'P2.0','animations':list(animations)}),encoding='utf-8')
                u.log(f'RABBITCOMPARE COMPLETE {N} frames')
                u.SystemLibrary.execute_console_command(w,'quit',pc)
                state['stage']='done'
            else:state['stage']='pose'
    except Exception as exc:
        u.log_error('RABBITCOMPARE '+repr(exc))
        import traceback
        u.log_error(traceback.format_exc())
        state['stage']='error'
        if w:u.SystemLibrary.execute_console_command(w,'quit',pc)

u.register_slate_post_tick_callback(tick)
u.log('RABBITCOMPARE capture armed')
