import unreal


ASSET_PATH = "/Game/Characters/Mannequins/Anims/throw"
SOURCE_MESH_PATH = ASSET_PATH + "/X_Bot_Throw"
TARGET_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
SOURCE_RIG_PATH = ASSET_PATH + "/IKR_XBot"
TARGET_RIG_PATH = ASSET_PATH + "/IKR_Manny"
RETARGETER_PATH = ASSET_PATH + "/RTG_XBot_To_Manny"
SOURCE_ANIMATION_PATH = ASSET_PATH + "/X_Bot_Throw_Anim"
OUTPUT_ANIMATION_PATH = ASSET_PATH + "/MM_Throw_Retargeted"


def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Required asset was not found: " + path)
    return asset


def create_retargeter_if_needed():
    existing = unreal.load_asset(RETARGETER_PATH)
    if existing:
        return existing

    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "RTG_XBot_To_Manny",
        ASSET_PATH,
        unreal.IKRetargeter,
        unreal.IKRetargetFactory(),
    )


def configure_retargeter(retargeter, source_rig, target_rig, source_mesh, target_mesh):
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source_mesh)
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target_mesh)
    controller.add_default_ops()
    controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    # XBot and Manny do not share the same reference arm pose. Align the target
    # chains to the source before baking so the hands stay outside the torso.
    controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    unreal.EditorAssetLibrary.save_loaded_asset(retargeter, False)


def create_or_update_animation(source_animation, source_mesh, target_mesh, retargeter):
    if unreal.EditorAssetLibrary.does_asset_exist(OUTPUT_ANIMATION_PATH):
        unreal.EditorAssetLibrary.delete_asset(OUTPUT_ANIMATION_PATH)

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(SOURCE_ANIMATION_PATH)]
    inputs.source_mesh = source_mesh
    inputs.target_mesh = target_mesh
    inputs.ik_retarget_asset = retargeter
    inputs.search = "X_Bot_Throw_Anim"
    inputs.replace = "MM_Throw_Retargeted"
    inputs.target_path = ASSET_PATH
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not created:
        raise RuntimeError("Batch retargeting did not produce an animation asset")
    return require_asset(OUTPUT_ANIMATION_PATH)


def main():
    source_mesh = require_asset(SOURCE_MESH_PATH)
    target_mesh = require_asset(TARGET_MESH_PATH)
    source_rig = require_asset(SOURCE_RIG_PATH)
    target_rig = require_asset(TARGET_RIG_PATH)
    source_animation = require_asset(SOURCE_ANIMATION_PATH)
    retargeter = create_retargeter_if_needed()
    if not retargeter:
        raise RuntimeError("Unable to create the IK Retargeter asset")

    configure_retargeter(retargeter, source_rig, target_rig, source_mesh, target_mesh)
    output_animation = create_or_update_animation(source_animation, source_mesh, target_mesh, retargeter)
    if output_animation.get_editor_property("skeleton") != target_mesh.get_editor_property("skeleton"):
        raise RuntimeError("Retargeted animation is not bound to Manny's skeleton")

    unreal.EditorAssetLibrary.save_loaded_asset(output_animation, False)
    unreal.log("[ThrowRetarget] Output: {0}".format(output_animation.get_path_name()))
    unreal.log("[ThrowRetarget] Length: {0}".format(output_animation.get_play_length()))
    unreal.log("[ThrowRetarget] Skeleton: {0}".format(output_animation.get_editor_property("skeleton").get_path_name()))


if __name__ == "__main__":
    main()
