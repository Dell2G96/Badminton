import unreal, json
from pathlib import Path
out = Path(r"D:/_Projects/FPS_Dell2g/Docs/26.09.15/AssetUpgradeWork")
paths = ["/Game/Badminton/Models/Shuttle/SM_Badminton_Shuttle_Source", "/Game/Badminton/Models/Court/SM_Badminton_Court_Source", "/Game/Badminton/Prototype/SM_SM_Badminton_Court_Details"]
report = []
for path in paths:
    mesh = unreal.load_asset(path)
    bounds = mesh.get_bounds()
    info = {"path":path,"bounds_origin":str(bounds.origin),"extent":str(bounds.box_extent),"sections":mesh.get_num_sections(0),"materials":[str(x.material_interface.get_path_name()) if x.material_interface else None for x in mesh.get_editor_property("static_materials")]}
    try: info["source"] = list(mesh.get_editor_property("asset_import_data").extract_filenames())
    except Exception as e: info["source_error"] = str(e)
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = str(out / (mesh.get_name()+".obj"))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.StaticMeshExporterOBJ()
    info["exported"] = unreal.Exporter.run_asset_export_task(task)
    report.append(info)
    unreal.log("BADMINTON_ASSET_INSPECT " + json.dumps(info))
(out/"asset_inventory.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
unreal.log("BADMINTON_ASSET_INSPECT_DONE")
