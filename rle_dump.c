/*
 * rle_dump.c — RedMoon RLE → BMP dumper (single-file, cross-platform)
 *
 * Build:
 *   gcc -O2 -o rle_dump rle_dump.c
 *   cl /O2 rle_dump.c          (MSVC)
 *
 * Usage:
 *   rle_dump <folder> [.ext]
 *   rle_dump RLEs/Int .rle
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
  #define PATH_SEP '\\'
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <dirent.h>
  #include <unistd.h>
  #define MKDIR(p) mkdir(p, 0755)
  #define PATH_SEP '/'
#endif

/* ── RLE constants ─────────────────────────────────────────────── */
#define HEADER_LEN    14
#define RLEHDR_SIZE   32
#define PIXFMT_555    1
#define PIXFMT_BGR    128
#define MAX_DIM       4096

/* ── helpers ───────────────────────────────────────────────────── */

static uint32_t rd_u4(const uint8_t *d, int *off) {
    uint32_t v = (uint32_t)d[*off]
              | ((uint32_t)d[*off+1] << 8)
              | ((uint32_t)d[*off+2] << 16)
              | ((uint32_t)d[*off+3] << 24);
    *off += 4;
    return v;
}

static int32_t rd_i4(const uint8_t *d, int *off) {
    uint32_t v = rd_u4(d, off);
    return (int32_t)v;
}

static void mkdirs(const char *path) {
    char tmp[4096];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return;
    memcpy(tmp, path, len + 1);
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            tmp[i] = '\0';
            MKDIR(tmp);
            tmp[i] = PATH_SEP;
        }
    }
    MKDIR(tmp);
}

static uint8_t *load_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

/* ── surface decode ────────────────────────────────────────────── */

typedef struct {
    int tex_w, tex_h;
    uint8_t *pixels; /* RGBA top-to-bottom */
    int has_visible;
} Surface;

static Surface decode_surface(const uint8_t *data, size_t data_len, uint32_t position) {
    Surface s = {0, 0, NULL, 0};
    if (position == 0 || position + 4 + RLEHDR_SIZE > data_len) return s;

    int off = (int)position;
    uint32_t length = rd_u4(data, &off);
    if (length < RLEHDR_SIZE) return s;

    int end_off = (int)position + 4 + (int)length;
    if (end_off > (int)data_len) end_off = (int)data_len;

    int src_x = rd_i4(data, &off);
    int src_y = rd_i4(data, &off);
    int wdh   = rd_i4(data, &off);
    int hgh   = rd_i4(data, &off);
    rd_i4(data, &off); /* adj_x */
    rd_i4(data, &off); /* adj_y */
    int pix_fmt = rd_i4(data, &off);
    rd_i4(data, &off); /* data_ptr */

    if (wdh <= 0 || hgh <= 0 || wdh > MAX_DIM || hgh > MAX_DIM) return s;

    int pad_x = src_x > 0 ? src_x : 0;
    int pad_y = src_y > 0 ? src_y : 0;
    int tex_w = wdh + pad_x;
    int tex_h = hgh + pad_y;

    int is_bgr  = (pix_fmt & PIXFMT_BGR) != 0;
    int base_fmt = pix_fmt & 0x7F;

    size_t pix_size = (size_t)tex_w * (size_t)tex_h * 4;
    uint8_t *buf = (uint8_t *)calloc(1, pix_size);
    if (!buf) return s;

    int curx = 0, cury = 0, has_visible = 0;

    while (off < end_off) {
        uint8_t flag = data[off++];
        if (flag == 1) { /* COLOR RUN */
            if (off + 4 > end_off) break;
            uint32_t count = rd_u4(data, &off);
            for (uint32_t j = 0; j < count; j++) {
                if (cury >= hgh) break;
                if (off + 2 > end_off) break;
                uint16_t tc = (uint16_t)(data[off] | (data[off+1] << 8));
                off += 2;
                if (tc == 0) { curx++; continue; }

                has_visible = 1;
                int r, g, b;
                if (base_fmt == PIXFMT_555) {
                    r = (tc >> 10) & 0x1F; g = (tc >> 5) & 0x1F; b = tc & 0x1F;
                    r = r * 255 / 31; g = g * 255 / 31; b = b * 255 / 31;
                } else {
                    r = (tc >> 11) & 0x1F; g = (tc >> 5) & 0x3F; b = tc & 0x1F;
                    r = r * 255 / 31; g = g * 255 / 63; b = b * 255 / 31;
                }
                if (is_bgr) { int t = r; r = b; b = t; }

                int wx = curx + pad_x, wy = cury + pad_y;
                if (wx >= 0 && wy >= 0 && wx < tex_w && wy < tex_h) {
                    size_t pi = ((size_t)wy * tex_w + wx) * 4;
                    buf[pi] = (uint8_t)r; buf[pi+1] = (uint8_t)g;
                    buf[pi+2] = (uint8_t)b; buf[pi+3] = 255;
                }
                curx++;
            }
        } else if (flag == 2) { /* SKIP */
            if (off + 4 > end_off) break;
            int skip = rd_i4(data, &off);
            curx += skip / 2;
        } else if (flag == 3) { /* NEWLINE */
            cury++;
            if (cury >= hgh) break;
        } else { /* END */
            break;
        }
    }

    if (!has_visible) { free(buf); return s; }

    s.tex_w = tex_w; s.tex_h = tex_h;
    s.pixels = buf; s.has_visible = 1;
    return s;
}

