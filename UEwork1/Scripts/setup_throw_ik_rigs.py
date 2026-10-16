import unreal


ASSET_PATH = "/Game/Characters/Mannequins/Anims/throw"
SOURCE_RIG_PATH = ASSET_PATH + "/IKR_XBot"
TARGET_RIG_PATH = ASSET_PATH + "/IKR_Manny"
TARGET_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"


def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Required asset was not found: " + path)
    return asset


def create_target_rig_if_needed():
    existing = unreal.load_asset(TARGET_RIG_PATH)
    if existing:
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    return asset_tools.create_asset(
        "IKR_Manny",
        ASSET_PATH,
        unreal.IKRigDefinition,
        unreal.IKRigDefinitionFactory(),
    )


def configure_rig(label, rig, mesh):
    controller = unreal.IKRigController.get_controller(rig)
    if controller.get_skeletal_mesh() != mesh:
        if not controller.set_skeletal_mesh(mesh):
            raise RuntimeError(label + " could not use its requested skeletal mesh")

    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError(label + " could not generate its retarget definition")

    chains = controller.get_retarget_chains()
    if not chains:
        raise RuntimeError(label + " has no retarget chains after auto-generation")

    unreal.log("[ThrowRigSetup] {0} chains: {1}".format(label, chains))
    unreal.EditorAssetLibrary.save_loaded_asset(rig, False)


def main():
    source_rig = require_asset(SOURCE_RIG_PATH)
    source_mesh = require_asset("/Game/Characters/Mannequins/Anims/throw/X_Bot_Throw")
    target_mesh = require_asset(TARGET_MESH_PATH)
    target_rig = create_target_rig_if_needed()
    if not target_rig:
        raise RuntimeError("Unable to create IKR_Manny")

    configure_rig("XBot", source_rig, source_mesh)
    configure_rig("Manny", target_rig, target_mesh)
    unreal.log("[ThrowRigSetup] Complete: {0}".format(TARGET_RIG_PATH))


if __name__ == "__main__":
    main()
