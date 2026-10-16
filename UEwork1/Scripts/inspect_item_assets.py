import os
import unreal

output_dir = r"D:\unreal_engine\program\work1\UEwork1\Saved\ItemTexturePreviews"
os.makedirs(output_dir, exist_ok=True)

texture_paths = [
    "/Game/Fab/Ball/ball/Textures/ball_texture_0",
    "/Game/Fab/Ball/ball/Textures/ball_texture_1",
    "/Game/Sword/Textures/T_Sword_low_Sword_BaseColor",
    "/Game/Fab/Food__fish__chicken__meat__3d_models_low_poly_/food_fish_chicken_meat3d_models_low_poly/Textures/food_fish_chicken_meat3d_models_low_poly_texture_0",
    "/Game/Fab/Food__fish__chicken__meat__3d_models_low_poly_/food_fish_chicken_meat3d_models_low_poly/Textures/food_fish_chicken_meat3d_models_low_poly_texture_1",
    "/Game/Fab/Food__fish__chicken__meat__3d_models_low_poly_/food_fish_chicken_meat3d_models_low_poly/Textures/food_fish_chicken_meat3d_models_low_poly_texture_2",
    "/Game/Fab/Food__fish__chicken__meat__3d_models_low_poly_/food_fish_chicken_meat3d_models_low_poly/Textures/food_fish_chicken_meat3d_models_low_poly_texture_4",
]

for texture_path in texture_paths:
    texture = unreal.load_asset(texture_path)
    if not texture:
        unreal.log_error("[ItemInspect] missing texture=" + texture_path)
        continue
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = os.path.join(output_dir, texture.get_name() + ".png")
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    task.exporter = unreal.TextureExporterPNG()
    unreal.Exporter.run_asset_export_task(task)
    unreal.log("[ItemInspect] exported=" + task.filename)


paths = [
    "/Game/Fab/Ball/ball/StaticMeshes/Sphere001_Material__26_0",
    "/Game/Sword/Sword/SM_Sword",
    "/Game/Fab/Food__fish__chicken__meat__3d_models_low_poly_/food_fish_chicken_meat3d_models_low_poly/StaticMeshes/food_fish_chicken_meat3d_models_low_poly",
]

for path in paths:
    asset = unreal.load_asset(path)
    if not asset:
        unreal.log_error("[ItemInspect] missing=" + path)
        continue
    unreal.log("[ItemInspect] asset=" + asset.get_path_name())
    if hasattr(asset, "get_bounds"):
        bounds = asset.get_bounds()
        unreal.log("[ItemInspect] bounds=" + str(bounds))
    if hasattr(asset, "get_editor_property"):
        materials = asset.get_editor_property("static_materials")
        for index, material_slot in enumerate(materials):
            material = material_slot.material_interface
            unreal.log("[ItemInspect] material_{0}={1}".format(index, material.get_path_name() if material else "None"))
