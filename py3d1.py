import bpy
import struct

file_path = "/home/jiji/Documents/testBSDP3D/enemyAttack.bin"
obj = bpy.context.active_object
start_frame = 1
end_frame = 59

with open(file_path, "wb") as f:
    mesh_eval = obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    # ВАЖНО: Тут по-прежнему количество вершин (polygons * 3), а не 6! 
    # 6 — это сколько float-ов на одну вершину.
    total_draw_verts = len(mesh_eval.polygons) * 3 
    f.write(struct.pack("II", total_draw_verts, end_frame - start_frame + 1))

    for frame in range(start_frame, end_frame + 1):
        bpy.context.scene.frame_set(frame)
        depsgraph = bpy.context.evaluated_depsgraph_get()
        obj_eval = obj.evaluated_get(depsgraph)
        mesh = obj_eval.to_mesh()

        for poly in mesh.polygons:
            for vert_idx in poly.vertices:
                v = mesh.vertices[vert_idx]
                
                # МЕНЯЕМ ОСИ ТУТ (Blender Z -> OpenGL Y)
                # Позиция: X остается X, Z становится Y, Y становится -Z
                posX, posY, posZ = v.co.x, v.co.z, -v.co.y
                # Нормали меняем точно так же
                normX, normY, normZ = v.normal.x, v.normal.z, -v.normal.y

                f.write(struct.pack("ffffff", 
                    posX, posY, posZ, 
                    normX, normY, normZ))
            
        obj_eval.to_mesh_clear()

print("Okey - Axes Swapped")
