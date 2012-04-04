# Released into the Public Domain

bl_info = {
    "name": "Export Binary Static Mesh (.bsm)",
    "description": "Export to Binary Static Mesh (.bsm)",
    "author": "zvxryb@gmail.com",
    "version": (2012, 3, 20),
    "blender": (2, 6, 2),
    "location": "File > Export > Binary Static Mesh",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Import-Export" }

import os, sys, struct, bpy, random, itertools
from bpy_extras.io_utils import ExportHelper
from mathutils import *

bsm_version = 1
bsm_extension = 0
bsm_magic = b"BINARYSTATICMESH"
bsm_header_v1 = struct.Struct("<16s2i10f17i")
bsm_position = struct.Struct("<3f")
bsm_texcoord = struct.Struct("<2f")
bsm_normal   = struct.Struct("<3f")
bsm_tangent  = struct.Struct("<4f")
bsm_triangle = struct.Struct("<3i")
bsm_mesh     = struct.Struct("<2i256s")
bsm_hullvert = struct.Struct("<3f")
bsm_hull     = struct.Struct("<2i")
bsm_visvert  = struct.Struct("<3f")
bsm_vistri   = struct.Struct("<3i")

class GeomVert:
    def __init__(self, mesh, position):
        self.mesh     = mesh
        self.position = position.copy()
        self.normal   = None
        self.texverts = []
        self.tris     = []

    def addtexcoord(self, texcoord):
        for texvert in self.texverts:
            if (texcoord - texvert.texcoord).length < 0.0001:
                return texvert
        texvert = TexVert(self, texcoord)
        self.texverts.append(texvert)
        self.mesh.texverts.append(texvert)
        return texvert

    def calcnormal(self):
        # this is calculated pre-tangent-split
        N = Vector((0.0, 0.0, 0.0))
        for tri in self.tris:
            N += tri.normal
        N.normalize()
        self.normal = N

    def reftris(self, tri):
        if not tri in self.tris:
            self.tris.append(tri)

class TexVert:
    def __init__(self, geomvert, texcoord):
        self.index      = None
        self.geomvert   = geomvert
        self.texcoord   = texcoord.copy()
        self.tangent    = None
        self.handedness = None
        self.tris       = []

    def split(self):
        # split along tangent-space discontinuities
        if len(self.tris) < 2:
            return
        
        reference = self.tris[0]
        T = reference.tangent
        B = reference.bitangent

        clone = None
        for tri in self.tris[1:]:
            if T.dot(tri.tangent) < 0.0 or B.dot(tri.bitangent) < 0.0:
                if not clone:
                    clone = TexVert(self.geomvert, self.texcoord)
                    self.geomvert.mesh.texverts.append(clone)
                    self.geomvert.texverts.append(clone)
                clone.tris.append(tri)
                self.tris.remove(tri)
                index = tri.verts.index(self)
                tri.verts[index] = clone

        if clone:
            clone.calctangent()

    def calctangent(self):
        self.split()
        
        # average adjacent triangles
        T = Vector((0.0, 0.0, 0.0))
        B = Vector((0.0, 0.0, 0.0))
        for tri in self.tris:
            T += tri.tangent
            B += tri.bitangent
        T.normalize()
        B.normalize()
        
        # orthogonalize
        N = self.geomvert.normal
        T = T - T.project(N)
        T.normalize()
        NxT = N.cross(T)

        self.tangent    = T
        self.handedness = 1.0 if NxT.dot(B) >= 0.0 else -1.0

    def reftris(self, tri):
        if not tri in self.tris:
            self.tris.append(tri)
        self.geomvert.reftris(tri)

class Triangle:
    def __init__(self, a, b, c, normal):
        # CCW winding:
        ab = b.geomvert.position - a.geomvert.position
        ac = c.geomvert.position - a.geomvert.position
        if (ab).cross(ac).dot(normal) < 0.0:
            self.verts = [c, b, a]
        else:
            self.verts = [a, b, c]
            
        a.reftris(self)
        b.reftris(self)
        c.reftris(self)
        
        self.normal    = normal.copy()
        self.tangent   = None
        self.bitangent = None

    def calctangent(self):
        co = [self.verts[i].geomvert.position for i in range(3)]
        uv = [self.verts[i].texcoord for i in range(3)]

        dP1 = co[1] - co[0]
        dP2 = co[2] - co[0]

        du1 = uv[1].x - uv[0].x
        dv1 = uv[1].y - uv[0].y
        du2 = uv[2].x - uv[0].x
        dv2 = uv[2].y - uv[0].y

        T = dP1 * dv2 - dP2 * dv1
        B = dP2 * du1 - dP1 * du2
        if du1 * dv2 - du2 * dv1 < 0.0:
            T.negate()
            B.negate()

        self.tangent   = T
        self.bitangent = B

class Mesh:
    def __init__(self, material):
        self.vertmap   = {}
        self.geomverts = []
        self.texverts  = []
        self.tris      = []
        self.trisoffset = None
        self.material  = material

    def addvert(self, smooth, blendvert, uv):
        index    = blendvert.index
        position = Vector(blendvert.co)
        texcoord = Vector(uv)
        if smooth and index in self.vertmap:
            geomvert = self.vertmap[index]
        else:
            geomvert = GeomVert(self, position)
            self.geomverts.append(geomvert)
            if smooth:
              self.vertmap[index] = geomvert
        texvert = geomvert.addtexcoord(texcoord)
        return texvert
        
    def addface(self, blendmesh, blendface, blenduv):
        faceverts = []
        for i, vertidx in enumerate(blendface.vertices):
            blendvert = blendmesh.vertices[vertidx]
            uv = blenduv.uv_raw[2*i:2*i+2]
            vert = self.addvert(blendface.use_smooth, blendvert, uv)
            faceverts.append(vert)
            if i > 1:
                tri = Triangle(faceverts[0], faceverts[i-1], faceverts[i], blendface.normal)
                self.tris.append(tri)

    def finalize(self, vertoffset, trisoffset):
        for tri in self.tris:
            tri.calctangent()
        for geomvert in self.geomverts:
            geomvert.calcnormal()
        for texvert in self.texverts[:]: # make a shallow copy because calctangent() modifies the texverts array
            texvert.calctangent()
        for index, texvert in enumerate(self.texverts):
            texvert.index = index + vertoffset
        self.trisoffset = trisoffset
        return len(self.texverts), len(self.tris)

    def vertices(self):
        verts = []
        for texvert in self.texverts:
            position   = texvert.geomvert.position
            texcoord   = texvert.texcoord
            normal     = texvert.geomvert.normal
            tangent    = texvert.tangent
            handedness = texvert.handedness
            vert = Vertex(position, texcoord, normal, tangent, handedness)
            verts.append(vert)
        return verts

class Plane:
    def __init__(self, n, d):
        self.n = n.copy()
        self.d = d

    @staticmethod
    def intersect(a, b, c):
        A = Matrix((
            (a.n.x, a.n.y, a.n.z),
            (b.n.x, b.n.y, b.n.z),
            (c.n.x, c.n.y, c.n.z)
        ))
        A_det  = A.determinant()
        if abs(A_det) < 0.0000001:
            return None
        Ax = Matrix((
            (a.d, a.n.y, a.n.z),
            (b.d, b.n.y, b.n.z),
            (c.d, c.n.y, c.n.z)
        ))
        Ay = Matrix((
            (a.n.x, a.d, a.n.z),
            (b.n.x, b.d, b.n.z),
            (c.n.x, c.d, c.n.z)
        ))
        Az = Matrix((
            (a.n.x, a.n.y, a.d),
            (b.n.x, b.n.y, b.d),
            (c.n.x, c.n.y, c.d)
        ))
        Ax_det = Ax.determinant()
        Ay_det = Ay.determinant()
        Az_det = Az.determinant()
        return Vector((Ax_det / A_det, Ay_det / A_det, Az_det / A_det))

    @staticmethod
    def frompoints(a, b, c):
        n = (b - a).cross(c - a)
        n.normalize()
        d = n.dot(a)
        return Plane(n, d)

EPSILON = 0.0001
class Sphere:
    def __init__(self, center, radius):
        self.center = center.copy()
        self.radius = radius

    def contains(self, points):
        for p in points:
            if self < p:
                return False
        return True

    @staticmethod
    def frompoints(points):
        if len(points) <= 4:
            return Sphere._direct(points)
        return Sphere._welzl(points)

    @staticmethod
    def _direct(points):
        assert len(points) <= 4
        if len(points) == 1:
            return Sphere(points[0], 0.0)
        if len(points) == 2:
            return Sphere._sphere2(points[0], points[1])
        if len(points) > 2:
            for a, b in itertools.combinations(points, 2):
                S = Sphere._sphere2(a, b)
                if S.contains(points):
                    return S
        if len(points) == 3:
            return Sphere._sphere3(points[0], points[1], points[2])
        if len(points) > 3:
            for a, b, c in itertools.combinations(points, 3):
                S = Sphere._sphere3(a, b, c)
                if S.contains(points):
                    return S
        if len(points) == 4:
            return Sphere._sphere4(points[0], points[1], points[2], points[3])
        if len(points) > 4:
            for a, b, c, d in itertools.combinations(points, 4):
                S = Sphere._sphere4(a, b, c, d)
                if S.contains(points):
                    return S
        return None
            
    @staticmethod
    def _midplane(a, b):
        n = a - b
        n.normalize()
        m = (a + b) / 2.0
        d = n.dot(m)
        return Plane(n, d)

    @staticmethod
    def _welzl(points):
        P = points[:]
        random.shuffle(P)
        R = []
        S = Sphere(Vector((0.0, 0.0, 0.0)), 0.0)
        i = 0
        def unique(R, p):
            for r in R:
                if (r - p).length < 0.0001:
                    return False
            return True
        while i < len(P):
            p = P[i]
            if S < p and unique(R, p):
                Q = R[:]
                for j in range(len(R)):
                    r = R[j]
                    Q_prime = Q[:]
                    Q_prime.remove(r)
                    S = Sphere._direct(Q_prime + [p])
                    if S.contains(R):
                        Q = Q_prime
                        break
                assert len(Q) <= 3
                R = Q[:3] + [p]
                S = Sphere._direct(R)
                i = 0
            else:
                i += 1
        return S

    @staticmethod
    def _sphere2(a, b):
        center = (a + b) / 2.0
        radius = (a - b).length / 2.0
        return Sphere(center, radius)

    @staticmethod
    def _sphere3(a, b, c):
        p1 = Sphere._midplane(a, b)
        p2 = Sphere._midplane(a, c)
        p3 = Plane.frompoints(a, b, c)
        center = Plane.intersect(p1, p2, p3)
        if center == None:
            return None
        radius = (center - a).length
        return Sphere(center, radius)

    @staticmethod
    def _sphere4(a, b, c, d):
        p1 = Sphere._midplane(a, b)
        p2 = Sphere._midplane(a, c)
        p3 = Sphere._midplane(a, d)
        center = Plane.intersect(p1, p2, p3)
        if center == None:
            return None
        radius = (center - a).length
        return Sphere(center, radius)

    def __lt__(self, other):
        if type(other) == Sphere:
            return self.radius + EPSILON < other.radius
        return self.radius + EPSILON < (self.center - other).length

    def __le__(self, other):
        if type(other) == Sphere:
            return self.radius - EPSILON < other.radius
        return self.radius - EPSILON < (self.center - other).length

    def __eq__(self, other):
        if type(other) == Sphere:
            return abs(self.radius - other.radius) < EPSILON
        return abs((self.center - other).length - self.radius) < EPSILON

    def __ne__(self, other):
        if type(other) == Sphere:
            return abs(self.radius - other.radius) > EPSILON
        return abs((self.center - other).length - self.radius) > EPSILON

    def __gt__(self, other):
        if type(other) == Sphere:
            return self.radius - EPSILON > other.radius
        return self.radius - EPSILON > (self.center - other).length

    def __gt__(self, other):
        if type(other) == Sphere:
            return self.radius + EPSILON > other.radius
        return self.radius + EPSILON > (self.center - other).length

class Box:
    def __init__(self, point_min, point_max):
        self.min = point_min.copy()
        self.max = point_max.copy()

    def update(self, point):
        if point.x < self.min.x:
            self.min.x = point.x
        if point.x > self.max.x:
            self.max.x = point.x
        if point.y < self.min.y:
            self.min.y = point.y
        if point.y > self.max.y:
            self.max.y = point.y
        if point.z < self.min.z:
            self.min.z = point.z
        if point.z > self.max.z:
            self.max.z = point.z

    @staticmethod
    def frompoints(points):
        B = Box(points[0], points[0])
        for p in points[1:]:
            B.update(p)
        return B
    
class Model:
    def __init__(self):
        self.meshes  = []
        self.meshmap = {}

    def addmesh(self, blendmesh):
        blenduvs = blendmesh.uv_textures.active and blendmesh.uv_textures.active.data or None
        for blendface in blendmesh.faces:
            blenduv  = blenduvs and blenduvs[blendface.index] or None 
            material = blenduv and blenduv.image and os.path.basename(blenduv.image.filepath) or ''
            if not blenduv:
                print('Cannot export mesh without UV coordinates!')
            if material in self.meshmap:
                mesh = self.meshmap[material]
                mesh.addface(blendmesh, blendface, blenduv)
            else:
                mesh = Mesh(material)
                mesh.addface(blendmesh, blendface, blenduv)
                self.meshmap[material] = mesh
                self.meshes.append(mesh)

    def finalize(self):
        vertoffs = 0
        trioffs  = 0
        for mesh in self.meshes:
            nverts, ntris = mesh.finalize(vertoffs, trioffs)
            vertoffs += nverts
            trioffs  += ntris

    def vertices(self):
        verts = []
        for mesh in self.meshes:
            verts += mesh.vertices()
        return verts

    def triangles(self):
        tris = []
        for mesh in self.meshes:
            tris += mesh.tris
        return tris

    def bsphere(self):
        points = [gvert.position for mesh in self.meshes for gvert in mesh.geomverts]
        return Sphere.frompoints(points)

    def bbox(self):
        points = [gvert.position for mesh in self.meshes for gvert in mesh.geomverts]
        return Box.frompoints(points)

    def tobsm(self):
        return BinaryStaticMesh(self.bsphere(), self.bbox(), self.vertices(), self.triangles(), self.meshes)
                
class Vertex:
    def __init__(self, position, texcoord, normal, tangent, handedness):
        self.position   = position
        self.texcoord   = texcoord
        self.normal     = normal
        self.tangent    = tangent
        self.handedness = handedness

class BinaryStaticMesh:
    def __init__(self, bsphere, bbox, verts, tris, meshes):
        self.bsphere   = bsphere
        self.bbox      = bbox
        self.verts     = verts
        self.tris      = tris
        self.meshes    = meshes
        self.hullverts = []
        self.hulls     = []
        self.visverts  = []
        self.vistris   = []

    def positionbuf(self):
        buf = b""
        for vert in self.verts:
            position = vert.position
            buf += bsm_position.pack(position.x, position.y, position.z)
        return buf

    def texcoordbuf(self):
        buf = b""
        for vert in self.verts:
            texcoord = vert.texcoord
            buf += bsm_texcoord.pack(texcoord.x, texcoord.y)
        return buf

    def normalbuf(self):
        buf = b""
        for vert in self.verts:
            normal = vert.normal
            buf += bsm_normal.pack(normal.x, normal.y, normal.z)
        return buf

    def tangentbuf(self):
        buf = b""
        for vert in self.verts:
            tangent = vert.tangent
            buf += bsm_tangent.pack(tangent.x, tangent.y, tangent.z, vert.handedness)
        return buf

    def trisbuf(self):
        buf = b""
        for tri in self.tris:
            index = [tri.verts[i].index for i in range(3)]
            buf += bsm_triangle.pack(index[0], index[1], index[2])
        return buf

    def meshesbuf(self):
        buf = b""
        for mesh in self.meshes:
            buf += bsm_mesh.pack(mesh.trisoffset, len(mesh.tris), mesh.material.encode('utf-8'))
        return buf

    def data(self):
        positionbuf = self.positionbuf()
        texcoordbuf = self.texcoordbuf()
        normalbuf   = self.normalbuf()
        tangentbuf  = self.tangentbuf()
        trisbuf     = self.trisbuf()
        meshesbuf   = self.meshesbuf()
        
        num_verts      = len(self.verts)
        offs_position  = bsm_header_v1.size
        offs_texcoord  = offs_position + len(positionbuf)
        offs_normal    = offs_texcoord + len(texcoordbuf)
        offs_tangent   = offs_normal   + len(normalbuf)
        num_tris       = len(self.tris)
        offs_tris      = offs_tangent + len(tangentbuf)
        num_meshes     = len(self.meshes)
        offs_meshes    = offs_tris + len(trisbuf)
        num_hullverts  = 0
        offs_hullverts = 0
        num_hulls      = 0
        offs_hulls     = 0
        num_visverts   = 0
        offs_visverts  = 0
        num_vistris    = 0
        offs_vistris   = 0
        
        header = bsm_header_v1.pack(
            bsm_magic, bsm_version, bsm_extension,
            self.bsphere.center.x, self.bsphere.center.y, self.bsphere.center.z, self.bsphere.radius,
            self.bbox.min.x, self.bbox.min.y, self.bbox.min.z, self.bbox.max.x, self.bbox.max.y, self.bbox.max.z,
            num_verts, offs_position, offs_texcoord, offs_normal, offs_tangent,
            num_tris, offs_tris, num_meshes, offs_meshes,
            num_hullverts, offs_hullverts, num_hulls, offs_hulls,
            num_visverts, offs_visverts, num_vistris, offs_vistris)

        return header + positionbuf + texcoordbuf + normalbuf + tangentbuf + trisbuf + meshesbuf

    def write(self, filepath):
        #try:
        file = open(filepath, 'wb')
        #except:
        #    print('Could not open file for writing!')
        #    return
        file.write(self.data())
        file.close()

class BSMExporter(bpy.types.Operator, ExportHelper):
    """Export a Binary Static Mesh (.bsm)"""
    
    bl_idname = "export.bsm"
    bl_label = "Export BSM"
    filename_ext = ".bsm"
    
    vismeshname  = bpy.props.StringProperty(name="Occlusion Mesh Name", description="Name of mesh used for occlusion in dynamic visibility testing", default="occlusion_mesh")
    hullprefix   = bpy.props.StringProperty(name="Collision Hull Prefix", description="Prefix for convex hulls used in collision detection", default="collision_mesh")
    exportscene  = bpy.props.BoolProperty(name="Export Entire Scene", description="Export all meshes in the scene.  Disable to export selected only.", default=True)
    def execute(self, context):
        filepath = bpy.path.ensure_ext(self.filepath, ".bsm")
        objects = bpy.context.scene.objects if self.exportscene else bpy.context.selected_objects
        meshes = []
        vismesh = None
        hulls = []
        for obj in objects:
            if obj.type != "MESH":
                continue
            mesh = obj.to_mesh(bpy.context.scene, True, "PREVIEW")
            if self.vismeshname and obj.name == self.vismeshname:
                vismesh = mesh
            elif self.hullprefix and obj.name.startswith(self.hullprefix):
                hulls.append(mesh)
            else:
                meshes.append(mesh)

        model = Model()
        for mesh in meshes:
            model.addmesh(mesh)
        model.finalize()
        model.tobsm().write(filepath)
        
        return {'FINISHED'}

def menu_func(self, context):

    self.layout.operator(BSMExporter.bl_idname, text="Binary Static Mesh (.bsm)")

def register():
    bpy.utils.register_class(BSMExporter)
    bpy.types.INFO_MT_file_export.append(menu_func)
    
def unregister():
    bpy.utils.unregister_class(BSMExporter)
    bpy.types.INFO_MT_file_export.remove(menu_func)

if __name__ == "__main__":
    register()
