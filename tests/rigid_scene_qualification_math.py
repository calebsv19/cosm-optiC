"""Independent BR6 geometry/oracles. No production solver or renderer imports."""
import math


def sub(a,b): return [x-y for x,y in zip(a,b)]
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def norm(a): return math.sqrt(dot(a,a))


def rotation(q):
    w,x,y,z=q
    return [[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
            [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
            [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]


def world_vertices(mesh,binding,pose):
    m=binding['principal_to_source'];center=binding['source_center_m'];r=rotation(pose['orientation_wxyz'])
    result=[]
    for v in mesh['vertices']:
        source=[v[k]-center[i] for i,k in enumerate('xyz')]
        principal=[sum(m[k*3+i]*source[k] for k in range(3)) for i in range(3)]
        result.append([pose['position_m'][i]+dot(r[i],principal) for i in range(3)])
    return result


def features(vertices,triangles):
    normals=[];edges=[]
    def add_unique(out,v):
        length=norm(v)
        if length<1e-12:return
        axis=[x/length for x in v]
        if not any(abs(dot(axis,old))>1-1e-10 for old in out):out.append(axis)
    for tri in triangles:
        a,b,c=[vertices[tri[k]] for k in ('a','b','c')]
        ab,ac,bc=sub(b,a),sub(c,a),sub(c,b)
        add_unique(normals,cross(ab,ac))
        for e in (ab,ac,bc):add_unique(edges,e)
    return normals,edges


def penetration(a,ta,b,tb):
    """Convex SAT minimum translation depth, zero for separated/touching solids.

    Axes include both meshes' face normals and every nonparallel edge cross.
    This checks the transformed source geometry, independently of contact logs.
    """
    na,ea=features(a,ta);nb,eb=features(b,tb);axes=na+nb
    for x in ea:
        for y in eb:
            axis=cross(x,y);length=norm(axis)
            if length>1e-10:axes.append([v/length for v in axis])
    depth=math.inf
    for axis in axes:
        pa=[dot(v,axis) for v in a];pb=[dot(v,axis) for v in b]
        overlap=min(max(pa)-min(pb),max(pb)-min(pa))
        if overlap<=0:return 0.
        depth=min(depth,overlap)
    return depth


def quaternion_from_matrix(m):
    # Independent conversion of the asset's source/principal frame.
    trace=m[0]+m[4]+m[8]
    if trace>0:
        s=2*math.sqrt(trace+1)
        return [s/4,(m[7]-m[5])/s,(m[2]-m[6])/s,(m[3]-m[1])/s]
    i=max(range(3),key=lambda k:m[k*3+k]);j=(i+1)%3;k=(i+2)%3
    s=2*math.sqrt(1+m[i*3+i]-m[j*3+j]-m[k*3+k]);q=[0.]*4
    q[0]=(m[k*3+j]-m[j*3+k])/s;q[i+1]=s/4
    q[j+1]=(m[j*3+i]+m[i*3+j])/s;q[k+1]=(m[k*3+i]+m[i*3+k])/s
    return q


def prism_stl(path):
    """Right triangular prism: 0.8 x 0.5 base, 0.6 extrusion, offset source origin."""
    vertices=[(.1,-.2,.05),(.9,-.2,.05),(.1,.3,.05),(.1,-.2,.65),(.9,-.2,.65),(.1,.3,.65)]
    faces=[(0,2,1),(3,4,5),(0,1,4),(0,4,3),(1,2,5),(1,5,4),(2,0,3),(2,3,5)]
    lines=['solid br6_prism']
    for face in faces:
        lines+=['facet normal 0 0 0','outer loop']
        lines+=['vertex '+' '.join(format(x,'.17g') for x in vertices[i]) for i in face]
        lines+=['endloop','endfacet']
    path.write_text('\n'.join(lines+['endsolid br6_prism','']))


def prism_mass_oracle(metadata):
    a,b,h=.8,.5,.6
    com=[.1+a/3,-.2+b/3,.05+h/2]
    xx=b*b/18+h*h/12;yy=a*a/18+h*h/12;zz=(a*a+b*b)/18;xy=a*b/36
    delta=math.sqrt((xx-yy)**2+4*xy*xy)
    moments=sorted([(xx+yy-delta)/2,(xx+yy+delta)/2,zz])
    return {'com_error_m':max(abs(x-y) for x,y in zip(com,metadata['source_center_m'])),
            'inertia_per_kg_error':max(abs(x-y) for x,y in zip(moments,sorted(metadata['inertia_per_kg']))),
            'expected_com_m':com,'expected_principal_inertia_per_kg':moments,
            'expected_volume_m3':a*b*h/2}
