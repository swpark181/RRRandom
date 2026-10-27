"""Verify the redesigned rabbit's asset set, then run RR_SEQ in a game session."""
from pathlib import Path
import unreal

folder = '/Game/Characters/Rabbit'
mesh = unreal.load_asset(f'{folder}/SK_Rabbit')
assert isinstance(mesh, unreal.SkeletalMesh)
skeleton = mesh.get_editor_property('skeleton')
names = ['Idle','Walk','Fire','Death','Victory','AimIdle','AimRunF','AimRunB','AimRunL','AimRunR',
         'Dance1','Dance2','Dance3','Dance4','Defeat1','Defeat2','Defeat3','Run','FlyBack',
         'JumpUp','FrontFlip','FallIdle','HardLanding']
for name in names:
    asset = unreal.load_asset(f'{folder}/Anims/A_Rabbit_{name}')
    assert isinstance(asset, unreal.AnimSequence), name
    assert asset.get_editor_property('skeleton') == skeleton, name
    assert asset.get_play_length() > 0, name
portrait = unreal.load_asset('/Game/UI/T_RabbitPortrait')
assert isinstance(portrait, unreal.Texture2D)
assert portrait.blueprint_get_size_x() == 256
pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
hips = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, 'Hips', unreal.AnimPoseSpaces.WORLD)
assert abs(hips.translation.z - 24) < .1, str(hips.translation)
unreal.log('RABBITREDESIGN VERIFIED: 23 animations share the new skeleton, hips 24, portrait 256px')
sequence = Path(__file__).with_name('ue_game_seq.py')
exec(compile(sequence.read_text(encoding='utf-8'), str(sequence), 'exec'),
     {'__name__':'__main__','__file__':str(sequence)})
