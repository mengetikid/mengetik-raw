// mengetik.id Raw Edit - LibRaw WebAssembly wrapper
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "libraw/libraw.h"

extern "C" {
__attribute__((import_module("env"), import_name("js_throw"))) void js_throw(int code);
void *__cxa_allocate_exception(size_t s) { return malloc(s + 16); }
void __cxa_free_exception(void *p) { free(p); }
void __cxa_throw(void *obj, void *tinfo, void (*dest)(void *)) { int code = obj ? *(int *)obj : -1; js_throw(code); __builtin_unreachable(); }
}

static LibRaw *RP = 0;
static char *JS = 0; static size_t JSN = 0, JSL = 0;
static void jput(const char *s) { size_t n = strlen(s); if (JSL + n + 1 > JSN) { JSN = (JSL + n + 1) * 2 + 4096; JS = (char *)realloc(JS, JSN); } memcpy(JS + JSL, s, n + 1); JSL += n; }
static void jstr(const char *k, const char *v) {
  char b[16]; jput("\""); jput(k); jput("\":\"");
  for (const unsigned char *p = (const unsigned char *)v; *p; p++) {
    if (*p == '"' || *p == '\\') { b[0] = '\\'; b[1] = *p; b[2] = 0; jput(b); }
    else if (*p < 32) { snprintf(b, sizeof b, "\\u%04x", *p); jput(b); }
    else { b[0] = *p; b[1] = 0; jput(b); }
  }
  jput("\",");
}
static void jnum(const char *k, double v) { char b[64]; if (!isfinite(v)) v = 0; snprintf(b, sizeof b, "\"%s\":%.9g,", k, v); jput(b); }

/* ---- EXIF tambahan via callback ---- */
struct Ex { double bias, ev35, flash, meter, prog, wb, expmode, scene, fl35; int has[16]; char lens[128], lensmake[128], dto[32], offs[16], soft[64], copyright[128]; };
static Ex EX;
static unsigned rd2(LibRaw_abstract_datastream *s, unsigned ord) { unsigned char b[2] = {0, 0}; s->read(b, 1, 2); return ord == 0x4949 ? b[0] | b[1] << 8 : b[0] << 8 | b[1]; }
static unsigned rd4(LibRaw_abstract_datastream *s, unsigned ord) { unsigned char b[4] = {0, 0, 0, 0}; s->read(b, 1, 4); return ord == 0x4949 ? (b[0] | b[1] << 8 | b[2] << 16 | (unsigned)b[3] << 24) : ((unsigned)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]); }
static double rdv(LibRaw_abstract_datastream *s, int type, unsigned ord) {
  if (type == 3) return rd2(s, ord);
  if (type == 4) return rd4(s, ord);
  if (type == 8) return (short)rd2(s, ord);
  if (type == 9) return (int)rd4(s, ord);
  if (type == 5) { double n = rd4(s, ord), d = rd4(s, ord); return d ? n / d : 0; }
  if (type == 10) { double n = (int)rd4(s, ord), d = (int)rd4(s, ord); return d ? n / d : 0; }
  if (type == 1 || type == 7) { unsigned char c = 0; s->read(&c, 1, 1); return c; }
  return 0;
}
static void rds(LibRaw_abstract_datastream *s, int len, char *out, int max) { int n = len < max - 1 ? len : max - 1; if (n < 0) n = 0; s->read(out, 1, n); out[n] = 0; }
static void exif_cb(void *ctx, int tag, int type, int len, unsigned int ord, void *ifp, INT64 base) {
  LibRaw_abstract_datastream *s = (LibRaw_abstract_datastream *)ifp;
  INT64 pos = s->tell();
  int t = tag & 0xffff, ifd = (tag >> 16) & 0xf;
  if (ifd == 4) { s->seek(pos, SEEK_SET); return; } /* GPS ditangani LibRaw */
  switch (t) {
  case 0x9204: if (!EX.has[0]) { EX.bias = rdv(s, type, ord); EX.has[0] = 1; } break;
  case 0x9209: if (!EX.has[1]) { EX.flash = rdv(s, type, ord); EX.has[1] = 1; } break;
  case 0x9207: if (!EX.has[2]) { EX.meter = rdv(s, type, ord); EX.has[2] = 1; } break;
  case 0x8822: if (!EX.has[3]) { EX.prog = rdv(s, type, ord); EX.has[3] = 1; } break;
  case 0xa403: if (!EX.has[4]) { EX.wb = rdv(s, type, ord); EX.has[4] = 1; } break;
  case 0xa402: if (!EX.has[5]) { EX.expmode = rdv(s, type, ord); EX.has[5] = 1; } break;
  case 0xa406: if (!EX.has[6]) { EX.scene = rdv(s, type, ord); EX.has[6] = 1; } break;
  case 0xa405: if (!EX.has[7]) { EX.fl35 = rdv(s, type, ord); EX.has[7] = 1; } break;
  case 0xa434: if (!EX.lens[0] && type == 2) rds(s, len, EX.lens, sizeof EX.lens); break;
  case 0xa433: if (!EX.lensmake[0] && type == 2) rds(s, len, EX.lensmake, sizeof EX.lensmake); break;
  case 0x9003: if (!EX.dto[0] && type == 2) rds(s, len, EX.dto, sizeof EX.dto); break;
  case 0x9011: if (!EX.offs[0] && type == 2) rds(s, len, EX.offs, sizeof EX.offs); break;
  case 0x0131: if (!EX.soft[0] && type == 2) rds(s, len, EX.soft, sizeof EX.soft); break;
  case 0x8298: if (!EX.copyright[0] && type == 2) rds(s, len, EX.copyright, sizeof EX.copyright); break;
  }
  s->seek(pos, SEEK_SET);
}

