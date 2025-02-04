# DEV4 Enhanced Level Exporter v1.4
# an upgraded export script based on the one from 3DCC
# this one automatically exports .obj/.mtl files and converts to .h2b
# it also includes object aligned bounding box data 

import bpy
import os
import subprocess
from bpy_extras.io_utils import axis_conversion
import mathutils
import math

print("----------Begin Level Export----------")

path = os.path.dirname(bpy.data.filepath)
file = open(os.path.join(path, "GameLevel.txt"),"w")
file.write("# Game Level Exporter v1.3\n")

scene = bpy.context.scene

def print_heir(ob, levels=10):
    def recurse(ob, parent, depth):
        if depth > levels: 
            return
        # spacing to show hierarchy
        spaces = "  " * depth;
        # print to system console for debugging
        print(spaces, ob.type)
        print(spaces, ob.name)
        print(spaces, ob.matrix_world)
        # send to file
        file.write(spaces + ob.type + "\n")
        file.write(spaces + ob.name + "\n")
        
        # swap from blender space to vulkan/d3d 
        # { rx, ry, rz, 0 } to { rx, rz, ry, 0 }  
        # { ux, uy, uz, 0 }    { ux, uz, uy, 0 }
        # { lx, ly, lz, 0 }    { lx, lz, ly, 0 } 
        # { px, py, pz, 1 }    { px, pz, py, 1 }  
        row_world = ob.matrix_world.transposed()
        converted = mathutils.Matrix.Identity(4)
        converted[0][0:3] = row_world[0][0], row_world[0][2], row_world[0][1]
        converted[1][0:3] = row_world[1][0], row_world[1][2], row_world[1][1] 
        converted[2][0:3] = row_world[2][0], row_world[2][2], row_world[2][1] 
        converted[3][0:3] = row_world[3][0], row_world[3][2], row_world[3][1]  
        
        # flip the local Z axis for winding and transpose for export
        scaleZ = mathutils.Matrix.Scale(-1.0, 4, (0.0, 0.0, 1.0))
        converted = scaleZ.transposed() @ converted  
        file.write(spaces + str(converted) + "\n")
        
        pointLightPos = mathutils.Vector((converted[3][0], converted[3][1], converted[3][2]))
        spotLightPos = mathutils.Vector((converted[3][0], converted[3][1], converted[3][2]))
        spotLightDir = mathutils.Vector((converted[2][0], converted[2][1], converted[2][2]))
        
        if ob.type == "LIGHT":
            light = ob.data
            lightColor = mathutils.Vector((light.color[0], light.color[1], light.color[2]))
            if light.type == "POINT" :
                file.write("<Position (" + "{:.4f}".format(pointLightPos[0]) + ", " + "{:.4f}".format(pointLightPos[1]) + ", " + "{:.4f}".format(pointLightPos[2]) + ", " + "1.0000" + ")>\n")
                file.write("<Color (" + "{:.4f}".format(lightColor[0]) + ", " + "{:.4f}".format(lightColor[1]) + ", " + "{:.4f}".format(lightColor[2]) + ")>\n")
                file.write("<Radius (" + str(light.shadow_soft_size) + ")>\n")
            elif light.type == "SPOT" :
                file.write("<Position (" + "{:.4f}".format(spotLightPos[0]) + ", " + "{:.4f}".format(spotLightPos[1]) + ", " + "{:.4f}".format(spotLightPos[2]) + ", " + "2.0000" + ")>\n")
                file.write("<Color (" + "{:.4f}".format(lightColor[0]) + ", " + "{:.4f}".format(lightColor[1]) + ", " + "{:.4f}".format(lightColor[2]) + ")>\n")
                file.write("<Radius (" + str(light.shadow_soft_size) + ")>\n")
                file.write("<Direction (" + "{:.4f}".format(spotLightDir[0]) + ", " + "{:.4f}".format(spotLightDir[1]) + ", " + "{:.4f}".format(spotLightDir[2]) + ")>\n")
                file.write("<Inner Cone (" + "{:.4f}".format(math.cos((light.spot_size - light.spot_blend) * 0.5)) + ")>\n") 
                file.write("<Outer Cone (" + "{:.4f}".format(math.cos(light.spot_size * 0.5)) + ")>\n")
        
        # adding export of object aligned bounding data 
        # only do this for a MESH
        if ob.type == "MESH":
            bbox_corners = [mathutils.Vector(corner) for corner in ob.bound_box]
            for corner in bbox_corners:
        #        # convert corners to vulkan/d3d coordinates
                corner[1], corner[2] = corner[2], corner[1]
                print(spaces, corner) # debug only
                file.write(spaces + str(corner) + "\n") # write to file
         
        # TODO: For a game ready exporter we would
        # probably want the delta(pivot) matrix, lights,
        # detailed mesh hierarchy information
        
        for child in ob.children:
            recurse(child, ob,  depth + 1)
    recurse(ob, ob.parent, 0)

