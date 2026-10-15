# Usage: python tools/build.py OUT.pak [PREVIOUS_RELEASE.pak]
import json, sys, glob, os, re, locres, pak
W = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..') + '/'
entries = json.load(open(W + 'source/entries.json', encoding='utf-8'))
cat = {c['id']: c['src'] for c in json.load(open(W + 'source/catalog.json', encoding='utf-8'))}
tr = {}
def fix_ws(src, t):
    lead = re.match(r'^\s*', src).group(0); trail = re.search(r'\s*$', src).group(0)
    return lead + t.strip() + trail
bad = []
for f in sorted(glob.glob(W + 'translation/b*.txt')):
    txt = open(f, encoding='utf-8').read()
    parts = re.split(r'^### (\d+)[^\n]*\n', txt, flags=re.M)
    for i in range(1, len(parts), 2):
        cid = int(parts[i]); t = parts[i + 1]
        if t.endswith('\n'): t = t[:-1]
        src = cat[cid]
        t = t.replace('\r\n', '\n').replace('\n', '\r\n') if '\r\n' in src else t
        t = fix_ws(src, t)
        # placeholder check
        ph_s = sorted(re.findall(r'\{[^}]*\}', src)); ph_t = sorted(re.findall(r'\{[^}]*\}', t))
        if ph_s != ph_t: bad.append((cid, src[:60], t[:60]))
        tr[src] = t
out = [(ns, k, s, tr[s]) for ns, k, s in entries if tr.get(s)]
lr = locres.write_locres(out)
lm = locres.write_locmeta('en', ['en'], 'Game')
files = {'SilverFish/Content/Localization/Game/Game.locmeta': lm, 'SilverFish/Content/Localization/Game/en/Game.locres': lr}
# Fonts with Cyrillic: taken from fonts/*.ufont if present, otherwise from a previous release pak (argv[2])
fonts = glob.glob(W + 'fonts/*.ufont')
if fonts:
    for fp in fonts:
        files['SilverFish/Content/Classes/Misc/Fonts/' + os.path.basename(fp)] = open(fp, 'rb').read()
elif len(sys.argv) > 2:
    _m, old, rd = pak.read_pak(sys.argv[2])
    for n in old:
        if n.endswith('.ufont'): files[n] = rd(n)
else:
    print('WARNING: no fonts found; pass a previous release pak as 2nd argument')
dst = sys.argv[1]
pak.write_pak(dst, '../../../', files)
print('translated sources', len(tr), 'entries', len(out), 'of', len(entries), 'pak bytes', os.path.getsize(dst))
for b in bad: print('PLACEHOLDER MISMATCH', b)
