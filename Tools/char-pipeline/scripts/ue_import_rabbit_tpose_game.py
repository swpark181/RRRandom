"""Import the user's real Mixamo T-pose rig and corrected back texture, plus all game motions.

Before running, move the old Content/Characters/Rabbit folder to a local backup
while Unreal is closed. This wrapper uses the existing animal import pipeline.
"""
from pathlib import Path
import sys
import unreal as u

scripts=Path(__file__).resolve().parent
work=scripts.parent/'input/rabbit_smart_tpose'
source=Path('C:/work/char-pipeline/input/rabbit_mjc')
hips=26.67163908481598
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')

def run(name,argument):
    path=scripts/name;old=sys.argv;sys.argv=[str(path),argument]
    try:exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),{'__name__':'__main__','__file__':str(path)})
    finally:sys.argv=old

def validate():
    eal=u.EditorAssetLibrary;mesh=u.load_asset('/Game/Characters/Rabbit/SK_Rabbit')
    assert isinstance(mesh,u.SkeletalMesh)
    skeleton=mesh.get_editor_property('skeleton')
    names=['Idle','Walk','Fire','Death','Victory','AimIdle','AimRunF','AimRunB','AimRunL','AimRunR',
           'Dance1','Dance2','Dance3','Dance4','Defeat1','Defeat2','Defeat3','Run','FlyBack',
           'JumpUp','FrontFlip','FallIdle','HardLanding']
    for name in names:
        anim=u.load_asset('/Game/Characters/Rabbit/Anims/A_Rabbit_'+name)
        assert isinstance(anim,u.AnimSequence),name
        assert anim.get_editor_property('skeleton')==skeleton,name
        assert anim.get_play_length()>0,name
        assert u.AnimationLibrary.get_num_keys(anim)<=anim.get_play_length()*31+2,name+' oversampled'
    pose=u.AnimPoseExtensions.get_reference_pose(skeleton)
    bones=u.AnimPoseExtensions.get_bone_names(pose)
    assert len(bones)==57,len(bones)
    hip=u.AnimPoseExtensions.get_ref_bone_pose(pose,'Hips',u.AnimPoseSpaces.WORLD)
    assert abs(hip.translation.z-hips)<.05,str(hip.translation)
    assert u.load_asset('/Game/Characters/Rabbit/Textures/T_Rabbit_BaseColor')
    eal.save_directory('/Game/Characters/Rabbit')
    u.log('RABBITTPOSE VERIFIED 57 bones, 23 animations, hips '+str(hips))

if '--validate-only' not in sys.argv:
    if '--animations-only' not in sys.argv:
        run('ue_import_animal.py',f'Rabbit|{work}/mixamo|{work}/rabbit_tpose_basecolor_clean.png')
    run('ue_import_rig_anims.py',f'Rabbit|{hips}|{source}/mixamo|rabbit|18.5')
    run('ue_import_rig_anims.py',f'Rabbit|{hips}|{source}/mixamo_extra|rabbit|18.5'
        '|jumpup=JumpUp|frontflip=FrontFlip|fallidle=FallIdle|hardlanding=HardLanding')
    run('ue_import_shared_anim.py','C:/work/char-pipeline/input/shared_anims/dragon_rifle_run.fbx|Run|20.4|Rabbit='+str(hips))
    run('ue_import_shared_anim.py',f'{source}/mixamo/rabbit_flyback.fbx|FlyBack|18.5|Rabbit={hips}')
    if '--animations-only' not in sys.argv:
        run('ue_fur_rim.py','/Game/Characters/Rabbit/M_Rabbit|dither')
        run('ue_set_scalar_defaults.py','/Game/Characters/Rabbit/M_Rabbit|FurMaskFrom=0.4|FurWhiteFrom=0.25|FurRimStrength=0.45|FurSheen=0.02|FurEdgeAmount=0.45')
validate()
