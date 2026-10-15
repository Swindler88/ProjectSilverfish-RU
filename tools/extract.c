// Silverfish text extractor: reads IoStore (.utoc/.ucas), decompresses package chunks with Oodle,
// scans for localizable FText (namespace/key/source) and writes them as JSON lines.
// Read-only with respect to the game files.
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
typedef unsigned short PCH;
#define P(s) L##s
#define WINAPI __stdcall
#define DI __declspec(dllimport)
DI void* WINAPI GetStdHandle(uint32_t);
DI int WINAPI WriteFile(void*, const void*, uint32_t, uint32_t*, void*);
DI int WINAPI ReadFile(void*, void*, uint32_t, uint32_t*, void*);
DI void* WINAPI CreateFileW(const PCH*, uint32_t, uint32_t, void*, uint32_t, uint32_t, void*);
DI int WINAPI SetFilePointerEx(void*, int64_t, int64_t*, uint32_t);
DI int WINAPI GetFileSizeEx(void*, int64_t*);
DI int WINAPI CloseHandle(void*);
DI uint32_t WINAPI GetModuleFileNameW(void*, PCH*, uint32_t);
DI void* WINAPI LoadLibraryW(const PCH*);
DI void* WINAPI GetProcAddress(void*, const char*);
DI void* WINAPI VirtualAlloc(void*, size_t, uint32_t, uint32_t);
DI int WINAPI VirtualFree(void*, size_t, uint32_t);
DI void WINAPI ExitProcess(uint32_t);
DI uint32_t WINAPI GetLastError(void);
DI int WINAPI SetConsoleOutputCP(uint32_t);
DI uint32_t WINAPI GetFileAttributesW(const PCH*);
#define INVALID_H ((void*)(intptr_t)-1)
void* memcpy(void* d, const void* s, size_t n) { uint8_t* a = d; const uint8_t* b = s; while (n--) *a++ = *b++; return d; }
void* memset(void* d, int c, size_t n) { uint8_t* a = d; while (n--) *a++ = (uint8_t)c; return d; }
int _fltused = 0;
static void out_raw(const char* s, size_t n) { uint32_t w; WriteFile(GetStdHandle((uint32_t)-11), s, (uint32_t)n, &w, 0); }
static void* xalloc(size_t n) { return VirtualAlloc(0, n, 0x3000, 4); }
static void xfree(void* p) { if (p) VirtualFree(p, 0, 0x8000); }
typedef void* FH;
static FH f_open(const PCH* p, int write) {
    void* h = write ? CreateFileW(p, 0x40000000, 1, 0, 2, 0x80, 0) : CreateFileW(p, 0x80000000, 1 | 2, 0, 3, 0x80, 0);
    return h == INVALID_H ? 0 : h;
}
static int64_t f_size(FH h) { int64_t s = 0; GetFileSizeEx(h, &s); return s; }
static int f_pread(FH h, void* buf, size_t n, int64_t off) {
    if (!SetFilePointerEx(h, off, 0, 0)) return 0;
    uint8_t* b = buf;
    while (n) { uint32_t want = n > (1u << 30) ? (1u << 30) : (uint32_t)n, got = 0; if (!ReadFile(h, b, want, &got, 0) || !got) return 0; b += got; n -= got; }
    return 1;
}
static int f_write(FH h, const void* buf, size_t n) { uint32_t w; return WriteFile(h, buf, (uint32_t)n, &w, 0) && w == n; }
static void f_close(FH h) { CloseHandle(h); }
static int p_exists(const PCH* p) { return GetFileAttributesW(p) != 0xFFFFFFFFu; }
static void* lib_load(const PCH* p) { return LoadLibraryW(p); }
static void* lib_sym(void* l, const char* n) { return GetProcAddress(l, n); }
static void wait_enter(void) { char c[8]; uint32_t r; ReadFile(GetStdHandle((uint32_t)-10), c, 1, &r, 0); }
static size_t exe_dir(PCH* buf, size_t cap) {
    uint32_t n = GetModuleFileNameW(0, buf, (uint32_t)cap);
    while (n && buf[n - 1] != '\\' && buf[n - 1] != '/') n--;
    buf[n] = 0; return n;
}
static void sys_exit(int c) { ExitProcess((uint32_t)c); }
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/stat.h>
typedef char PCH;
#define P(s) s
static void out_raw(const char* s, size_t n) { fwrite(s, 1, n, stdout); fflush(stdout); }
static void* xalloc(size_t n) { return calloc(1, n); }
static void xfree(void* p) { free(p); }
typedef FILE* FH;
static FH f_open(const PCH* p, int write) { return fopen(p, write ? "wb" : "rb"); }
static int64_t f_size(FH h) { fseeko(h, 0, SEEK_END); return ftello(h); }
static int f_pread(FH h, void* buf, size_t n, int64_t off) { if (fseeko(h, off, SEEK_SET)) return 0; return fread(buf, 1, n, h) == n; }
static int f_write(FH h, const void* buf, size_t n) { return fwrite(buf, 1, n, h) == n; }
static void f_close(FH h) { fclose(h); }
static int p_exists(const PCH* p) { struct stat st; return stat(p, &st) == 0; }
static void* lib_load(const PCH* p) { return dlopen(p, RTLD_NOW); }
static void* lib_sym(void* l, const char* n) { return dlsym(l, n); }
static void wait_enter(void) {}
static size_t exe_dir(PCH* buf, size_t cap) { const char* d = getenv("SFX_DIR"); if (!d) d = "./"; size_t n = strlen(d); memcpy(buf, d, n + 1); return n; }
static void sys_exit(int c) { exit(c); }
#endif

