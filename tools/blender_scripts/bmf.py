#Spec:
#we need a better spec


bl_info = {
    "name": "Bryant Mesh Format Exporter",
    "description": "Exports a mesh to bmf format. Adapted from Mark Kughler's (GoliathForgeOnline) GEO format export script: https://www.gamedev.net/blogs/entry/2266917-blender-mesh-export-script/",
    "author": "Ian Bryant",
    "version": (3, 0),
    "blender": (2, 79, 0),
    "location": "File > Export > bmf",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "support": 'COMMUNITY',
    "category": "Import-Export"
}

import math
import os
import bpy
import bmesh
import struct
from bpy import context

def getTextureName(material):
    nodes = material.node_tree.nodes
    glossy = next(n for n in nodes if n.type == "BSDF_GLOSSY")
    glass = next(n for n in nodes if n.type == "BSDF_GLASS")
    if glossy:
        return glossy.inputs["Color"].links[0].from_node.image.name
    elif glass:
        return glass.inputs["Color"].links[0].from_node.image.name

def triangulate_object(obj):
    me = obj.data
    # Get a BMesh representation
    bm = bmesh.new()
    bm.from_mesh(me)

    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    # V2.79 : bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method=0, ngon_method=0)

    # Finish up, write the bmesh back to the mesh
    bm.to_mesh(me)
    bm.free()

class ObjectExport(bpy.types.Operator):
    bl_idname = "object.export_bmf"
    bl_label = "Bryant Mesh Format Export"
    bl_options = {'REGISTER', 'UNDO'}
    filename_ext = ".bmf"
    
    total           = bpy.props.IntProperty(name="Steps", default=2, min=1, max=100)
    filter_glob     = bpy.props.StringProperty(default="*.bmf", options={'HIDDEN'}, maxlen=255)
    filepath        = bpy.props.StringProperty(subtype='FILE_PATH')
    
    def execute(self, context):
        if(context.active_object.mode == 'EDIT'):
            bpy.ops.object.mode_set(mode='OBJECT')

        with open(self.filepath, "wb") as f:

            f.write(struct.pack("<i",len(bpy.data.materials))) #material count
            for mat in bpy.data.materials:
                bsdf = 0
                for n in mat.node_tree.nodes:
                    if n.type == "BSDF_DIFFUSE":
                        bsdf = n
                        break
                    elif n.type == "BSDF_GLOSSY":
                        bsdf = n
                        break
                    elif n.type == "BSDF_GLASS":
                        bsdf = n
                        break
                texName = bsdf.inputs["Color"].links[0].from_node.image.name
                tex = bpy.data.images[texName]
                texPath = os.path.normpath(bpy.path.abspath(tex.filepath, library=tex.library))
                with open(texPath,"rb") as texFile:
                    texBytes = texFile.read()
                    f.write(struct.pack("<i",len(texBytes))) #texture len
                    f.write(texBytes) #texture

            f.write(struct.pack("<i",len(bpy.data.objects))) #object count
            for obj in bpy.data.objects:
                triangulate_object(obj)
                f.write(struct.pack("<i",len(obj.name))) #name length
                f.write(obj.name.encode("ascii")) #name
                f.write(struct.pack("<3f", obj.location.y, obj.location.z, obj.location.x)) #position
                f.write(struct.pack("<i",len(obj.data.vertices))) #vertex count
                for v in obj.data.vertices:
                    f.write(struct.pack("<6f",v.co.y,v.co.z,v.co.x,v.normal.y,v.normal.z,v.normal.x)) #vertices
                f.write(struct.pack("<i",len(obj.data.uv_layers.active.data))) #uv count, we need to eliminate duplicates from this
                for uvd in obj.data.uv_layers.active.data:
                    f.write(struct.pack("<2f",uvd.uv.x,uvd.uv.y)) #uvs
                polygroups = []
                for i in range(len(bpy.data.materials)):
                    polygroups.append([])
                for p in obj.data.polygons:
                    mat = obj.material_slots[p.material_index].material
                    for i in range(len(bpy.data.materials)):
                        if bpy.data.materials[i] == mat:
                            polygroups[i].append(p)
                            break
                groupcount = 0
                for g in polygroups:
                    if len(g):
                        groupcount += 1
                f.write(struct.pack("<i",groupcount)) #trigroup count
                for i in range(len(polygroups)):
                    g = polygroups[i]
                    if len(g):
                        f.write(struct.pack("<i",i)) #material index
                        f.write(struct.pack("<i",len(g)*3)) #index count
                        for p in g:
                            for i in range(3):
                                f.write(struct.pack("<2i",p.vertices[i],p.loop_indices[i])) #triangle indices

        return {'FINISHED'}

    def invoke(self, context, event):
        filename = bpy.path.basename(bpy.data.filepath)
        filename = os.path.splitext(filename)[0]
        self.filepath = filename + ".bmf"
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}



# Add trigger into a dynamic menu
def menu_func_export(self, context):
    self.layout.operator(ObjectExport.bl_idname, text="Bryant Mesh Format (.bmf)")
    

def register():
    bpy.utils.register_class(ObjectExport)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ObjectExport)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()