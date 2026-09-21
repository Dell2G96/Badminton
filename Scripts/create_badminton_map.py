"""UE Python commandlet: create the dedicated badminton test map without changing existing maps."""

import unreal

MAP = "/Game/Badminton/Maps/L_Badminton_Prototype"
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if assets.does_asset_exist(MAP):
    raise RuntimeError(f"Map already exists; refusing to overwrite: {MAP}")
if not levels.new_level(MAP):
    raise RuntimeError("Could not create map")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property(
    "default_game_mode", unreal.load_class(None, "/Script/FPS_Dell2g.BadmintonGameMode")
)
court = actors.spawn_actor_from_class(
    unreal.load_class(None, "/Script/FPS_Dell2g.BadmintonCourt"), unreal.Vector(0, 0, 0)
)
court.set_actor_label("Badminton Court - Network Prototype")
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(-55, -35, 0))
sun.light_component.set_editor_property("intensity", 5.0)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500))
sky.light_component.set_editor_property("intensity", 1.0)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
if not levels.save_current_level():
    raise RuntimeError("Could not save map")
unreal.log("BADMINTON_MAP_CREATED " + MAP)
