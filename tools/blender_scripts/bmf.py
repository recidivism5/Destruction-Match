#Spec:
#list of vertices with:
#   position, normal and uv
#list of materials with:
#   texture (inline png), roughness (0-1), glass boolean
#list of vertex counts per material
#list of AABB colliders:
#   min (vec3), max (vec3)

bl_info = {
    "name": "Bryant Mesh Format Exporter",
    "description": "Exports a mesh to bmf format. Adapted from Mark Kughler's (GoliathForgeOnline) GEO format export script: https://www.gamedev.net/blogs/entry/2266917-blender-mesh-export-script/",
    "author": "Ian Bryant",
    "version": (2, 0),
    "blender": (2, 79, 0),
    "location": "File > Export > bmf",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "support": 'COMMUNITY',
    "category": "Import-Export"
}

import os
import bpy
import bmesh
import struct
from bpy import context

def getChildren(obj):
    children = []
    for ob in bpy.data.objects:
        if ob.parent == obj:
            children.append(ob)
    return children

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

def getMeshTriangulated(obj):
    me = obj.data
    # Get a BMesh representation
    bm = bmesh.new()
    bm.from_mesh(me)

    #bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method=0, ngon_method=0)

    mesh = bpy.data.meshes.new("temp")
    bm.to_mesh(me)
    bm.free()
    return obj
    
def writeObject(path, obj):
    uv_layer = obj.data.uv_layers.active.data
    with open(path, "wb") as f:

        polygonGroups = []
        for slot in obj.material_slots:
            polygonGroups.append([])
        for p in obj.data.polygons:
            polygonGroups[p.material_index].append(p)

        f.write(struct.pack("<i", 3*len(obj.data.polygons))) #vertexCount
        for group in polygonGroups:
            for p in group:
                for i in range(3):
                    pi = p.vertices[i]
                    uvi = p.loop_indices[i]
                    v = obj.data.vertices[pi].co
                    n = obj.data.vertices[pi].normal
                    uv = uv_layer[uvi].uv
                    f.write(struct.pack("<3f", v.x, v.y, v.z)) #position
                    f.write(struct.pack("<3f", n.x, n.y, n.z)) #normal
                    f.write(struct.pack("<2f", uv.x, uv.y)) #uv

        f.write(struct.pack("<i", len(obj.material_slots))) #materialCount
        for i, slot in obj.material_slots:
            nodes = slot.material.node_tree.nodes

            f.write(struct.pack("<i", 3*len(polygonGroups[i]))) #vertexCount
            
            bsdf = 0
            roughness = 0.0
            for n in nodes:
                if n.type == "BSDF_GLOSSY":
                    bsdf = n
                    f.write(struct.pack("<i",0)) #glass = false
                    roughness = nodes[2].inputs["Roughness"].default_value
                    break
                elif n.type == "BSDF_GLASS":
                    bsdf = n
                    f.write(struct.pack("<i",1)) #glass = true
                    roughness = nodes[1].inputs["Roughness"].default_value
                    break
            f.write(struct.pack("<f",roughness)) #roughness

            texName = bsdf.inputs["Color"].links[0].from_node.image.name
            tex = bpy.data.images[texName]
            texPath = os.path.normpath(bpy.path.abspath(tex.filepath, library=tex.library))
            with open(texPath,"rb") as texFile:
                texBytes = texFile.read()
                f.write(struct.pack("<i",len(texBytes))) #texture len
                f.write(texBytes) #texture
        
        #cl = getChildren(obj)
        #f.write(struct.pack("<i", len(cl))) #boxColliderCount
        #for c in cl:
        #    s = c.dimensions / 2.0
        #    f.write(struct.pack("<3f", s.x, s.y, s.z)) #scale
        #    q = c.rotation_euler.to_quaternion()
        #    f.write(struct.pack("<4f", q.w, q.x, q.y, q.z)) #orientation quaternion
        #    loc = c.location
        #    f.write(struct.pack("<3f", loc.x, loc.y, loc.z)) #position

    return {'FINISHED'}



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
        
        triangulate_object(context.active_object)
        writeObject(self.filepath, context.active_object)

        return {'FINISHED'}

    def invoke(self, context, event):
        self.filepath = context.active_object.name + ".bmf"
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