/* ── BMP writer ────────────────────────────────────────────────── */

static void write_bmp(const char *path, int w, int h, const uint8_t *rgba) {
    int row_bytes = w * 3;
    int row_pad = (4 - row_bytes % 4) % 4;
    int row_size = row_bytes + row_pad;
    int pix_data_size = row_size * h;
    int header_size = 14 + 40;
    int file_size = header_size + pix_data_size;

    uint8_t *out = (uint8_t *)calloc(1, (size_t)file_size);
    if (!out) return;

    /* BMP file header */
    out[0] = 'B'; out[1] = 'M';
    out[2] = file_size & 0xFF; out[3] = (file_size >> 8) & 0xFF;
    out[4] = (file_size >> 16) & 0xFF; out[5] = (file_size >> 24) & 0xFF;
    out[10] = header_size & 0xFF;

    /* BITMAPINFOHEADER */
    out[14] = 40;
    out[18] = w & 0xFF; out[19] = (w >> 8) & 0xFF;
    out[20] = (w >> 16) & 0xFF; out[21] = (w >> 24) & 0xFF;
    out[22] = h & 0xFF; out[23] = (h >> 8) & 0xFF;
    out[24] = (h >> 16) & 0xFF; out[25] = (h >> 24) & 0xFF;
    out[26] = 1;  /* planes */
    out[28] = 24; /* bpp */
    out[34] = pix_data_size & 0xFF; out[35] = (pix_data_size >> 8) & 0xFF;
    out[36] = (pix_data_size >> 16) & 0xFF; out[37] = (pix_data_size >> 24) & 0xFF;
    /* 2835 ppm = 0x0B13 */
    out[38] = 0x13; out[39] = 0x0B;
    out[42] = 0x13; out[43] = 0x0B;

    /* Pixel data — BMP bottom-up, buffer top-to-bottom */
    int dst = header_size;
    for (int y = h - 1; y >= 0; y--) {
        int src_row = y * w * 4;
        for (int x = 0; x < w; x++) {
            int pi = src_row + x * 4;
            if (rgba[pi+3] == 0) {
                out[dst] = 0xFF; out[dst+1] = 0x00; out[dst+2] = 0xFF;
            } else {
                out[dst]   = rgba[pi+2]; /* B */
                out[dst+1] = rgba[pi+1]; /* G */
                out[dst+2] = rgba[pi];   /* R */
            }
            dst += 3;
        }
        dst += row_pad;
    }

    FILE *f = fopen(path, "wb");
    if (f) { fwrite(out, 1, (size_t)file_size, f); fclose(f); }
    free(out);
}

/* ── process one .rle file ─────────────────────────────────────── */

static int process_file(const char *rle_path, const char *out_dir) {
    size_t data_len = 0;
    uint8_t *data = load_file(rle_path, &data_len);
    if (!data) return 0;

    if (data_len < HEADER_LEN + 8) { free(data); return 0; }

    int off = HEADER_LEN;
    rd_u4(data, &off); /* version */
    uint32_t count = rd_u4(data, &off);

    /* read offset table */
    uint32_t *offsets = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!offsets) { free(data); return 0; }
    for (uint32_t i = 0; i < count; i++) {
        if (off + 4 > (int)data_len) {
            fprintf(stderr, "  ERROR: offset table truncated at entry %u/%u, file size %zu\n",
                    (unsigned)i, (unsigned)count, data_len);
            free(offsets); free(data); return 0;
        }
        offsets[i] = rd_u4(data, &off);
    }

    /* decode all surfaces */
    Surface *surfs = (Surface *)calloc(count, sizeof(Surface));
    if (!surfs) { free(offsets); free(data); return 0; }

    int visible_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        surfs[i] = decode_surface(data, data_len, offsets[i]);
        if (surfs[i].has_visible) visible_count++;
    }

    /* batch write */
    if (visible_count > 0) {
        mkdirs(out_dir);
        for (uint32_t i = 0; i < count; i++) {
            if (!surfs[i].has_visible) continue;
            char bmp_path[4096];
            snprintf(bmp_path, sizeof(bmp_path), "%s%c%05u.bmp", out_dir, PATH_SEP, (unsigned)i);
            write_bmp(bmp_path, surfs[i].tex_w, surfs[i].tex_h, surfs[i].pixels);
        }
    }

    for (uint32_t i = 0; i < count; i++)
        free(surfs[i].pixels);
    free(surfs);
    free(offsets);
    free(data);
    return visible_count;
}

/* ── directory walk ────────────────────────────────────────────── */

static int total_files = 0;
static int total_images = 0;

