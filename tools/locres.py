import struct, io, zlib
MAGIC = bytes.fromhex('0e147475674a03fc4a15909dc3377f1b')
LOCMETA_MAGIC = bytes.fromhex('4fee4ca168485583 6c4c46bd70da507c'.replace(' ',''))
def rstr(f):
    n, = struct.unpack('<i', f.read(4))
    if n == 0: return ''
    if n > 0: return f.read(n)[:-1].decode('latin-1')
    return f.read(-2*n)[:-2].decode('utf-16-le')
def wstr(s):
    if s == '': return struct.pack('<i', 0)
    if all(ord(c) < 128 for c in s): b = s.encode('ascii') + b'\0'; return struct.pack('<i', len(b)) + b
    b = s.encode('utf-16-le') + b'\0\0'; return struct.pack('<i', -(len(b)//2)) + b
def str_crc32(s):
    # FCrc::StrCrc32 over TCHAR (UTF-16 code units on Windows), each processed as 4 bytes
    u = s.encode('utf-16-le')
    data = b''.join(u[i:i+2] + b'\0\0' for i in range(0, len(u), 2))
    return zlib.crc32(data) & 0xFFFFFFFF
def read_locres(b):
    f = io.BytesIO(b); assert f.read(16) == MAGIC; ver = f.read(1)[0]
    off, = struct.unpack('<q', f.read(8))
    if ver >= 2: f.read(4)
    cur = f.tell(); f.seek(off); n, = struct.unpack('<i', f.read(4)); strs = []
    for _ in range(n):
        strs.append(rstr(f))
        if ver >= 2: f.read(4)
    f.seek(cur); out = []
    for _ in range(struct.unpack('<I', f.read(4))[0]):
        if ver >= 2: f.read(4)
        ns = rstr(f)
        for _ in range(struct.unpack('<I', f.read(4))[0]):
            if ver >= 2: f.read(4)
            k = rstr(f); h, i = struct.unpack('<Ii', f.read(8)); out.append((ns, k, h, strs[i]))
    return ver, out
def write_locres(entries):
    """entries: list of (ns, key, source, translation). Compact (v1) format."""
    strs = []; sidx = {}
    bns = {}
    for ns, k, src, tr in entries:
        if tr not in sidx: sidx[tr] = len(strs); strs.append(tr)
        bns.setdefault(ns, {})[k] = (str_crc32(src), sidx[tr])
    body = struct.pack('<I', len(bns))
    for ns, keys in bns.items():
        body += wstr(ns) + struct.pack('<I', len(keys))
        for k, (h, i) in keys.items(): body += wstr(k) + struct.pack('<Ii', h, i)
    head_len = 16 + 1 + 8
    off = head_len + len(body)
    tail = struct.pack('<i', len(strs)) + b''.join(wstr(s) for s in strs)
    return MAGIC + bytes([1]) + struct.pack('<q', off) + body + tail
def write_locmeta(native='en', cultures=('en',), target='Game'):
    return LOCMETA_MAGIC + bytes([1]) + wstr(native) + wstr(f'{native}/{target}.locres') + struct.pack('<i', len(cultures)) + b''.join(wstr(c) for c in cultures)
