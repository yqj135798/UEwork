import unreal
import sys

def log(s):
    print("PROBE:", s, flush=True)

mesh = unreal.load_asset("/Game/Fab/Ball/ball/StaticMeshes/Sphere001_Material__26_0")
log("mesh=%r" % mesh)
if mesh:
    b = mesh.get_bounds()
    log("bounds origin=%r box_extent=%r sphere_radius=%r" % (b[0], b[1], b[2]))
    mats = mesh.get_editor_property("static_materials")
    for i, m in enumerate(mats):
        log("material slot %d = %r" % (i, m.get_editor_property("material_interface")))

# also sword
sword = unreal.load_asset("/Game/Sword/Sword/SM_Sword")
if sword:
    bs = sword.get_bounds()
    log("sword bounds origin=%r box_extent=%r sphere_radius=%r" % (bs[0], bs[1], bs[2]))