static int str_ends_with_ci(const char *s, const char *suffix) {
    size_t slen = strlen(s), suflen = strlen(suffix);
    if (slen < suflen) return 0;
    for (size_t i = 0; i < suflen; i++) {
        char a = s[slen - suflen + i], b = suffix[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
    }
    return 1;
}

static void walk_dir(const char *src_dir, const char *dump_dir,
                     const char *rel_prefix, const char *ext)
{
#ifdef _WIN32
    char pattern[4096];
    snprintf(pattern, sizeof(pattern), "%s\\*", src_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == '.') continue;
        char full[4096], rel[4096];
        snprintf(full, sizeof(full), "%s\\%s", src_dir, fd.cFileName);
        if (rel_prefix[0])
            snprintf(rel, sizeof(rel), "%s\\%s", rel_prefix, fd.cFileName);
        else
            snprintf(rel, sizeof(rel), "%s", fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char sub_dump[4096];
            snprintf(sub_dump, sizeof(sub_dump), "%s\\%s", dump_dir, fd.cFileName);
            walk_dir(full, sub_dump, rel, ext);
        } else if (str_ends_with_ci(fd.cFileName, ext)) {
            /* strip extension for output dir */
            char stem[4096];
            snprintf(stem, sizeof(stem), "%s", rel);
            size_t slen = strlen(stem), elen = strlen(ext);
            if (slen > elen) stem[slen - elen] = '\0';

            char out_dir[4096];
            snprintf(out_dir, sizeof(out_dir), "%s\\%s", dump_dir, stem);
            /* but we need dump_dir based on top-level, recalc */
            /* out_dir is already correct since dump_dir mirrors structure */

            /* rebuild out_dir from dump root + stem */
            size_t rplen = strlen(rel_prefix);
            if (rplen > 0) {
                char fname_stem[4096];
                size_t fnlen = strlen(fd.cFileName);
                memcpy(fname_stem, fd.cFileName, fnlen + 1);
                if (fnlen > elen) fname_stem[fnlen - elen] = '\0';
                snprintf(out_dir, sizeof(out_dir), "%s\\%s", dump_dir, fname_stem);
            } else {
                size_t fnlen = strlen(fd.cFileName);
                char fname_stem[4096];
                memcpy(fname_stem, fd.cFileName, fnlen + 1);
                if (fnlen > elen) fname_stem[fnlen - elen] = '\0';
                snprintf(out_dir, sizeof(out_dir), "%s\\%s", dump_dir, fname_stem);
            }

            int n = process_file(full, out_dir);
            total_files++;
            total_images += n;
            if (n > 0) printf("  %s -> %d images\n", rel, n);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(src_dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char full[4096], rel[4096];
        snprintf(full, sizeof(full), "%s/%s", src_dir, ent->d_name);
        if (rel_prefix[0])
            snprintf(rel, sizeof(rel), "%s/%s", rel_prefix, ent->d_name);
        else
            snprintf(rel, sizeof(rel), "%s", ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            char sub_dump[4096];
            snprintf(sub_dump, sizeof(sub_dump), "%s/%s", dump_dir, ent->d_name);
            walk_dir(full, sub_dump, rel, ext);
        } else if (str_ends_with_ci(ent->d_name, ext)) {
            char fname_stem[4096];
            size_t fnlen = strlen(ent->d_name), elen = strlen(ext);
            memcpy(fname_stem, ent->d_name, fnlen + 1);
            if (fnlen > elen) fname_stem[fnlen - elen] = '\0';

            char out_dir[4096];
            snprintf(out_dir, sizeof(out_dir), "%s/%s", dump_dir, fname_stem);

            int n = process_file(full, out_dir);
            total_files++;
            total_images += n;
            if (n > 0) printf("  %s -> %d images\n", rel, n);
        }
    }
    closedir(d);
#endif
}

/* ── main ──────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rle_dump <folder> [.ext]\n");
        return 1;
    }

    const char *src_dir = argv[1];
    const char *ext = argc >= 3 ? argv[2] : ".rle";

    /* build dump dir: sibling of src_dir named X_dump */
    char dump_dir[4096];
    /* find last path separator */
    const char *base = src_dir;
    for (const char *p = src_dir; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    /* parent */
    size_t parent_len = (size_t)(base - src_dir);
    if (parent_len > 0) {
        memcpy(dump_dir, src_dir, parent_len);
        /* remove trailing separator */
        if (dump_dir[parent_len - 1] == '/' || dump_dir[parent_len - 1] == '\\')
            parent_len--;
        snprintf(dump_dir + parent_len, sizeof(dump_dir) - parent_len,
                 "%c%s_dump", PATH_SEP, base);
    } else {
        snprintf(dump_dir, sizeof(dump_dir), "%s_dump", base);
    }

    /* strip trailing separator from src_dir */
    char src_clean[4096];
    snprintf(src_clean, sizeof(src_clean), "%s", src_dir);
    size_t slen = strlen(src_clean);
    while (slen > 0 && (src_clean[slen-1] == '/' || src_clean[slen-1] == '\\'))
        src_clean[--slen] = '\0';

    clock_t t0 = clock();

    walk_dir(src_clean, dump_dir, "", ext);

    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("\nDone: %d files, %d images -> %s (%.1fs)\n",
           total_files, total_images, dump_dir, elapsed);

    return 0;
}
