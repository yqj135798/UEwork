import unreal


ANIMATION_NAMES = [
    "draw_a_great_sword_1",
    "draw_a_great_sword_2",
    "great_sword_idle",
    "great_sword_idle__2_",
    "great_sword_idle__3_",
    "great_sword_idle__4_",
    "great_sword_idle__5_",
    "great_sword_walk",
    "great_sword_walk__2_",
    "great_sword_run",
    "great_sword_run__2_",
    "great_sword_strafe",
    "great_sword_strafe__2_",
    "great_sword_strafe__3_",
    "great_sword_strafe__4_",
    "great_sword_turn",
    "great_sword_turn__2_",
    "great_sword_180_turn",
    "great_sword_180_turn__2_",
    "great_sword_jump",
    "great_sword_jump__2_",
]
FOLDER = "/Game/Characters/Mannequins/Anims/sword/"


def prop(asset, name, fallback="n/a"):
    try:
        return asset.get_editor_property(name)
    except Exception:
        return fallback


def main():
    sword_mesh = unreal.load_asset("/Game/Sword/Sword/SM_Sword")
    if sword_mesh:
        bounds = sword_mesh.get_bounds()
        unreal.log(
            "[GreatSwordInspect] MeshBounds | Origin={0} | Extent={1} | Radius={2}".format(
                bounds.origin,
                bounds.box_extent,
                bounds.sphere_radius,
            )
        )

    for name in ANIMATION_NAMES:
        animation = unreal.load_asset(FOLDER + name)
        if not animation:
            unreal.log_error("[GreatSwordInspect] Missing: " + name)
            continue

        skeleton = prop(animation, "skeleton")
        skeleton_name = skeleton.get_name() if skeleton else "None"
        frames = prop(animation, "number_of_sampled_keys")
        rate = prop(animation, "sampling_frame_rate")
        root_motion = prop(animation, "enable_root_motion")
        looping = prop(animation, "looping_interpolation")
        unreal.log(
            "[GreatSwordInspect] {0} | Skeleton={1} | Duration={2:.3f}s | Frames={3} | Rate={4} | RootMotion={5} | Looping={6}".format(
                name,
                skeleton_name,
                animation.get_play_length(),
                frames,
                rate,
                root_motion,
                looping,
            )
        )


if __name__ == "__main__":
    main()
