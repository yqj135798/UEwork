import unreal


asset_path = "/Game/FreeAnimationsPack2/Animations/AS_Eats"
animation = unreal.load_asset(asset_path)
if not animation:
    raise RuntimeError("AS_Eats was not found")

skeleton = animation.get_editor_property("skeleton")
unreal.log("[EatInspect] animation=" + animation.get_path_name())
unreal.log("[EatInspect] skeleton=" + (skeleton.get_path_name() if skeleton else "None"))

registry = unreal.AssetRegistryHelpers.get_asset_registry()
for data in registry.get_assets_by_path("/Game", True):
    if data.asset_class_path.asset_name == "SkeletalMesh":
        mesh = unreal.load_asset(data.package_name)
        if mesh and mesh.get_editor_property("skeleton") == skeleton:
            unreal.log("[EatInspect] matching_mesh=" + mesh.get_path_name())
