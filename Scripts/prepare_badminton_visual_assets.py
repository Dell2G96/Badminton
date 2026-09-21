import unreal, json
from pathlib import Path
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
static = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.StaticMeshEditorSubsystem()
folder = "/Game/Badminton/Models/Gameplay"
net_path = folder + "/SM_Badminton_Net_Detailed"
source_net = unreal.load_asset("/Game/Badminton/Prototype/SM_SM_Badminton_Court_Details")
net = unreal.load_asset(net_path) if assets.does_asset_exist(net_path) else assets.duplicate_asset(source_net.get_path_name(), net_path)
mesh = unreal.DynamicMesh()
result = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(source_net, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
unreal.log("NET_COPY " + str(result))
# Remove court markings outside the existing net assembly. Preserve original materials and UVs.
result = unreal.GeometryScript_MeshSelection.select_mesh_elements_in_box(mesh, unreal.Box(unreal.Vector(-400,300,-1),unreal.Vector(400,400,160)), invert=False)
unreal.log("NET_SELECTION " + str(result))
selection = unreal.GeometryScript_MeshSelection.invert_mesh_selection(mesh,result[1])[1]
result = unreal.GeometryScript_MeshEdits.delete_selected_triangles_from_mesh(mesh, selection)
unreal.log("NET_DELETE " + str(result))
# Original court runs along Y with its centre at Y=350; gameplay court runs along X.
transform = unreal.Transform(location=unreal.Vector(350,0,0),rotation=unreal.Rotator(pitch=0,yaw=90,roll=0),scale=unreal.Vector(1,1,155.0/156.5))
unreal.GeometryScript_MeshTransforms.transform_mesh(mesh,transform)
result = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh,net,unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD())
unreal.log("NET_WRITE " + str(result))
bounds = net.get_bounds()
assert abs(bounds.origin.x) < 2 and abs(bounds.origin.y) < 2, str(bounds)
assert bounds.box_extent.x < 30 and 300 < bounds.box_extent.y < 340 and 75 < bounds.box_extent.z < 80, str(bounds)
assert assets.save_loaded_asset(net), "Net save failed"
shuttle_path = folder + "/SM_Badminton_Shuttle"
source_shuttle = unreal.load_asset("/Game/Badminton/Models/Shuttle/SM_Badminton_Shuttle_Source")
shuttle = unreal.load_asset(shuttle_path) if assets.does_asset_exist(shuttle_path) else assets.duplicate_asset(source_shuttle.get_path_name(),shuttle_path)
options = unreal.StaticMeshReductionOptions()
options.auto_compute_lod_screen_size = False
options.reduction_settings = [unreal.StaticMeshReductionSettings(percent_triangles=1.0,screen_size=1.0),unreal.StaticMeshReductionSettings(percent_triangles=0.10,screen_size=0.16),unreal.StaticMeshReductionSettings(percent_triangles=0.015,screen_size=0.035)]
if shuttle.get_num_lods() < 3:
    unreal.log("SHUTTLE_LODS " + str(static.set_lods(shuttle,options)))
assert assets.save_loaded_asset(shuttle), "Shuttle save failed"
report = {"net":net.get_path_name(),"net_bounds":str(net.get_bounds()),"net_triangles":mesh.get_triangle_count(),"shuttle":shuttle.get_path_name(),"shuttle_bounds":str(shuttle.get_bounds()),"shuttle_lods":shuttle.get_num_lods()}
Path(r"D:/_Projects/FPS_Dell2g/Docs/26.09.15/AssetUpgradeWork/prepared_assets.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
unreal.log("BADMINTON_VISUAL_ASSETS_READY " + json.dumps(report))

