import struct, sys, collections
def read_fstring(b, p):
    n, = struct.unpack_from('<i', b, p); p += 4
    if n == 0: return '', p
    if n > 0: s = b[p:p+n-1].decode('latin-1'); return s, p+n
    n = -n; s = b[p:p+2*n-2].decode('utf-16-le'); return s, p+2*n
def parse(path):
    b = open(path,'rb').read()
    h = {}
    (h['ver'],) = struct.unpack_from('<B', b, 16)
    (h['hsize'], h['entries'], h['blocks'], h['bsize'], h['mcount'], h['mlen'], h['cblock'], h['dirsize'], h['parts']) = struct.unpack_from('<9I', b, 20)
    h['flags'] = b[80]
    h['seeds'], = struct.unpack_from('<I', b, 84)
    h['partsize'], = struct.unpack_from('<Q', b, 88)
    h['nohash'], = struct.unpack_from('<I', b, 96)
    p = h['hsize']; E = h['entries']
    ids = [struct.unpack_from('<QHBB', b, p+12*i) for i in range(E)]; p += 12*E
    ol = []
    for i in range(E):
        d = b[p+10*i:p+10*i+10]
        ol.append((int.from_bytes(d[0:5],'big'), int.from_bytes(d[5:10],'big')))
    p += 10*E
    p += 4*h['seeds']
    if h['ver'] >= 5: p += 4*h['nohash']
    blocks = []
    for i in range(h['blocks']):
        d = b[p+12*i:p+12*i+12]
        blocks.append((int.from_bytes(d[0:5],'little'), int.from_bytes(d[5:8],'little'), int.from_bytes(d[8:11],'little'), d[11]))
    p += 12*h['blocks']
    methods = [b[p+h['mlen']*i:p+h['mlen']*(i+1)].rstrip(b'\0').decode() for i in range(h['mcount'])]; p += h['mlen']*h['mcount']
    di = b[p:p+h['dirsize']]; p += h['dirsize']
    # directory index
    q = 0
    mount, q = read_fstring(di, q)
    n, = struct.unpack_from('<I', di, q); q += 4
    dirs = [struct.unpack_from('<4I', di, q+16*i) for i in range(n)]; q += 16*n
    n, = struct.unpack_from('<I', di, q); q += 4
    files = [struct.unpack_from('<3I', di, q+12*i) for i in range(n)]; q += 12*n
    n, = struct.unpack_from('<I', di, q); q += 4
    names = []
    for i in range(n):
        s, q = read_fstring(di, q); names.append(s)
    paths = {}
    NONE = 0xFFFFFFFF
    def walk(d, prefix):
        name, child, sib, ff = dirs[d]
        cur = prefix + ([names[name]] if name != NONE else [])
        f = ff
        while f != NONE:
            fn, nxt, ud = files[f]
            paths[ud] = '/'.join(cur + [names[fn]])
            f = nxt
        c = child
        while c != NONE:
            walk(c, cur); c = dirs[c][2]
    walk(0, [])
    return dict(h=h, ids=ids, ol=ol, blocks=blocks, methods=methods, mount=mount, paths=paths)
if __name__ == '__main__':
    t = parse(sys.argv[1])
    print(t['h'], t['methods'], t['mount'], len(t['paths']))
    c = collections.Counter(i[3] for i in t['ids']); print(c)
    sz = collections.Counter()
    for i,(cid,idx,pad,typ) in enumerate(t['ids']): sz[typ] += t['ol'][i][1]
    print({k: v//2**20 for k,v in sz.items()})
    import itertools
    for i in list(t['paths'])[:10]: print(i, t['ids'][i], t['paths'][i], t['ol'][i])