// ---------- small utils ----------
static size_t c_strlen(const char* s) { size_t n = 0; while (s[n]) n++; return n; }
static void out(const char* s) { out_raw(s, c_strlen(s)); }
static void out_u64(uint64_t v) { char b[24]; int i = 23; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); out(b + i); }
static void pjoin(PCH* dst, const PCH* base, const char* rel) {
    size_t i = 0; while (base[i]) { dst[i] = base[i]; i++; }
    for (size_t j = 0; rel[j]; j++) { char c = rel[j];
#ifdef _WIN32
        if (c == '/') c = '\\';
#endif
        dst[i++] = (PCH)(unsigned char)c; }
    dst[i] = 0;
}
static void fail(const char* msg) { out("\r\nОШИБКА: "); out(msg); out("\r\nНажмите Enter, чтобы закрыть окно.\r\n"); wait_enter(); sys_exit(1); }

// ---------- buffered writer ----------
typedef struct { FH h; uint8_t* buf; size_t n, cap; uint64_t total; } W;
static void w_flush(W* w) { if (w->n) { if (!f_write(w->h, w->buf, w->n)) fail("не удалось записать выходной файл"); w->total += w->n; w->n = 0; } }
static void w_put(W* w, const void* d, size_t n) {
    const uint8_t* s = d;
    while (n) { size_t k = w->cap - w->n; if (k > n) k = n; memcpy(w->buf + w->n, s, k); w->n += k; s += k; n -= k; if (w->n == w->cap) w_flush(w); }
}
static void w_str(W* w, const char* s) { w_put(w, s, c_strlen(s)); }
static void w_u64(W* w, uint64_t v) { char b[24]; int i = 23; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); w_str(w, b + i); }
static void w_open(W* w, const PCH* path) { w->h = f_open(path, 1); if (!w->h) fail("не удалось создать выходной файл"); w->cap = 1 << 20; w->buf = xalloc(w->cap); w->n = 0; w->total = 0; }
static void w_close(W* w) { w_flush(w); f_close(w->h); }

