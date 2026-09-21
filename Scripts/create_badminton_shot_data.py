"""Create the editable MVP shot data once; preserve an existing tuned asset."""
import unreal

path = "/Game/Badminton/Data/DA_BadmintonShots"
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
if assets.does_asset_exist(path):
    unreal.log("BADMINTON_SHOT_DATA_EXISTS " + path)
else:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.load_class(None, "/Script/FPS_Dell2g.BadmintonShotData"))
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_BadmintonShots", "/Game/Badminton/Data", None, factory)
    if not asset or not assets.save_loaded_asset(asset):
        raise RuntimeError("Could not save badminton shot data")
    unreal.log("BADMINTON_SHOT_DATA_CREATED " + path)
