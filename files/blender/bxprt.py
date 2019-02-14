import bpy
import mathutils
import os.path
import typing
from dataclasses import dataclass as DATACLASSES_dataclass
from functools import reduce as FUNCTOOLS_reduce
from functools import wraps as FUNCTOOLS_wraps
from math import ceil as MATH_ceil
from math import floor as MATH_floor
from mathutils import Matrix as MATHUTILS_Matrix
from pprint import pprint as pp
from re import match as RE_match

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
    def meob(klass, meso: bpy.types.Object, armo: bpy.types.Object):
        assert meso.type == 'MESH' and armo.type == 'ARMATURE'
        mesh: bpy.types.Mesh = meso.data
        armt: bpy.types.Armature = armo.data
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

# column-major
def m2(x: mathutils.Matrix):
    return [x.col[c][r] for c in range(len(x.col)) for r in range(len(x.row))]

def bone_matrix_local_to_relative(b: bpy.types.Bone):
    m = b.parent.matrix_local if b.parent else MATHUTILS_Matrix()
    return m.inverted() @ b.matrix_local

def _modl(d: dict, meob: MeOb):
    d['modl'][meob.m_mesh.name] = {'vert':[], 'indx':[], 'uvla':{}, 'weit':{'bna':[], 'bwt':[]}}
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
        d['modl'][meob.m_mesh.name]['weit']['bna'].append([v.name for v in lim_affect])
        d['modl'][meob.m_mesh.name]['weit']['bwt'].append([v.weight(vertidx) for v in lim_affect])

def _armt(d: dict, meob: MeOb):
    d['armt'][meob.m_armo.name] = {'matx':[], 'bone':{}, 'tree':{}}
    d['armt'][meob.m_armo.name]['matx'] = m2(meob.m_armo.matrix_world)
    for b in meob.m_armt.bones:
        d['armt'][meob.m_armo.name]['bone'][b.name] = m2(bone_matrix_local_to_relative(b))
    def _rec(b_: bpy.types.Bone):
        return {b.name : _rec(b) for b in b_.children}
    rootbones: bpy.types.Bone = [b for b in meob.m_armt.bones if not b.parent]
    assert len(rootbones) == 1
    d['armt'][meob.m_armo.name]['tree'][rootbones[0].name] = _rec(rootbones[0])

def _actn(d: dict, meob: MeOb):
    for actn in bpy.data.actions:
        d['actn'][actn.name] = {'fcrv': {}}
        fcrv = {}
        for f in actn.fcurves:
            m = RE_match('pose.bones\["(\w+)"\].(\w+)', f.data_path)
            f.update() # API docs: Ensure keyframes are sorted in chronological order and handles are set correctly
            rnge = range(MATH_floor(f.range()[0]), MATH_ceil(f.range()[1]))
            fcrv.setdefault(m[1], {}).setdefault(m[2], {})[f.array_index] = [f.evaluate(v) for v in rnge]
        d['actn'][actn.name]['fcrv'] = fcrv

def _run():
    d = {'modl':{}, 'armt':{}, 'actn':{}}
    meso: typing.List[bpy.types.Object] = [o for o in bpy.data.objects if o.type == 'MESH']
    armo: typing.List[bpy.types.Object] = [o for o in bpy.data.objects if o.type == 'ARMATURE']
    assert len(meso) == 1 and len(armo) == 1
    meob: MeOb = MeOb.meob(meso[0], armo[0])
    _modl(d, meob)
    with WithPose(meob.m_armt, 'REST'):
        _armt(d, meob)
    _actn(d, meob)
    d_write(d)

if __name__ == '__main__':
    _run()