// JSON string from either ANSI bytes or UTF-16LE units
static void w_jchar(W* w, uint32_t c) {
    char b[8];
    if (c == '"' || c == '\\') { b[0] = '\\'; b[1] = (char)c; w_put(w, b, 2); return; }
    if (c == '\n') { w_put(w, "\\n", 2); return; }
    if (c == '\r') { w_put(w, "\\r", 2); return; }
    if (c == '\t') { w_put(w, "\\t", 2); return; }
    if (c < 0x20) { const char* hx = "0123456789abcdef"; b[0]='\\';b[1]='u';b[2]='0';b[3]='0';b[4]=hx[c>>4];b[5]=hx[c&15]; w_put(w, b, 6); return; }
    if (c < 0x80) { b[0] = (char)c; w_put(w, b, 1); }
    else if (c < 0x800) { b[0] = 0xC0 | (c >> 6); b[1] = 0x80 | (c & 63); w_put(w, b, 2); }
    else if (c < 0x10000) { b[0] = 0xE0 | (c >> 12); b[1] = 0x80 | ((c >> 6) & 63); b[2] = 0x80 | (c & 63); w_put(w, b, 3); }
    else { b[0] = 0xF0 | (c >> 18); b[1] = 0x80 | ((c >> 12) & 63); b[2] = 0x80 | ((c >> 6) & 63); b[3] = 0x80 | (c & 63); w_put(w, b, 4); }
}
typedef struct { const uint8_t* p; uint32_t len; int wide; } S; // len in chars, excluding terminator
static void w_js(W* w, S s) {
    w_put(w, "\"", 1);
    if (!s.wide) for (uint32_t i = 0; i < s.len; i++) w_jchar(w, s.p[i]);
    else for (uint32_t i = 0; i < s.len; i++) {
        uint32_t c = s.p[2 * i] | (s.p[2 * i + 1] << 8);
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.len) { uint32_t d = s.p[2 * i + 2] | (s.p[2 * i + 3] << 8); c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); i++; }
        w_jchar(w, c);
    }
    w_put(w, "\"", 1);
}

