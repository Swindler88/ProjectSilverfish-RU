import os, sys, io
from fontTools.ttLib import TTFont, newTable
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.varLib import instancer

G = os.path.expanduser('~/tools/gfonts/')
PLAN = {
    'DJGROSS.ufont': (G + 'ofl/neucha/Neucha.ttf', {}),
    'Oxanium-SemiBold.ufont': (G + 'ofl/tektur/Tektur[wdth,wght].ttf', {'wght': 600, 'wdth': 100}),
    'UAV-OSD-Sans-Mono.ufont': (G + 'ofl/jetbrainsmono/JetBrainsMono[wght].ttf', {'wght': 400}),
    'VCR_OSD_MONO_1_001.ufont': (G + 'ofl/jetbrainsmono/JetBrainsMono[wght].ttf', {'wght': 600}),
    'alarm_clock.ufont': (G + 'ofl/ptmono/PTM55FT.ttf', {}),
    'atwriter.ufont': (G + 'ofl/ibmplexmono/IBMPlexMono-Bold.ttf', {}),
    'gunplay_rg.ufont': (G + 'ofl/oswald/Oswald[wght].ttf', {'wght': 600}),
    'Nouveau_IBM_Stretch.ufont': ('orig/Flexi_IBM_VGA_True.ufont', {}),
    'whitrabt.ufont': (G + 'ofl/jetbrainsmono/JetBrainsMono[wght].ttf', {'wght': 500}),
}
FORCE_CAPS = {'DJGROSS.ufont'}
EXTRA = [0x00AB, 0x00BB, 0x2014, 0x2013, 0x2116, 0x2026, 0x00B0, 0x201E, 0x201C, 0x201D]
CYR = list(range(0x0400, 0x0460))

def gbounds(font, gname):
    gs = font.getGlyphSet(); bp = BoundsPen(gs); gs[gname].draw(bp); return bp.bounds

def char_bounds(font, ch):
    cm = font.getBestCmap()
    if ord(ch) not in cm: return None
    return gbounds(font, cm[ord(ch)])

def merge(orig_path, out_path, donor_path, axes):
    f = TTFont(orig_path)
    d = TTFont(donor_path)
    if 'fvar' in d: d = instancer.instantiateVariableFont(d, axes)
    fcm = f.getBestCmap(); dcm = d.getBestCmap()
    # vertical scale: cap height ratio
    fb = char_bounds(f, 'H'); db = char_bounds(d, 'H')
    s = (fb[3]) / (db[3])
    # caps-only original? compare lowercase 'x' height to cap height
    fx = char_bounds(f, 'x'); dx = char_bounds(d, 'x')
    caps_only = (fx is not None and fx[3] > 0.88 * fb[3]) or os.path.basename(orig_path) in FORCE_CAPS
    small_caps_scale = None
    if caps_only and fx[3] < 0.98 * fb[3]: small_caps_scale = max(fx[3] / fb[3], 0.8)
    # lowercase vertical scale to match x-height (if not caps-only)
    s_low = s
    if not caps_only and fx and dx: s_low = fx[3] / dx[3]
    # monospace?
    hm = f['hmtx'].metrics
    widths = {hm[fcm[c]][0] for c in range(0x21, 0x7F) if c in fcm}
    mono = len(widths) == 1
    mono_w = widths.pop() if mono else None
    dgs = d.getGlyphSet()
    glyf = f['glyf']; order = list(f.getGlyphOrder())
    added = 0
    for cp in CYR + EXTRA:
        if cp in fcm or cp not in dcm: continue
        src_cp = cp
        is_lower = (0x0430 <= cp <= 0x045F)
        if caps_only and is_lower:
            up = cp - 0x20 if 0x0430 <= cp <= 0x044F else cp - 0x50
            if up in dcm: src_cp = up
        sg = dcm[src_cp]
        sc = s
        if is_lower and not caps_only: sc = s_low * 0.5 + s * 0.5 if False else s_low
        if caps_only and is_lower and small_caps_scale: sc = s * small_caps_scale
        sx = sc
        adv = d['hmtx'].metrics[sg][0] * sx
        dx_shift = 0
        if mono:
            # fit into mono cell
            bb = gbounds(d, sg)
            if bb:
                gw = (bb[2] - bb[0]) * sx
                maxw = mono_w * 0.92
                if gw > maxw: sx = sx * maxw / gw; gw = maxw
                dx_shift = (mono_w - gw) / 2 - bb[0] * sx
            adv = mono_w
        pen = TTGlyphPen(None)
        tp = TransformPen(pen, (sx, 0, 0, sc, dx_shift, 0))
        rp = DecomposingRecordingPen(dgs); dgs[sg].draw(rp); rp.replay(tp)
        g = pen.glyph()
        name = 'uni%04X' % cp
        while name in glyf: name += '_'
        glyf[name] = g
        order.append(name)
        if g.numberOfContours: g.recalcBounds(glyf); lsb = g.xMin
        else: lsb = 0
        f['hmtx'].metrics[name] = (int(round(adv)), int(lsb))
        for t in f['cmap'].tables:
            if t.isUnicode() or (t.platformID == 3 and t.platEncID in (1, 10)):
                t.cmap[cp] = name
        added += 1
    f.setGlyphOrder(order)
    if 'post' in f and f['post'].formatType == 2.0:
        f['post'].extraNames = []; f['post'].mapping = {}
    for tag in ('hdmx', 'LTSH', 'VDMX', 'DSIG', 'vhea', 'vmtx'):
        if tag in f: del f[tag]
    if 'maxp' in f: f['maxp'].numGlyphs = len(order)
    f['hhea'].numberOfHMetrics = len(order)
    f.save(out_path)
    return added, caps_only, mono, s

if __name__ == '__main__':
    os.makedirs('merged', exist_ok=True)
    for name, (donor, axes) in PLAN.items():
        r = merge('orig/' + name, 'merged/' + name, donor, axes)
        print(name, 'added=%d caps_only=%s mono=%s scale=%.3f' % r)
