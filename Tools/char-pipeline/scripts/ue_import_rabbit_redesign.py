"""Import the cream moon rabbit and retarget its existing 23 motions.

Run as an Unreal Python commandlet with the legacy FBX importer. Keep a filesystem
backup of Content/Characters/Rabbit before running: ue_import_animal replaces it.
"""
from pathlib import Path
import sys
import unreal

scripts = Path(__file__).resolve().parent
work = scripts.parent / 'input' / 'rabbit_redesign'
source = Path('C:/work/char-pipeline/input/rabbit_mjc')

# Set this after engine startup as well: newer UE versions may consume the
# command-line deferred cvar after import pipelines have already initialized.
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
unreal.log('RABBITREDESIGN legacy FBX importer requested')

def run(name, argument):
    path = scripts / name
    old = sys.argv
    sys.argv = [str(path), argument]
    try:
        exec(compile(path.read_text(encoding='utf-8'), str(path), 'exec'),
             {'__name__':'__main__', '__file__':str(path)})
    finally:
        sys.argv = old

def validate():
    eal = unreal.EditorAssetLibrary
    mesh = unreal.load_asset('/Game/Characters/Rabbit/SK_Rabbit')
    animations = [unreal.load_asset(p) for p in eal.list_assets('/Game/Characters/Rabbit/Anims')]
    assert len(animations) == 23, f'Expected 23 motions, got {len(animations)}'
    skeleton = mesh.get_editor_property('skeleton')
    assert all(isinstance(a,unreal.AnimSequence) and a.get_editor_property('skeleton') == skeleton
               for a in animations)
    assert all(a.get_play_length() > 0 for a in animations)
    eal.save_directory('/Game/Characters/Rabbit')
    unreal.log('RABBITREDESIGN DONE: mesh, portrait, fur material and 23 retargeted motions')

if '--validate-only' in sys.argv:
    validate()
    sys.exit(0)

run('ue_import_animal.py', f'Rabbit|{work}/mixamo|{work}/rabbit_cream_clean.png')
run('ue_import_rig_anims.py', f'Rabbit|24|{source}/mixamo|rabbit|18.5')
run('ue_import_rig_anims.py', f'Rabbit|24|{source}/mixamo_extra|rabbit|18.5'
    '|jumpup=JumpUp|frontflip=FrontFlip|fallidle=FallIdle|hardlanding=HardLanding')
run('ue_import_shared_anim.py',
    'C:/work/char-pipeline/input/shared_anims/dragon_rifle_run.fbx|Run|20.4|Rabbit=24')
run('ue_import_shared_anim.py', f'{source}/mixamo/rabbit_flyback.fbx|FlyBack|18.5|Rabbit=24')
run('ue_fur_rim.py', '/Game/Characters/Rabbit/M_Rabbit|dither')
run('ue_set_scalar_defaults.py', '/Game/Characters/Rabbit/M_Rabbit|FurMaskFrom=0.4'
    '|FurWhiteFrom=0.25|FurRimStrength=0.45|FurSheen=0.02|FurEdgeAmount=0.45')
run('ue_import_ui_texture.py', f'/Game/UI|{work}/T_RabbitPortrait.png=T_RabbitPortrait')

validate()