// ---------- FString validation ----------
// mode: 0 = identifier-ish (namespace/key), 1 = display text
static int ok_char(uint32_t c, int mode) {
    if (c == '\n' || c == '\r' || c == '\t') return mode == 1;
    if (c < 0x20 || c == 0x7F) return 0;
    if (c == 0xFFFE || c == 0xFFFF) return 0;
    return 1;
}
// returns position after the string or 0 on failure
static size_t rd_fstring(const uint8_t* b, size_t n, size_t p, uint32_t maxlen, int allow_empty, int mode, S* s) {
    if (p + 4 > n) return 0;
    int32_t L = (int32_t)(b[p] | (b[p + 1] << 8) | (b[p + 2] << 16) | ((uint32_t)b[p + 3] << 24));
    p += 4;
    if (L == 0) { if (!allow_empty) return 0; s->p = b + p; s->len = 0; s->wide = 0; return p; }
    if (L > 0) {
        if ((uint32_t)L > maxlen + 1 || p + (size_t)L > n) return 0;
        if (b[p + L - 1] != 0) return 0;
        if (L == 1 && !allow_empty) return 0;
        for (int32_t i = 0; i < L - 1; i++) { uint8_t c = b[p + i]; if (c >= 0x80 || !ok_char(c, mode)) return 0; }
        s->p = b + p; s->len = (uint32_t)L - 1; s->wide = 0; return p + (size_t)L;
    }
    if (L == INT32_MIN) return 0;
    uint32_t U = (uint32_t)(-L);
    if (U > maxlen + 1 || p + 2 * (size_t)U > n) return 0;
    if (b[p + 2 * (U - 1)] || b[p + 2 * (U - 1) + 1]) return 0;
    if (U == 1 && !allow_empty) return 0;
    int nonascii = 0;
    for (uint32_t i = 0; i + 1 < U; i++) {
        uint32_t c = b[p + 2 * i] | (b[p + 2 * i + 1] << 8);
        if (c >= 0xD800 && c < 0xDC00) { if (i + 2 >= U) return 0; uint32_t d = b[p + 2 * i + 2] | (b[p + 2 * i + 3] << 8); if (d < 0xDC00 || d >= 0xE000) return 0; i++; nonascii = 1; continue; }
        if (c >= 0xDC00 && c < 0xE000) return 0;
        if (!ok_char(c, mode)) return 0;
        if (c >= 0x80) nonascii = 1;
    }
    if (!nonascii && U > 1) return 0; // UE stores pure-ASCII strings as ANSI
    s->p = b + p; s->len = U - 1; s->wide = 1; return p + 2 * (size_t)U;
}
// null-terminated string used by blueprint bytecode (EX_StringConst / EX_UnicodeStringConst)
static size_t rd_bcstring(const uint8_t* b, size_t n, size_t p, uint32_t maxlen, int mode, S* s) {
    if (p >= n) return 0;
    uint8_t tok = b[p++];
    if (tok == 0x1F) {
        size_t st = p; while (p < n && b[p]) { if (b[p] >= 0x80 || !ok_char(b[p], mode)) return 0; p++; if (p - st > maxlen) return 0; }
        if (p >= n) return 0; s->p = b + st; s->len = (uint32_t)(p - st); s->wide = 0; return p + 1;
    }
    if (tok == 0x34) {
        size_t st = p;
        while (p + 1 < n && (b[p] | b[p + 1])) { uint32_t c = b[p] | (b[p + 1] << 8); if (!(c >= 0xD800 && c < 0xE000) && !ok_char(c, mode)) return 0; p += 2; if ((p - st) / 2 > maxlen) return 0; }
        if (p + 1 >= n) return 0; s->p = b + st; s->len = (uint32_t)((p - st) / 2); s->wide = 1; return p + 2;
    }
    return 0;
}
static int has_letter(S s) {
    for (uint32_t i = 0; i < s.len; i++) {
        uint32_t c = s.wide ? (uint32_t)(s.p[2 * i] | (s.p[2 * i + 1] << 8)) : s.p[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0xC0) return 1;
    }
    return 0;
}
static int plausible(S s) {
    if (!s.wide) return 1;
    for (uint32_t i = 0; i < s.len; i++) { uint32_t c = s.p[2 * i] | (s.p[2 * i + 1] << 8); if (!(c < 0x0530 || (c >= 0x2000 && c < 0x2070) || (c >= 0x2100 && c < 0x2200))) return 0; }
    return 1;
}
static int has_space(S s) {
    for (uint32_t i = 0; i < s.len; i++) { uint32_t c = s.wide ? (uint32_t)(s.p[2 * i] | (s.p[2 * i + 1] << 8)) : s.p[i]; if (c == ' ') return 1; }
    return 0;
}

