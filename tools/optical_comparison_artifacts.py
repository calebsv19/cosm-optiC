"""Dependency-free, labeled SVG contact sheets from verified native BMP cells."""
import base64,html,json,struct,zlib
from pathlib import Path

def bmp_rgba(path):
    data=Path(path).read_bytes()
    if len(data)<54 or data[:2]!=b'BM':raise ValueError('invalid BMP')
    offset=struct.unpack_from('<I',data,10)[0];w,h=struct.unpack_from('<ii',data,18);bpp=struct.unpack_from('<H',data,28)[0];compression=struct.unpack_from('<I',data,30)[0]
    if w<1 or h==0 or w>2048 or abs(h)>2048 or bpp not in (24,32) or compression not in (0,3):raise ValueError('unsupported BMP')
    stride=((w*bpp+31)//32)*4;height=abs(h)
    if len(data)<offset+stride*height:raise ValueError('truncated BMP')
    if compression==3 and (bpp!=32 or len(data)<70 or struct.unpack_from('<III',data,54)!=(0xFF0000,0xFF00,0xFF)):raise ValueError('unsupported BMP masks')
    rgba=bytearray()
    for y in range(height):
        start=offset+(height-1-y if h>0 else y)*stride
        for x in range(w):
            b,g,r=data[start+x*(bpp//8):start+x*(bpp//8)+3];rgba.extend((r,g,b,255))
    return w,height,bytes(rgba)

def png_bytes(w,h,rgba):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xFFFFFFFF)
    pixels=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b'')

def contact_sheet(root,receipt,filename="matrix.svg"):
    root=Path(root);axes=receipt['request']['axes'];cells=receipt['cells'];columns=len(axes[0]['values']);rows=len(axes[1]['values'])
    width=receipt['request']['render']['width'];height=receipt['request']['render']['height'];cellw=max(width,240)+16;cellh=height+52;left=230;top=100
    sw=max(820,left+columns*cellw+20);sh=top+rows*cellh+40
    parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{sw}" height="{sh}" viewBox="0 0 {sw} {sh}">',f'<rect width="{sw}" height="{sh}" fill="#141820"/>','<g fill="#e9edf4" font-family="Arial, sans-serif" font-size="14">']
    def text(x,y,value,size=14):parts.append(f'<text x="{x}" y="{y}" font-size="{size}">{html.escape(str(value))}</text>')
    text(20,30,'optiC optical comparison — '+receipt['request']['comparison_id'],20)
    render=receipt['request']['render'];text(20,55,f"{render['integrator']} · frame {render['frame']} · {render['temporal_frames']} passes · sampling {render['sample_sequence']}")
    text(20,78,'Authored optical parameters; all other scene settings held at baseline.')
    for c,value in enumerate(axes[0]['values']):text(left+c*cellw,top-10,f"{axes[0]['parameter']} = {value:g}")
    for r,value in enumerate(axes[1]['values']):text(15,top+r*cellh+20,f"{axes[1]['parameter']} = {value:g}")
    for cell in cells:
        w,h,rgba=bmp_rgba(root/cell['image_relative']);assert (w,h)==(width,height)
        encoded=base64.b64encode(png_bytes(w,h,rgba)).decode();x=left+cell['column']*cellw;y=top+cell['row']*cellh
        parts.append(f'<image x="{x}" y="{y}" width="{w}" height="{h}" href="data:image/png;base64,{encoded}"/>')
        text(x,y+height+20,cell['image_sha256'][:16],12)
    text(20,sh-15,'Saved cell scenes and verified hashes: matrix.json',12);parts.append('</g></svg>')
    path=root/filename
    with path.open('x') as f:f.write('\n'.join(parts)+'\n')
    return path
