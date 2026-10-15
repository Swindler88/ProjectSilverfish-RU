import struct, hashlib, io, ctypes, os
MAGIC = 0x5A6F12E1
def rstr(f):
    n, = struct.unpack('<i', f.read(4))
    if n == 0: return ''
    if n > 0: return f.read(n)[:-1].decode('latin-1')
    return f.read(-2*n)[:-2].decode('utf-16-le')
def wstr(s):
    if s == '': return struct.pack('<i', 0)
    try: b = s.encode('ascii') + b'\0'; return struct.pack('<i', len(b)) + b
    except UnicodeEncodeError: b = s.encode('utf-16-le') + b'\0\0'; return struct.pack('<i', -(len(b)//2)) + b
_oodle = None
def oodle_dec(src, rawlen):
    global _oodle
    if _oodle is None:
        _oodle = ctypes.CDLL(os.environ.get('OODLE_LIB', 'liboo2corelinux64.so.9'))
        _oodle.OodleLZ_Decompress.restype = ctypes.c_ssize_t
    out = ctypes.create_string_buffer(rawlen)
    n = _oodle.OodleLZ_Decompress(src, ctypes.c_ssize_t(len(src)), out, ctypes.c_ssize_t(rawlen), 1, 0, 0, None, 0, None, None, None, 0, 3)
    assert n == rawlen, (n, rawlen)
    return out.raw
def read_pak(path):
    f = open(path, 'rb'); f.seek(0, 2); sz = f.tell()
    f.seek(sz - 221); ft = f.read(221)
    enc = ft[16]; magic, ver, ioff, isz = struct.unpack_from('<IiQQ', ft, 17)
    assert magic == MAGIC and ver == 11 and not enc
    comp = [ft[61+32*i:61+32*(i+1)].rstrip(b'\0').decode() for i in range(5)]
    f.seek(ioff); idx = io.BytesIO(f.read(isz))
    mount = rstr(idx); count, seed = struct.unpack('<IQ', idx.read(12))
    if struct.unpack('<I', idx.read(4))[0]: idx.read(36)
    assert struct.unpack('<I', idx.read(4))[0]
    fdo, fds = struct.unpack('<QQ', idx.read(16)); idx.read(20)
    esz, = struct.unpack('<I', idx.read(4)); enc_entries = idx.read(esz)
    f.seek(fdo); fdi = io.BytesIO(f.read(fds))
    files = {}
    for _ in range(struct.unpack('<I', fdi.read(4))[0]):
        d = rstr(fdi)
        for _ in range(struct.unpack('<I', fdi.read(4))[0]):
            fn = rstr(fdi); off, = struct.unpack('<i', fdi.read(4))
            files[(d.lstrip('/') + fn)] = off
    def entry(off):
        e = io.BytesIO(enc_entries); e.seek(off)
        bits, = struct.unpack('<I', e.read(4))
        c = (bits >> 23) & 0x3f; nblk = (bits >> 6) & 0xffff; bs = bits & 0x3f
        if bs == 0x3f: bs, = struct.unpack('<I', e.read(4))
        else: bs <<= 11
        vi = lambda bit: struct.unpack('<I', e.read(4))[0] if bits & (1 << bit) else struct.unpack('<Q', e.read(8))[0]
        o = vi(31); u = vi(30); cs = vi(29) if c else u
        blocks = []
        base = 8+8+8+4+20 + (4 + 16*nblk if c else 0) + 1 + 4
        if c:
            if nblk == 1: blocks = [(base, base + cs)]
            else:
                p = base
                for _ in range(nblk): b, = struct.unpack('<I', e.read(4)); blocks.append((p, p+b)); p += b
        return dict(off=o, usize=u, csize=cs, comp=comp[c-1] if c else None, blocks=blocks, bsize=bs)
    def read(name):
        en = entry(files[name])
        if not en['comp']:
            f.seek(en['off'] + 8+8+8+4+20+1+4); return f.read(en['usize'])
        out = b''
        for i, (a, b) in enumerate(en['blocks']):
            f.seek(en['off'] + a); data = f.read(b - a)
            ul = min(en['bsize'] or en['usize'], en['usize'] - len(out))
            out += oodle_dec(data, ul)
        return out
    return mount, files, read
def sha1(b): return hashlib.sha1(b).digest()
def fnv64_path(path, seed):
    h = (0xcbf29ce484222325 + seed) & 0xFFFFFFFFFFFFFFFF
    for b in path.lower().encode('utf-16-le'):
        h ^= b; h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h
def write_pak(path, mount, files):
    """files: dict relpath -> bytes. Uncompressed v11 pak."""
    out = io.BytesIO(); recs = []
    names = sorted(files)
    for n in names:
        d = files[n]; off = out.tell()
        out.write(struct.pack('<QQQI', 0, len(d), len(d), 0) + sha1(d) + b'\0' + struct.pack('<I', 0))
        out.write(d); recs.append((n, off, len(d)))
    index_offset = out.tell()
    enc = io.BytesIO(); offs = []
    for n, off, sz in recs:
        offs.append(enc.tell())
        flags = (1 << 31) | (1 << 30) | (1 << 29)
        enc.write(struct.pack('<III', flags, off, sz))
    enc = enc.getvalue()
    seed = 0
    phi = struct.pack('<I', len(recs)) + b''.join(struct.pack('<QI', fnv64_path(n, seed), o) for (n, _, _), o in zip(recs, offs)) + struct.pack('<I', 0)
    fdi_map = {}
    def split(p):
        p = p.rstrip('/') if p != '/' else p
        i = p.rfind('/')
        return (p[:i+1], p[i+1:]) if i >= 0 else ('/', p)
    for (n, _, _), o in zip(recs, offs):
        p = n
        while p not in ('', '/'):
            parent, _c = split(p); fdi_map.setdefault(parent, {}); p = parent
        d, fn = split(n); fdi_map.setdefault(d, {})[fn] = o
    fdi = struct.pack('<I', len(fdi_map))
    for d in sorted(fdi_map):
        fdi += wstr(d) + struct.pack('<I', len(fdi_map[d]))
        for fn in sorted(fdi_map[d]): fdi += wstr(fn) + struct.pack('<I', fdi_map[d][fn])
    mp = wstr(mount)
    before_phi = len(mp) + 4 + 8 + 4 + 36 + 4 + 36 + 4 + len(enc) + 4
    phi_off = index_offset + before_phi; fdi_off = phi_off + len(phi)
    idx = mp + struct.pack('<IQ', len(recs), seed)
    idx += struct.pack('<IQQ', 1, phi_off, len(phi)) + sha1(phi)
    idx += struct.pack('<IQQ', 1, fdi_off, len(fdi)) + sha1(fdi)
    idx += struct.pack('<I', len(enc)) + enc + struct.pack('<I', 0)
    assert len(idx) == before_phi
    out.write(idx); out.write(phi); out.write(fdi)
    footer = b'\0'*16 + b'\0' + struct.pack('<IiQQ', MAGIC, 11, index_offset, len(idx)) + sha1(idx) + b'\0'*(32*5)
    assert len(footer) == 221
    out.write(footer)
    open(path, 'wb').write(out.getvalue())