root_obs = (o for o in scene.objects if not o.parent)

for o in root_obs:
    print_heir(o)
    
file.close()

print("----------End Level Export----------")


# *NEW* save any unique MESH data as .obj and .mtl files 
print("----------Begin Model Export----------")

# Set the directory to save the .obj files
export_dir = path + "\\Models"

# Create the export directory if it does not exist
if not os.path.exists(export_dir):
    os.makedirs(export_dir)

# Create a set to store unique object names
unique_names = set()

# Iterate over all scene objects
for obj in bpy.context.scene.objects:
    # Check if the object has mesh data
                
    if obj.type == 'MESH':
        # remove duplicate naming from the model
        base_name, extension = os.path.splitext(obj.name)
        # Check if the object is a unique instance
        if base_name not in unique_names:
            # Add the object name to the set of unique names
            unique_names.add(base_name)
            # IMPORTANT: This script filters duplicates therefore
            # modified duplicate geometry/materials will be ignored
            
            # Select the object and set it as the active object
            bpy.context.view_layer.objects.active = obj
            obj.select_set(True)

            # save the object's marix
            save = obj.matrix_world.copy()

            # Set the object's matrix to the identity matrix
            obj.matrix_world.identity()

            # Export the object as an .obj file
            obj_file = os.path.join(export_dir, f"{base_name}.obj")
            #bpy.ops.export_scene.obj(
            #    filepath=obj_file,
            #    use_selection=True,
            #    use_materials=True,
            #    use_normals=True,
            #    use_triangles=True,
            #    use_mesh_modifiers=True,
            #    path_mode='STRIP',
                # need to look into why these had to be set this way
            #    axis_forward='Y',
            #    axis_up='-Z'
            #)
            
            # credit to this link for finding the correct function to export as obj
            # with Blender 4.0.  
            # https://blender.stackexchange.com/questions/84934/what-is-the-python-script-to-export-the-selected-meshes-in-obj/309888#309888
            bpy.ops.wm.obj_export(
                filepath=obj_file,
                check_existing=True,
                filter_blender=False,
                filter_backup=False,
                filter_image=False,
                filter_movie=False,
                filter_python=False,
                filter_font=False,
                filter_sound=False,
                filter_text=False,
                filter_archive=False,
                filter_btx=False,
                filter_collada=False,
                filter_alembic=False,
                filter_usd=False,
                filter_obj=False,
                filter_volume=False,
                filter_folder=True,
                filter_blenlib=False,
                filemode=8,
                display_type='DEFAULT',
                sort_method='DEFAULT',
                export_animation=False,
                start_frame=-2147483648,
                end_frame=2147483647,
                # need to look into why these had to be set this way
                forward_axis='Y',
                up_axis='Z',
                global_scale=1.0,
                apply_modifiers=True,
                export_eval_mode='DAG_EVAL_VIEWPORT',
                export_selected_objects=True,
                export_uv=True,
                export_normals=True,
                export_colors=False,
                export_materials=True,
                export_pbr_extensions=True,
                path_mode='STRIP',
                export_triangulated_mesh=True,
                export_curves_as_nurbs=False,
                export_object_groups=False,
                export_material_groups=False,
                export_vertex_groups=False,
                export_smooth_groups=False,                
                smooth_group_bitflags=False,
                filter_glob='*.obj;*.mtl'
            )

            # Restore the object's original matrix
            obj.matrix_world = save
            
            # Deselect the object
            obj.select_set(False)

print("----------End Model Export----------")

# *NEW* Convert .obj and .mtl files to .h2b files if tool is available 
print("----------Begin Model Conversion----------")

# Look for my_executable.exe in the export directory
exe_path = os.path.join(export_dir, "Obj2Header.exe")
if os.path.isfile(exe_path):
    # Run the executable if it is present
    os.chdir(export_dir)
    process = subprocess.Popen([exe_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, errors = process.communicate()
    # delete any generated header files
    #for header in unique_names:
        #os.remove(os.path.join(export_dir, f"{header}.h"))

print("----------End Model Conversion----------")

# check the blender python API docs under "Object(ID)"
# that is where I found the "type" and "matrix_world"
# there is so much more useful stuff for a game level!