import bpy
import mathutils
import os.path
import typing
from dataclasses import dataclass as DATACLASSES_dataclass
from functools import reduce as FUNCTOOLS_reduce
from functools import wraps as FUNCTOOLS_wraps
from pprint import pprint as pp

# mesh.vertices.weights
# http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-9-vbo-indexing/
# https://docs.blender.org/api/blender2.8/bpy.types.Object.html#bpy.types.Object.find_armature
# https://blender.stackexchange.com/questions/74461/exporting-weight-and-bone-elements-to-a-text-file
# https://docs.blender.org/manual/en/latest/rigging/armatures/skinning/parenting.html

enum_POSE_REST_t = str

class WithPose:
    def __init__(self, armt: bpy.types.Armature, typ: enum_POSE_REST_t):
        self.m_armt = armt
        self.m_typ = typ
        self.m_old = None
    def __enter__(self):
        self.m_old: str = self.m_armt.pose_position
        self.m_armt.pose_position = self.m_typ
        return self
    def __exit__(self, exc_type, exc_value, traceback):
        self.m_armt.pose_position = self.m_old

@DATACLASSES_dataclass
class MeOb:
    m_mesh: bpy.types.Mesh
    m_meso: bpy.types.Object
    m_armt: bpy.types.Armature
    m_armo: bpy.types.Object
    
    @classmethod
    def meob(klass, mesh: bpy.types.Mesh):
        meso: bpy.types.Object = bpy.data.objects[mesh.name]
        armt: bpy.types.Armature = bpy.data.armatures[0]
        armo: bpy.types.Object = bpy.data.objects[armt.name]
        meob: MeOb = MeOb(m_mesh=mesh, m_meso=meso, m_armt=armt, m_armo=armo)
        meob.m_mesh.calc_loop_triangles() # FIXME: loop_triangles not used anymore
        return meob

def _genloopidx(polygons: bpy.types.MeshPolygon):
    # https://docs.blender.org/api/blender2.8/bpy.types.Mesh.html
    #   Mesh.loops, Mesh.uv_layers Mesh.vertex_colors are all aligned
    #   so the same polygon loop indices can be used to find
    #   the UV’s and vertex colors as with as the vertices.
    for poly in polygons:
        if poly.loop_total == 3:
            yield poly.loop_start + 0
            yield poly.loop_start + 1
            yield poly.loop_start + 2
        elif poly.loop_total == 4:
            # FIXME: CW CCW NAIVE TRIANGULATION ETC
            #   although results seem consistent with loop_triangles
            #   for v in meob.m_mesh.loop_triangles:
            #       g_o - mathutils.Vector((v.vertices[0], v.vertices[1], v.vertices[2]))
            yield poly.loop_start + 0
            yield poly.loop_start + 1
            yield poly.loop_start + 2
            yield poly.loop_start + 0
            yield poly.loop_start + 2
            yield poly.loop_start + 3
        else:
            raise RuntimeError()

def d_write(d: dict):
    import json
    export_path = os.path.splitext(bpy.data.filepath)[0] + ".psmdl"
    with open(file=export_path, mode='w', newline='\n') as f:
        json.dump(d, f, indent=4)

def v2(x: mathutils.Vector):
    return [x[c] for c in range(len(x))]

def m2(x: mathutils.Matrix):
    return [x.row[r][c] for r in range(4) for c in range(4)]

def _modl(d: dict, meob: MeOb):
    d['modl'][meob.m_mesh.name] = {'vert':[], 'indx':[], 'uvla':{}, 'weit':[]}
    d['modl'][meob.m_mesh.name]['vert'] = [v2(meob.m_mesh.vertices[meob.m_mesh.loops[x].vertex_index].co) for x in _genloopidx(meob.m_mesh.polygons)]
    d['modl'][meob.m_mesh.name]['indx'] = [x for x in range(len(list(_genloopidx(meob.m_mesh.polygons))))]
    for layr in meob.m_mesh.uv_layers:
        d['modl'][meob.m_mesh.name]['uvla'][layr.name] = [v2(layr.data[x].uv) for x in _genloopidx(meob.m_mesh.polygons)]
    map_idx_grp = {v.index : v for v in meob.m_meso.vertex_groups}
    for l in _genloopidx(meob.m_mesh.polygons):
        vertidx: int = meob.m_mesh.loops[l].vertex_index
        vert: bpy.types.MeshVertex = meob.m_mesh.vertices[vertidx]
        yes_affect: typing.List[bpy.types.VertexGroup] = [map_idx_grp[x.group] for x in vert.groups if x.group in map_idx_grp]
        not_affect: typing.List[bpy.types.VertexGroup] = [map_idx_grp[x.group] for x in vert.groups if x.group not in map_idx_grp]
        sor_affect: typing.List[bpy.types.VertexGroup] = sorted(yes_affect, key=lambda x: x.weight(vertidx), reverse=True)
        lim_affect: typing.List[bpy.types.VertexGroup] = sor_affect[0:4]
        if len(lim_affect):
            for v in lim_affect:
                d['modl'][meob.m_mesh.name]['weit'].append([v.name, v.weight(vertidx)])
        else:
            d['modl'][meob.m_mesh.name]['weit'].append([])

def _armt(d: dict, meob: MeOb):
    d['armt'][meob.m_armo.name] = {'matx':[], 'bone':{}}
    d['armt'][meob.m_armo.name]['matx'] = m2(meob.m_armo.matrix_world)
    for b in meob.m_armt.bones:
        d['armt'][meob.m_armo.name]['bone'][b.name] = m2(b.matrix_local)

def _run():
    d = {'modl':{}, 'armt':{}}
    meob: MeOb = MeOb.meob(bpy.data.meshes[0])
    _modl(d, meob)
    with WithPose(meob.m_armt, 'REST'):
        _armt(d, meob)
    d_write(d)

if __name__ == '__main__':
    _run()
