"""Validate the real Mixamo rabbit, then exercise it through RR_SEQ in a game session."""
from pathlib import Path
import unreal as u

folder='/Game/Characters/Rabbit'
mesh=u.load_asset(folder+'/SK_Rabbit')
assert isinstance(mesh,u.SkeletalMesh)
skeleton=mesh.get_editor_property('skeleton')
names=['Idle','Walk','Fire','Death','Victory','AimIdle','AimRunF','AimRunB','AimRunL','AimRunR',
       'Dance1','Dance2','Dance3','Dance4','Defeat1','Defeat2','Defeat3','Run','FlyBack',
       'JumpUp','FrontFlip','FallIdle','HardLanding']
for name in names:
    anim=u.load_asset(folder+'/Anims/A_Rabbit_'+name)
    assert isinstance(anim,u.AnimSequence),name
    assert anim.get_editor_property('skeleton')==skeleton,name
    assert anim.get_play_length()>0,name
    assert u.AnimationLibrary.get_num_keys(anim)<=anim.get_play_length()*31+2,name+' oversampled'
pose=u.AnimPoseExtensions.get_reference_pose(skeleton)
assert len(u.AnimPoseExtensions.get_bone_names(pose))==57
hips=u.AnimPoseExtensions.get_ref_bone_pose(pose,'Hips',u.AnimPoseSpaces.WORLD)
assert abs(hips.translation.z-26.671639)<.05,str(hips.translation)
tex=u.load_asset(folder+'/Textures/T_Rabbit_BaseColor')
assert tex.blueprint_get_size_x()==2048 and tex.blueprint_get_size_y()==2048
assert u.load_asset(folder+'/M_Rabbit')
u.log('RABBITTPOSE SMOKE VERIFIED 57 bones, 23 motions, hips 26.6716, corrected 2K texture')
path=Path(__file__).with_name('ue_game_seq.py')
exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),{'__name__':'__main__','__file__':str(path)})