static uint64_t n_ftext, n_bp, n_extra;
static void emit(W* w, const char* type, uint32_t idx, size_t off, uint32_t flags, S ns, S key, S src) {
    w_str(w, "{\"t\":\""); w_str(w, type); w_str(w, "\",\"i\":"); w_u64(w, idx); w_str(w, ",\"o\":"); w_u64(w, off);
    w_str(w, ",\"f\":"); w_u64(w, flags);
    w_str(w, ",\"n\":"); w_js(w, ns); w_str(w, ",\"k\":"); w_js(w, key); w_str(w, ",\"s\":"); w_js(w, src); w_str(w, "}\n");
}
static void scan(W* wt, W* wx, uint32_t idx, const uint8_t* b, size_t n) {
    S ns, key, src;
    for (size_t p = 0; p + 13 < n; p++) {
        uint8_t c = b[p];
        if (c == 0x00 && p >= 4) {
            // FText: uint32 flags, int8 historyType(0 = Base), FString ns, FString key, FString source
            if (b[p - 1] | b[p - 2] | b[p - 3]) continue;
            uint32_t flags = b[p - 4];
            if (flags >= 0x40) continue;
            size_t q = rd_fstring(b, n, p + 1, 512, 1, 0, &ns); if (!q) continue;
            q = rd_fstring(b, n, q, 512, 0, 0, &key); if (!q) continue;
            q = rd_fstring(b, n, q, 200000, 0, 1, &src); if (!q) continue;
            emit(wt, "F", idx, p - 4, flags, ns, key, src); n_ftext++;
        } else if (c == 0x29 && b[p + 1] == 0x01) {
            // Blueprint bytecode EX_TextConst / LocalizedText: source, key, namespace
            size_t q = rd_bcstring(b, n, p + 2, 200000, 1, &src); if (!q) continue;
            q = rd_bcstring(b, n, q, 512, 0, &key); if (!q || key.len == 0) continue;
            q = rd_bcstring(b, n, q, 512, 0, &ns); if (!q) continue;
            emit(wt, "B", idx, p, 0, ns, key, src); n_bp++;
        }
    }
    if (!wx) return;
    // extra: plain strings that look like human text (for finding non-localizable text)
    S none = { b, 0, 0 };
    for (size_t p = 0; p + 5 < n; p++) {
        uint8_t c = b[p];
        if (c == 0x1F || c == 0x34) {
            size_t q = rd_bcstring(b, n, p, 20000, 1, &src);
            if (q && src.len >= 3 && has_letter(src) && plausible(src) && (has_space(src) || src.wide)) { emit(wx, c == 0x1F ? "Xb" : "Xw", idx, p, 0, none, none, src); n_extra++; p = q - 1; continue; }
        }
        size_t q = rd_fstring(b, n, p, 20000, 0, 1, &src);
        if (q && src.len >= 3 && has_letter(src) && plausible(src) && (has_space(src) || src.wide)) { emit(wx, "X", idx, p, 0, none, none, src); n_extra++; p = q - 1; }
    }
}

// ---------- IoStore ----------
typedef intptr_t (*OodleDecompressFn)(const void*, intptr_t, void*, intptr_t, int, int, int, void*, intptr_t, void*, void*, void*, intptr_t, int);
static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t rdn_le(const uint8_t* p, int n) { uint64_t v = 0; for (int i = n - 1; i >= 0; i--) v = (v << 8) | p[i]; return v; }
static uint64_t rdn_be(const uint8_t* p, int n) { uint64_t v = 0; for (int i = 0; i < n; i++) v = (v << 8) | p[i]; return v; }

typedef struct { uint64_t off, len; uint32_t idx; } Entry;
static void sort_entries(Entry* a, uint32_t n) { // heap sort by off
    for (uint32_t s = n / 2; s-- > 0;) { uint32_t r = s; for (;;) { uint32_t c = 2 * r + 1; if (c >= n) break; if (c + 1 < n && a[c + 1].off > a[c].off) c++; if (a[r].off >= a[c].off) break; Entry t = a[r]; a[r] = a[c]; a[c] = t; r = c; } }
    for (uint32_t e = n; e-- > 1;) { Entry t = a[0]; a[0] = a[e]; a[e] = t; uint32_t r = 0; for (;;) { uint32_t c = 2 * r + 1; if (c >= e) break; if (c + 1 < e && a[c + 1].off > a[c].off) c++; if (a[r].off >= a[c].off) break; t = a[r]; a[r] = a[c]; a[c] = t; r = c; } }
}

static PCH g_dir[1024], g_path[2048];

static int find_paks(PCH* utoc, PCH* ucas) {
    static const char* cands[] = { "SilverFish/Content/Paks/", "../SilverFish/Content/Paks/", "../../SilverFish/Content/Paks/", "", 0 };
    for (int i = 0; cands[i]; i++) {
        char rel[256]; size_t k = 0; const char* c = cands[i]; while (*c) rel[k++] = *c++;
        const char* f = "SilverFish-Windows.utoc"; size_t k2 = k; while (*f) rel[k2++] = *f++; rel[k2] = 0;
        pjoin(utoc, g_dir, rel);
        if (p_exists(utoc)) { rel[k2 - 4] = 'u'; rel[k2 - 3] = 'c'; rel[k2 - 2] = 'a'; rel[k2 - 1] = 's'; pjoin(ucas, g_dir, rel); return 1; }
    }
    return 0;
}

