import unreal


SOURCE_FOLDER = "/Game/Characters/Mannequins/Anims/sword"
OUTPUT_FOLDER = SOURCE_FOLDER + "/Manny"
SOURCE_MESH_PATH = "/Game/Characters/Mannequins/Anims/throw/X_Bot_Throw"
TARGET_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
RETARGETER_PATH = "/Game/Characters/Mannequins/Anims/throw/RTG_XBot_To_Manny"

ANIMATIONS = {
    "draw_a_great_sword_1": "MM_GreatSword_Draw",
    "draw_a_great_sword_2": "MM_GreatSword_Draw_FollowUp",
    "great_sword_slash": "MM_GreatSword_Attack_Left",
    "great_sword_slash__2_": "MM_GreatSword_Attack_Right",
    "great_sword_idle": "MM_GreatSword_Idle_01",
    "great_sword_idle__2_": "MM_GreatSword_Idle_02",
    "great_sword_idle__3_": "MM_GreatSword_Idle_03",
    "great_sword_idle__4_": "MM_GreatSword_Idle_04",
    "great_sword_idle__5_": "MM_GreatSword_Idle_05",
    "great_sword_walk": "MM_GreatSword_Walk_Forward",
    "great_sword_walk__2_": "MM_GreatSword_Walk_Backward",
    "great_sword_run": "MM_GreatSword_Run_Backward",
    "great_sword_run__2_": "MM_GreatSword_Run_Forward",
    "great_sword_strafe": "MM_GreatSword_Strafe_Right",
    "great_sword_strafe__2_": "MM_GreatSword_Strafe_Left",
    "great_sword_strafe__3_": "MM_GreatSword_Strafe_Right_Alt",
    "great_sword_strafe__4_": "MM_GreatSword_Strafe_Left_Alt",
    "great_sword_turn": "MM_GreatSword_Turn_Right",
    "great_sword_turn__2_": "MM_GreatSword_Turn_Left",
    "great_sword_180_turn": "MM_GreatSword_Turn180_Right",
    "great_sword_180_turn__2_": "MM_GreatSword_Turn180_Left",
    "great_sword_jump": "MM_GreatSword_Jump_Start",
    "great_sword_jump__2_": "MM_GreatSword_Jump_Land",
}


def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Required asset was not found: " + path)
    return asset


def retarget_animation(source_mesh, target_mesh, retargeter, source_name, output_name):
    output_path = OUTPUT_FOLDER + "/" + output_name
    if unreal.EditorAssetLibrary.does_asset_exist(output_path):
        unreal.EditorAssetLibrary.delete_asset(output_path)

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [
        unreal.EditorAssetLibrary.find_asset_data(SOURCE_FOLDER + "/" + source_name)
    ]
    inputs.source_mesh = source_mesh
    inputs.target_mesh = target_mesh
    inputs.ik_retarget_asset = retargeter
    inputs.search = source_name
    inputs.replace = output_name
    inputs.target_path = OUTPUT_FOLDER
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True

    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not created:
        raise RuntimeError("Retargeting did not produce: " + output_name)

    output = require_asset(output_path)
    if output.get_editor_property("skeleton") != target_mesh.get_editor_property("skeleton"):
        raise RuntimeError(output_name + " was not bound to the Manny skeleton")
    unreal.EditorAssetLibrary.save_loaded_asset(output, False)
    unreal.log("[GreatSwordRetarget] {0}: {1}s".format(output_name, output.get_play_length()))


def main():
    source_mesh = require_asset(SOURCE_MESH_PATH)
    target_mesh = require_asset(TARGET_MESH_PATH)
    retargeter = require_asset(RETARGETER_PATH)
    unreal.EditorAssetLibrary.make_directory(OUTPUT_FOLDER)

    for source_name, output_name in ANIMATIONS.items():
        require_asset(SOURCE_FOLDER + "/" + source_name)
        retarget_animation(source_mesh, target_mesh, retargeter, source_name, output_name)

    unreal.EditorAssetLibrary.save_loaded_asset(retargeter, False)
    unreal.log("[GreatSwordRetarget] Complete")


if __name__ == "__main__":
    main()
