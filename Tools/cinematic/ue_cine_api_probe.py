# Lists what the editor's Python offers for Control Rig tracks in Sequencer (2026-11-24): whether an FK or layered
# Control Rig track and its keys can be made by script, as the rest of the cinematic is.
import unreal

lib = getattr(unreal, "ControlRigSequencerLibrary", None)
unreal.log(f"CINEBUILD ControlRigSequencerLibrary: {bool(lib)}; FKControlRig: {hasattr(unreal, 'FKControlRig')}")
if lib:
    names = [n for n in dir(lib) if not n.startswith("_")]
    for key in ("track", "layer", "additive", "bake", "transform", "fk"):
        unreal.log(f"CINEBUILD   {key}: {[n for n in names if key in n][:14]}")
for cls in ("MovieSceneControlRigParameterTrack", "MovieSceneControlRigParameterSection", "MovieSceneAnimationMixerTrack", "MovieSceneAnimMixerTrack"):
    unreal.log(f"CINEBUILD {cls}: {hasattr(unreal, cls)}")
unreal.log(f"CINEBUILD mixer-like classes: {[n for n in dir(unreal) if 'mixer' in n.lower() and 'anim' in n.lower()][:12]}")
unreal.log("CINEBUILD DONE")
