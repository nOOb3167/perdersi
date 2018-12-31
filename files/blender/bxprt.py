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

def ps(*args, **kwargs):
    ret = ''
    sep = kwargs['sep'] if 'sep' in kwargs else ' | '
    if len(args):
        for x in range(len(args) - 1):
            ret += str(args[x]) + sep
        ret += str(args[-1])
    return ret

class Outp:
    def __init__(self):
        self.m_outlines = []
    def __add__(self, other):
        assert(type(other) == str)
        p(f'section {other}')
        return self
    def __sub__(self, x):
        def f(x):
            return format(x, ' 8.4f')
        tx = type(x)
        if tx == str:
            self.p(x)
        elif tx == int:
            self.p(str(x))
        elif tx == mathutils.Vector:
            if len(x) == 2:
                self.p(f'{f(x[0])} {f(x[1])}')
            elif len(x) == 3:
                self.p(f'{f(x[0])} {f(x[1])} {f(x[2])}')
            else:
                raise RuntimeError()
        elif tx == mathutils.Matrix:
            self.p(f'{f(x.row[0][0])} {f(x.row[0][1])} {f(x.row[0][2])} {f(x.row[0][3])}')
            self.p(f'{f(x.row[1][0])} {f(x.row[1][1])} {f(x.row[1][2])} {f(x.row[1][3])}')
            self.p(f'{f(x.row[2][0])} {f(x.row[2][1])} {f(x.row[2][2])} {f(x.row[2][3])}')
            self.p(f'{f(x.row[3][0])} {f(x.row[3][1])} {f(x.row[3][2])} {f(x.row[3][3])}')
        elif tx == tuple:
            self.p(ps(*x, sep=' '))
        else:
            raise RuntimeError()
        return self
    def p(self, x):
        assert(type(x) == str)
        line = f'{self.ident()}' + x
        self.m_outlines.append(line)
        print(line)
    def ident(self):
        return ' ' * (self.m_ident * 4)
    def write(self):
        export_path = os.path.splitext(bpy.data.filepath)[0] + ".psmdl"
        with open(file=export_path, mode='w', newline='\n') as f:
            for v in self.m_outlines:
                f.write(f'{v}\n')
    m_ident = 0
    m_outlines = None
g_o = Outp()

class WithIdent:
    def __init__(self, ident: int = None):
        self.m_ident = ident
        self.m_old = None
    def __enter__(self):
        self.m_old: int = g_o.m_ident
        g_o.m_ident = self.m_ident or g_o.m_ident + 1
        return self
    def __exit__(self, exc_type, exc_value, traceback):
        g_o.m_ident = self.m_old

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

def _modl(meob: MeOb):
    # https://docs.blender.org/api/blender2.8/bpy.types.Mesh.html
    #   Mesh.loops, Mesh.uv_layers Mesh.vertex_colors are all aligned
    #   so the same polygon loop indices can be used to find
    #   the UV’s and vertex colors as with as the vertices.
    g_o - ('modl', meob.m_mesh.name)
    with WithIdent():
        g_o - 'vert'
        with WithIdent():
            for l in _genloopidx(meob.m_mesh.polygons):
                g_o - meob.m_mesh.vertices[meob.m_mesh.loops[l].vertex_index].co
        g_o - 'indx'
        with WithIdent():
            g_o - tuple([x for x in range(len(list(_genloopidx(meob.m_mesh.polygons))))])
        g_o - 'uvla'
        with WithIdent():
            for layr in meob.m_mesh.uv_layers:
                g_o - ('layr', layr.name)
                with WithIdent():
                    for l in _genloopidx(meob.m_mesh.polygons):
                        g_o - layr.data[l].uv
        g_o - 'weit'
        with WithIdent():
            map_idx_grp = {v.index : v for v in meob.m_meso.vertex_groups}
            for l in _genloopidx(meob.m_mesh.polygons):
                vertidx: int = meob.m_mesh.loops[l].vertex_index
                vert: bpy.types.MeshVertex = meob.m_mesh.vertices[vertidx]
                yes_affect: typing.List[bpy.types.VertexGroup] = [map_idx_grp[x.group] for x in vert.groups if x.group in map_idx_grp]
                not_affect: typing.List[bpy.types.VertexGroup] = [map_idx_grp[x.group] for x in vert.groups if x.group not in map_idx_grp]
                sor_affect: typing.List[bpy.types.VertexGroup] = sorted(yes_affect, key=lambda x: x.weight(vertidx), reverse=True)
                lim_affect: typing.List[bpy.types.VertexGroup] = sor_affect[0:4]
                line = ''
                for v in lim_affect:
                    line += f'{v.name} {v.weight(vertidx)} '
                for v in range(4 - len(lim_affect)):
                    line += f'NONE 0.0 '
                g_o - line.strip()

def _armt(meob: MeOb):
    g_o - ('armt', meob.m_armo.name)
    with WithIdent():
        g_o - 'matx'
        with WithIdent():
            g_o - meob.m_armo.matrix_world
        g_o - 'bone'
        with WithIdent():
            for b in meob.m_armt.bones:
                g_o - ('bone', b.name)
                with WithIdent():
                    for b in meob.m_armt.bones:
                        g_o - b.matrix_local

def _run():
    meob: MeOb = MeOb.meob(bpy.data.meshes[0])
    _modl(meob)
    with WithPose(meob.m_armt, 'REST'):
        _armt(meob)
    g_o.write()

if __name__ == '__main__':
    _run()
