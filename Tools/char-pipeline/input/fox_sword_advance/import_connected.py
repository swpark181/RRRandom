import runpy,sys,unreal
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.Enable 0')
root='C:/work/RRRandom/Tools/char-pipeline/input/fox_sword_advance/'
for i in range(1,4):
    sys.argv=['import_motion.py',f'{root}connected_{i}.fbx|SwordDedicated{i}|23.2|Fox=19.8']
    runpy.run_path('C:/work/RRRandom/Tools/char-pipeline/input/fox_sword_advance/import_motion.py',run_name='__main__')

# Adjacent cuts must share their boundary pose after retargeting as well as before.
import math
AL=unreal.AnimationLibrary
clips=[unreal.load_asset(f'/Game/Characters/Fox/Anims/A_Fox_SwordDedicated{i}') for i in range(1,4)]
for first,second in zip(clips,clips[1:]):
    worst_angle=0.; worst_pos=0.
    for bone in AL.get_animation_track_names(first):
        a=AL.get_bone_pose_for_frame(first,bone,AL.get_num_keys(first)-1,False)
        b=AL.get_bone_pose_for_frame(second,bone,0,False)
        dot=sum(getattr(a.rotation,k)*getattr(b.rotation,k) for k in ['x','y','z','w'])
        worst_angle=max(worst_angle,math.degrees(2*math.acos(min(1,abs(dot)))))
        worst_pos=max(worst_pos,(a.translation-b.translation).length())
    unreal.log(f'CONNECTED_BOUNDARY {first.get_name()} -> {second.get_name()}: {worst_angle:.4f} deg, {worst_pos:.5f} cm')
    assert worst_angle<0.1 and worst_pos<0.001

