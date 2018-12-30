import bpy
import mathutils
from dataclasses import (dataclass as DATACLASSES_dataclass)
from functools import reduce as FUNCTOOLS_reduce
from functools import wraps as FUNCTOOLS_wraps
from pprint import pprint as pp

# mesh.vertices.weights
# http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-9-vbo-indexing/

enum_POSE_REST_t = str

class Outp:
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
        else:
            raise RuntimeError()
        return self
    def p(self, x):
        assert(type(x) == str)
        p(f'{self.ident()}' + x)
    def ident(self):
        return ' ' * (self.m_ident * 4)
    m_ident = 0
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

class WithSect:
    def __init__(self, name: str):
        self.m_name = name
    def __enter__(self):
        g_o + self.m_name
        return self
    def __exit__(self, exc_type, exc_value, traceback):
        pass

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

def p(*args):
    if len(args):
        for x in range(len(args) - 1):
            print(str(args[x]) + ' | ', end='')
        print(str(args[len(args) - 1]))

def _modl(meob: MeOb):
    # https://docs.blender.org/api/blender2.8/bpy.types.Mesh.html
    #   Mesh.loops, Mesh.uv_layers Mesh.vertex_colors are all aligned
    #   so the same polygon loop indices can be used to find
    #   the UV’s and vertex colors as with as the vertices.
    with WithSect('modl'), WithIdent():
        g_o - 'name'
        with WithIdent():
            g_o - meob.m_mesh.name
        g_o - 'vert'
        with WithIdent():
            for l in _genloopidx(meob.m_mesh.polygons):
                g_o - meob.m_mesh.vertices[meob.m_mesh.loops[l].vertex_index].co
        g_o - 'indx'
        with WithIdent():
            for v in range(len(list(_genloopidx(meob.m_mesh.polygons)))):
                g_o - v
        g_o - 'uv'
        with WithIdent():
            for layr in meob.m_mesh.uv_layers:
                g_o - 'layr'
                with WithIdent():
                    g_o - 'name'
                    with WithIdent():
                        g_o - layr.name
                    g_o - 'vect'
                    with WithIdent():
                        for l in _genloopidx(meob.m_mesh.polygons):
                            g_o - layr.data[l].uv

def _armt(meob: MeOb):
    with WithSect('armt'), WithIdent():
        g_o - 'matx'
        with WithIdent():
            g_o - meob.m_armo.matrix_world

def _bone(meob: MeOb):
    with WithSect('bone'), WithIdent():
        for b in meob.m_armt.bones:
            g_o - 'bone'
            with WithIdent():
                g_o - 'name'
                with WithIdent():
                    g_o - b.name
                g_o - 'matx'
                with WithIdent():
                    for b in meob.m_armt.bones:
                        g_o - b.matrix_local

def _run():
    meob: MeOb = MeOb.meob(bpy.data.meshes[0])
    _modl(meob)
    with WithPose(meob.m_armt, 'REST'):
        _armt(meob)
        _bone(meob)

if __name__ == '__main__':
    _run()