extern "C" {
__attribute__((export_name("rw_malloc"))) void *rw_malloc(int n) { return malloc(n); }
__attribute__((export_name("rw_free"))) void rw_free(void *p) { free(p); }
__attribute__((export_name("rw_version"))) const char *rw_version() { return LibRaw::version(); }

__attribute__((export_name("rw_open"))) int rw_open(void *buf, int size) {
  if (RP) { RP->recycle(); delete RP; RP = 0; }
  memset(&EX, 0, sizeof EX);
  RP = new LibRaw(LIBRAW_OPTIONS_NONE);
  RP->set_exifparser_handler(exif_cb, 0);
  libraw_output_params_t &p = RP->imgdata.params;
  p.output_bps = 16; p.gamm[0] = 1.0; p.gamm[1] = 1.0; p.no_auto_bright = 1; p.output_color = 8; /* Rec.2020 linear */
  p.use_camera_wb = 0; p.use_auto_wb = 0; p.user_qual = 3; p.highlight = 0; p.use_camera_matrix = 1;
  return RP->open_buffer(buf, size);
}
/* kunci: 1 half_size 2 user_qual 3 highlight 4 use_camera_wb 5 output_color 6 fbdd 7 med_passes 8 use_auto_wb 9 four_color 10 user_flip 11 adjust_maximum_thr */
__attribute__((export_name("rw_set"))) void rw_set(int key, double v) {
  if (!RP) return; libraw_output_params_t &p = RP->imgdata.params;
  switch (key) {
  case 1: p.half_size = (int)v; break; case 2: p.user_qual = (int)v; break; case 3: p.highlight = (int)v; break;
  case 4: p.use_camera_wb = (int)v; break; case 5: p.output_color = (int)v; break; case 6: p.fbdd_noiserd = (int)v; break;
  case 7: p.med_passes = (int)v; break; case 8: p.use_auto_wb = (int)v; break; case 9: p.four_color_rgb = (int)v; break;
  case 10: p.user_flip = (int)v; break; case 11: p.adjust_maximum_thr = (float)v; break;
  }
}
__attribute__((export_name("rw_unpack"))) int rw_unpack() { return RP ? RP->unpack() : -1; }
__attribute__((export_name("rw_process"))) int rw_process() { return RP ? RP->dcraw_process() : -1; }
/* data gambar hasil proses: 4 kanal ushort per piksel (linear) */
__attribute__((export_name("rw_image"))) void *rw_image() { return RP ? (void *)RP->imgdata.image : 0; }
__attribute__((export_name("rw_iw"))) int rw_iw() { return RP ? RP->imgdata.sizes.iwidth : 0; }
__attribute__((export_name("rw_ih"))) int rw_ih() { return RP ? RP->imgdata.sizes.iheight : 0; }
__attribute__((export_name("rw_flip"))) int rw_flip() { return RP ? RP->imgdata.sizes.flip : 0; }
__attribute__((export_name("rw_free_image"))) void rw_free_image() { if (RP && RP->imgdata.image) { free(RP->imgdata.image); RP->imgdata.image = 0; } }
__attribute__((export_name("rw_close"))) void rw_close() { if (RP) { RP->recycle(); delete RP; RP = 0; } }

/* thumbnail tertanam (JPEG terbesar) */
static libraw_processed_image_t *TH = 0;
__attribute__((export_name("rw_thumb"))) void *rw_thumb() {
  if (!RP) return 0;
  if (TH) { LibRaw::dcraw_clear_mem(TH); TH = 0; }
  int best = -1, bw = 0;
  for (int i = 0; i < RP->imgdata.thumbs_list.thumbcount; i++) { libraw_thumbnail_item_t &t = RP->imgdata.thumbs_list.thumblist[i]; if (t.tformat == LIBRAW_INTERNAL_THUMBNAIL_JPEG && t.twidth * t.theight >= bw) { bw = t.twidth * t.theight; best = i; } }
  int r = best >= 0 ? RP->unpack_thumb_ex(best) : RP->unpack_thumb();
  if (r != LIBRAW_SUCCESS) return 0;
  int err = 0; TH = RP->dcraw_make_mem_thumb(&err);
  return TH;
}

__attribute__((export_name("rw_meta"))) const char *rw_meta() {
  JSL = 0; jput("{");
  if (!RP) { jput("}"); return JS; }
  libraw_data_t &d = RP->imgdata;
  jstr("make", d.idata.make); jstr("model", d.idata.model); jstr("normMake", d.idata.normalized_make); jstr("normModel", d.idata.normalized_model);
  jnum("rawCount", d.idata.raw_count); jnum("dngVersion", d.idata.dng_version); jnum("colors", d.idata.colors); jnum("filters", d.idata.filters);
  jnum("width", d.sizes.width); jnum("height", d.sizes.height); jnum("rawWidth", d.sizes.raw_width); jnum("rawHeight", d.sizes.raw_height); jnum("flip", d.sizes.flip); jnum("pixelAspect", d.sizes.pixel_aspect);
  jnum("iso", d.other.iso_speed); jnum("shutter", d.other.shutter); jnum("aperture", d.other.aperture); jnum("focal", d.other.focal_len); jnum("timestamp", (double)d.other.timestamp); jnum("shotOrder", d.other.shot_order);
  jstr("desc", d.other.desc); jstr("artist", d.other.artist);
  if (d.other.parsed_gps.gpsparsed) {
    libraw_gps_info_t &g = d.other.parsed_gps;
    double la = g.latitude[0] + g.latitude[1] / 60 + g.latitude[2] / 3600, lo = g.longitude[0] + g.longitude[1] / 60 + g.longitude[2] / 3600;
    if (g.latref == 'S') la = -la; if (g.longref == 'W') lo = -lo;
    jnum("gpsLat", la); jnum("gpsLon", lo); jnum("gpsAlt", g.altitude);
  }
  jstr("lens", d.lens.makernotes.Lens[0] ? d.lens.makernotes.Lens : (d.lens.Lens[0] ? d.lens.Lens : EX.lens));
  jstr("lensMake", d.lens.LensMake[0] ? d.lens.LensMake : EX.lensmake);
  jnum("focal35", d.lens.FocalLengthIn35mmFormat ? d.lens.FocalLengthIn35mmFormat : (EX.has[7] ? EX.fl35 : d.lens.makernotes.FocalLengthIn35mmFormat));
  jnum("minFocal", d.lens.MinFocal); jnum("maxFocal", d.lens.MaxFocal); jnum("maxAp", d.lens.EXIF_MaxAp);
  jstr("body", d.shootinginfo.BodySerial);
  if (EX.has[0]) jnum("expBias", EX.bias);
  if (EX.has[1]) jnum("flash", EX.flash);
  if (EX.has[2]) jnum("metering", EX.meter);
  if (EX.has[3]) jnum("program", EX.prog);
  if (EX.has[4]) jnum("wbMode", EX.wb);
  if (EX.has[5]) jnum("expMode", EX.expmode);
  if (EX.has[6]) jnum("scene", EX.scene);
  jstr("dateOriginal", EX.dto); jstr("offsetTime", EX.offs); jstr("software", EX.soft); jstr("copyright", EX.copyright);
  jnum("rawBps", d.color.raw_bps); jnum("black", d.color.black); jnum("maximum", d.color.maximum);
  char b[1024]; libraw_colordata_t &c = d.color;
  snprintf(b, sizeof b, "\"camMul\":[%g,%g,%g,%g],\"preMul\":[%g,%g,%g,%g],\"rgbCam\":[[%g,%g,%g,%g],[%g,%g,%g,%g],[%g,%g,%g,%g]],",
           c.cam_mul[0], c.cam_mul[1], c.cam_mul[2], c.cam_mul[3], c.pre_mul[0], c.pre_mul[1], c.pre_mul[2], c.pre_mul[3],
           c.rgb_cam[0][0], c.rgb_cam[0][1], c.rgb_cam[0][2], c.rgb_cam[0][3], c.rgb_cam[1][0], c.rgb_cam[1][1], c.rgb_cam[1][2], c.rgb_cam[1][3], c.rgb_cam[2][0], c.rgb_cam[2][1], c.rgb_cam[2][2], c.rgb_cam[2][3]);
  jput(b);
  jnum("thumbW", d.thumbnail.twidth); jnum("thumbH", d.thumbnail.theight); jnum("thumbCount", d.thumbs_list.thumbcount);
  jnum("ok", 1);
  if (JSL && JS[JSL - 1] == ',') JS[--JSL] = 0;
  jput("}");
  return JS;
}
}
