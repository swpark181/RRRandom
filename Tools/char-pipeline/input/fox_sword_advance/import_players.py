"""Quick rollout of the connected fox sword phrase to the other playable rigs."""
import json, runpy, sys
from pathlib import Path
import unreal

root=Path('C:/work/RRRandom/Tools/char-pipeline/input/fox_sword_advance')
out=Path('C:/work/RRRandom/Saved/SwordPlayers')
out.mkdir(parents=True,exist_ok=True)
targets={'Rabbit':26.6716,'Squirrel':21.5,'Tiger':25.6,'Dragon':22.0,'RedFox':22.4}
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.Enable 0')
for i in range(1,4):
    args='|'.join(f'{name}={height}' for name,height in targets.items())
    sys.argv=['import_motion.py',f'{root}/connected_{i}.fbx|SwordDedicated{i}|23.2|{args}']
    runpy.run_path(str(root/'import_motion.py'),run_name='__main__')

report=[]
for name in targets:
    mesh=unreal.load_asset(f'/Game/Characters/{name}/SK_{name}')
    skeleton=mesh.get_editor_property('skeleton')
    pose=unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    names={str(n).lower() for n in unreal.AnimPoseExtensions.get_bone_names(pose)}
    required=['Hips','Spine','Spine1','Spine2','Neck','Head']
    required += [s+b for s in ['Left','Right'] for b in ['Shoulder','Arm','ForeArm','Hand','HandIndex1','UpLeg','Leg','Foot','ToeBase']]
    assert all(n.lower() in names for n in required),name
    for i in range(1,4):
        path=f'/Game/Characters/{name}/Anims/A_{name}_SwordDedicated{i}'
        clip=unreal.load_asset(path)
        assert isinstance(clip,unreal.AnimSequence),path
        assert clip.get_editor_property('skeleton')==skeleton,path
        keys=unreal.AnimationLibrary.get_num_keys(clip)
        duration=unreal.AnimationLibrary.get_sequence_length(clip)
        assert keys>=18 and .55<duration<.65,(path,keys,duration)
        report.append({'asset':path,'keys':keys,'duration':duration,'skeleton':skeleton.get_path_name()})
(out/'imports.json').write_text(json.dumps(report,indent=2))
unreal.log(f'SWORD_PLAYERS_IMPORT_OK {len(report)} clips on {len(targets)} rigs')