#ifdef _WIN32
void mainCRTStartup(void)
#else
int main(void)
#endif
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    out("Silverfish: извлечение текста для перевода\r\n");
    exe_dir(g_dir, 1024);
    static PCH utoc_p[2048], ucas_p[2048];
    if (!find_paks(utoc_p, ucas_p)) fail("не найден SilverFish-Windows.utoc. Положите программу в папку игры или в папку _ru_tmp внутри неё.");

    pjoin(g_path, g_dir, "oo2core_9_win64.dll");
#ifndef _WIN32
    pjoin(g_path, g_dir, "liboo2corelinux64.so.9");
#endif
    void* lib = lib_load(g_path);
    if (!lib) fail("не удалось загрузить oo2core_9_win64.dll (должна лежать рядом с программой)");
    OodleDecompressFn dec = (OodleDecompressFn)lib_sym(lib, "OodleLZ_Decompress");
    if (!dec) fail("в oo2core_9_win64.dll нет OodleLZ_Decompress");

    FH ft = f_open(utoc_p, 0); if (!ft) fail("не удалось открыть .utoc");
    int64_t tsz = f_size(ft);
    uint8_t* toc = xalloc((size_t)tsz + 16);
    if (!f_pread(ft, toc, (size_t)tsz, 0)) fail("не удалось прочитать .utoc");
    f_close(ft);
    uint32_t ver = toc[16], hsize = rd32(toc + 20), E = rd32(toc + 24), NB = rd32(toc + 28), MC = rd32(toc + 36), ML = rd32(toc + 40), CB = rd32(toc + 44);
    uint32_t flags = toc[80], seeds = rd32(toc + 84), nohash = rd32(toc + 96);
    if (flags & 2) fail("архив зашифрован");
    const uint8_t* ids = toc + hsize;
    const uint8_t* ol = ids + 12ull * E;
    const uint8_t* q = ol + 10ull * E;
    q += 4ull * seeds; if (ver >= 5) q += 4ull * nohash;
    const uint8_t* blocks = q;
    const uint8_t* methods = blocks + 12ull * NB; (void)methods; (void)MC; (void)ML;
    out("Записей в оглавлении: "); out_u64(E); out(", блоков: "); out_u64(NB); out("\r\n");

    FH fc = f_open(ucas_p, 0); if (!fc) fail("не удалось открыть .ucas");

    // optional dump list
    static uint8_t want_dump[1 << 17];
    {
        pjoin(g_path, g_dir, "dump_list.txt");
        FH fd = f_open(g_path, 0);
        if (fd) {
            int64_t s = f_size(fd); uint8_t* t = xalloc((size_t)s + 1);
            if (f_pread(fd, t, (size_t)s, 0)) { uint32_t v = 0; int have = 0; for (int64_t i = 0; i <= s; i++) { uint8_t c = i < s ? t[i] : '\n'; if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); have = 1; } else { if (have && v < (1u << 17)) want_dump[v] = 1; v = 0; have = 0; } } }
            f_close(fd); xfree(t);
        }
    }

    Entry* ent = xalloc(sizeof(Entry) * (size_t)E); uint32_t ne = 0;
    for (uint32_t i = 0; i < E; i++) {
        uint8_t type = ids[12ull * i + 11];
        if (type != 1) continue;
        ent[ne].off = rdn_be(ol + 10ull * i, 5); ent[ne].len = rdn_be(ol + 10ull * i + 5, 5); ent[ne].idx = i; ne++;
    }
    sort_entries(ent, ne);
    out("Пакетов с данными: "); out_u64(ne); out("\r\n");

    W wt, wx, wd;
    pjoin(g_path, g_dir, "strings.jsonl"); w_open(&wt, g_path);
    pjoin(g_path, g_dir, "strings_extra.jsonl"); w_open(&wx, g_path);
    pjoin(g_path, g_dir, "dumps.bin"); w_open(&wd, g_path);

    size_t cap_raw = 0, cap_cmp = 0; uint8_t *raw = 0, *cmp = 0;
    uint64_t errors = 0, bytes = 0;
    for (uint32_t e = 0; e < ne; e++) {
        uint64_t off = ent[e].off, len = ent[e].len;
        if (!len) continue;
        uint64_t b0 = off / CB, b1 = (off + len - 1) / CB;
        if (b1 >= NB) { errors++; continue; }
        size_t rawsz = (size_t)((b1 - b0 + 1) * CB);
        uint64_t c0 = rdn_le(blocks + 12 * b0, 5);
        const uint8_t* lb = blocks + 12 * b1;
        uint64_t cend = rdn_le(lb, 5) + rdn_le(lb + 5, 3);
        size_t cmpsz = (size_t)(cend - c0);
        if (rawsz > cap_raw) { xfree(raw); cap_raw = rawsz + (1 << 20); raw = xalloc(cap_raw); }
        if (cmpsz > cap_cmp) { xfree(cmp); cap_cmp = cmpsz + (1 << 20); cmp = xalloc(cap_cmp); }
        if (!raw || !cmp) fail("не хватает памяти");
        if (!f_pread(fc, cmp, cmpsz, (int64_t)c0)) { errors++; continue; }
        size_t rp = 0; int bad = 0;
        for (uint64_t bi = b0; bi <= b1; bi++) {
            const uint8_t* be = blocks + 12 * bi;
            uint64_t bo = rdn_le(be, 5) - c0; uint32_t bc = (uint32_t)rdn_le(be + 5, 3), bu = (uint32_t)rdn_le(be + 8, 3); uint8_t m = be[11];
            if (m == 0) { memcpy(raw + rp, cmp + bo, bu); }
            else { intptr_t r = dec(cmp + bo, bc, raw + rp, bu, 1, 0, 0, 0, 0, 0, 0, 0, 0, 3); if (r != (intptr_t)bu) { bad = 1; break; } }
            rp += bu;
        }
        if (bad) { errors++; continue; }
        const uint8_t* data = raw + (off - b0 * CB);
        size_t dlen = (size_t)len;
        scan(&wt, &wx, ent[e].idx, data, dlen);
        if (ent[e].idx < (1u << 17) && want_dump[ent[e].idx]) {
            uint8_t h[8]; uint32_t ix = ent[e].idx, l = (uint32_t)dlen;
            for (int i = 0; i < 4; i++) { h[i] = (uint8_t)(ix >> (8 * i)); h[4 + i] = (uint8_t)(l >> (8 * i)); }
            w_put(&wd, h, 8); w_put(&wd, data, dlen);
        }
        bytes += dlen;
        if ((e & 1023) == 0 || e + 1 == ne) { out("\r  обработано "); out_u64(e + 1); out(" / "); out_u64(ne); out("  (найдено строк: "); out_u64(n_ftext + n_bp); out(")   "); }
    }
    f_close(fc);
    w_close(&wt); w_close(&wx); w_close(&wd);
    out("\r\n\r\nГотово!\r\n  строк для перевода: "); out_u64(n_ftext + n_bp);
    out("\r\n  прочих строк: "); out_u64(n_extra);
    out("\r\n  ошибок распаковки: "); out_u64(errors);
    out("\r\n  распаковано МБ: "); out_u64(bytes >> 20);
    out("\r\nФайлы strings.jsonl, strings_extra.jsonl и dumps.bin лежат рядом с программой.\r\nНапишите Claude, что всё готово. Нажмите Enter, чтобы закрыть окно.\r\n");
    wait_enter();
    sys_exit(0);
#ifndef _WIN32
    return 0;
#endif
}
