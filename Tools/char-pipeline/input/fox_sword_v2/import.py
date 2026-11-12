import runpy,sys
import unreal
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.Enable 0")
root='C:/work/RRRandom/Tools/char-pipeline/input/fox_sword_v2/'
for motion,suffix in [(0,'SwordDedicated1'),(2,'SwordDedicated2'),(1,'SwordDedicated3')]:
    sys.argv=['ue_import_shared_anim.py',f'{root}motion_{motion}.fbx|{suffix}|23.2|Fox=19.8']
    source=open('C:/work/RRRandom/Tools/char-pipeline/scripts/ue_import_shared_anim.py',encoding='utf-8').read().replace('set_editor_property("use_default_sample_rate", True)', 'set_editor_property("use_default_sample_rate", False)')
    exec(compile(source,'ue_import_shared_anim.py','exec'), {'__name__':'__main__'})



