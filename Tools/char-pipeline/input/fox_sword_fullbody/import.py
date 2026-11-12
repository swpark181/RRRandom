import runpy,sys,unreal
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.Enable 0')
root='C:/work/RRRandom/Tools/char-pipeline/input/fox_sword_fullbody/'
for i,suffix in [(0,'SwordDedicated1'),(2,'SwordDedicated2'),(1,'SwordDedicated3')]:
    sys.argv=['import_motion.py',f'{root}motion_{i}.fbx|{suffix}|23.2|Fox=19.8']
    runpy.run_path(root+'import_motion.py',run_name='__main__')
