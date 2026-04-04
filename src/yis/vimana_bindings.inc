#include <vimana.h>

#include <stdio.h>
#include <stdlib.h>

typedef enum VimanaHandleKind {
  VIMANA_HANDLE_SYSTEM = 1,
  VIMANA_HANDLE_SCREEN,
  VIMANA_HANDLE_DEVICE,
  VIMANA_HANDLE_DATETIME,
  VIMANA_HANDLE_FILE,
} VimanaHandleKind;

typedef struct VimanaHandle {
  YisObj base;
  void *ptr;
  int kind;
} VimanaHandle;

typedef struct VimanaFrameCtx {
  YisVal frame;
} VimanaFrameCtx;

static void vimana_handle_drop(YisObj *obj) {
  VimanaHandle *handle = (VimanaHandle *)obj;
  if (!handle)
    return;
  handle->ptr = NULL;
}

static VimanaHandle *vimana_handle_new(void *ptr, int kind) {
  VimanaHandle *handle =
      (VimanaHandle *)yis_obj_new(sizeof(VimanaHandle), vimana_handle_drop);
  handle->ptr = ptr;
  handle->kind = kind;
  return handle;
}

static YisVal vimana_wrap(void *ptr, int kind) {
  if (!ptr)
    return YV_NULLV;
  return YV_OBJ(vimana_handle_new(ptr, kind));
}

static VimanaHandle *vimana_handle_from_val(YisVal value, const char *what) {
  if (value.tag != EVT_OBJ)
    yis_trap(what);
  return (VimanaHandle *)value.as.p;
}

static vimana_system *vimana_system_from_val(YisVal value, const char *what) {
  return (vimana_system *)vimana_handle_from_val(value, what)->ptr;
}

static vimana_screen *vimana_screen_from_val(YisVal value, const char *what) {
  return (vimana_screen *)vimana_handle_from_val(value, what)->ptr;
}

static const char *vimana_required_cstr(YisVal value, YisStr **tmp) {
  if (value.tag == EVT_NULL)
    return "";
  if (value.tag == EVT_STR)
    return ((YisStr *)value.as.p)->data;
  YisStr *coerced = stdr_to_string(value);
  if (tmp)
    *tmp = coerced;
  return coerced ? coerced->data : "";
}

static bool vimana_is_numeric_tag(YisTag tag);

static const char *vimana_tag_name(YisTag tag) {
  switch (tag) {
  case EVT_NULL:
    return "null";
  case EVT_INT:
    return "int";
  case EVT_FLOAT:
    return "float";
  case EVT_BOOL:
    return "bool";
  case EVT_STR:
    return "str";
  case EVT_ARR:
    return "arr";
  case EVT_DICT:
    return "dict";
  case EVT_OBJ:
    return "obj";
  case EVT_FN:
    return "fn";
  default:
    return "?";
  }
}

static void vimana_debug_dump_icn_value(YisVal value, const char *what) {
  fprintf(stderr, "vimana.put_icn debug: %s\n", what ? what : "bad icn");
  fprintf(stderr, "  value tag=%s\n", vimana_tag_name(value.tag));
  if (value.tag == EVT_ARR) {
    YisArr *arr = (YisArr *)value.as.p;
    size_t len = arr ? arr->len : 0;
    fprintf(stderr, "  array len=%zu\n", len);
    size_t limit = len < 8 ? len : 8;
    for (size_t i = 0; i < limit; i++) {
      YisVal item = arr->items[i];
      fprintf(stderr, "  item[%zu] tag=%s", i, vimana_tag_name(item.tag));
      if (vimana_is_numeric_tag(item.tag)) {
        fprintf(stderr, " value=%lld", (long long)yis_as_int(item));
      } else if (item.tag == EVT_STR) {
        YisStr *text = (YisStr *)item.as.p;
        fprintf(stderr, " len=%zu text='%.*s'", text ? text->len : 0,
                text ? (int)text->len : 0, text ? text->data : "");
      }
      fputc('\n', stderr);
    }
  } else if (value.tag == EVT_STR) {
    YisStr *text = (YisStr *)value.as.p;
    fprintf(stderr, "  string len=%zu text='%.*s'\n", text ? text->len : 0,
            text ? (int)text->len : 0, text ? text->data : "");
  }
}

static uint16_t vimana_icn_short_from_val(YisVal value) {
  if (value.tag == EVT_INT)
    return (uint16_t)yis_as_int(value);
  YisStr *tmp = NULL;
  const char *text = vimana_required_cstr(value, &tmp);
  unsigned long out = text && text[0] ? strtoul(text, NULL, 16) : 0;
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return (uint16_t)(out & 0xFFFFu);
}

static int vimana_hex_digit_value(char ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'a' && ch <= 'f')
    return 10 + (ch - 'a');
  if (ch >= 'A' && ch <= 'F')
    return 10 + (ch - 'A');
  return -1;
}

static uint16_t vimana_parse_hex_word(const char *text, const char *what) {
  if (!text)
    yis_trap(what);
  uint16_t out = 0;
  for (int i = 0; i < 4; i++) {
    int digit = vimana_hex_digit_value(text[i]);
    if (digit < 0)
      yis_trap(what);
    out = (uint16_t)((out << 4) | (uint16_t)digit);
  }
  return out;
}

static bool vimana_is_numeric_tag(YisTag tag) {
  return tag == EVT_INT || tag == EVT_FLOAT || tag == EVT_BOOL ||
         tag == EVT_NULL;
}

static uint8_t vimana_byte_from_val(YisVal value) {
  int out = 0;
  if (value.tag != EVT_NULL)
    out = (int)yis_as_int(value);
  if (out < 0)
    out = 0;
  if (out > 255)
    out = 255;
  return (uint8_t)out;
}

static void vimana_icn_rows_from_val(YisVal value, uint16_t rows[8],
                                     const char *what) {
  for (int i = 0; i < 8; i++)
    rows[i] = 0;
  if (value.tag != EVT_ARR)
    yis_trap(what);
  YisArr *arr = (YisArr *)value.as.p;
  if (!arr)
    return;
  size_t limit = arr->len < 8 ? arr->len : 8;
  for (size_t i = 0; i < limit; i++)
    rows[i] = vimana_icn_short_from_val(arr->items[i]);
}

static void vimana_uf2_rows_from_val(YisVal value, uint16_t rows[16],
                                      const char *what) {
  for (int i = 0; i < 16; i++)
    rows[i] = 0;
  if (value.tag != EVT_ARR)
    yis_trap(what);
  YisArr *arr = (YisArr *)value.as.p;
  if (!arr)
    return;
  size_t limit = arr->len < 16 ? arr->len : 16;
  for (size_t i = 0; i < limit; i++)
    rows[i] = vimana_icn_short_from_val(arr->items[i]);
}

static void vimana_icn_byte_rows_from_val(YisVal value, uint8_t rows[8],
                                          const char *what) {
  uint16_t word_rows[8] = {0};
  vimana_icn_rows_from_val(value, word_rows, what);
  for (int i = 0; i < 8; i++)
    rows[i] = (uint8_t)(word_rows[i] & 0xFFu);
}

static void vimana_icn_words_from_val(YisVal value, uint16_t words[4],
                                      const char *what) {
  for (int i = 0; i < 4; i++)
    words[i] = 0;

  if (value.tag == EVT_STR) {
    YisStr *text = (YisStr *)value.as.p;
    if (!text || text->len != 16) {
      vimana_debug_dump_icn_value(value, what);
      yis_trap(what);
    }
    for (size_t i = 0; i < 4; i++)
      words[i] = vimana_parse_hex_word(text->data + (i * 4), what);
    return;
  }

  if (value.tag != EVT_ARR) {
    vimana_debug_dump_icn_value(value, what);
    yis_trap(what);
  }
  YisArr *arr = (YisArr *)value.as.p;
  if (!arr || arr->len < 4) {
    vimana_debug_dump_icn_value(value, what);
    yis_trap(what);
  }
  for (size_t i = 0; i < 4; i++) {
    if (vimana_is_numeric_tag(arr->items[i].tag)) {
      words[i] = (uint16_t)(yis_as_int(arr->items[i]) & 0xFFFFu);
      continue;
    }
    YisStr *tmp = NULL;
    const char *text = vimana_required_cstr(arr->items[i], &tmp);
    if (!text || strlen(text) != 4) {
      if (tmp)
        yis_release_val(YV_STR(tmp));
      vimana_debug_dump_icn_value(value, what);
      yis_trap(what);
    }
    words[i] = vimana_parse_hex_word(text, what);
    if (tmp)
      yis_release_val(YV_STR(tmp));
  }
}

static void vimana_icn_bytes_from_val(YisVal value, uint8_t rows[8],
                                      const char *what) {
  if (value.tag == EVT_ARR) {
    YisArr *arr = (YisArr *)value.as.p;
    if (arr && arr->len >= 8) {
      vimana_icn_byte_rows_from_val(value, rows, what);
      return;
    }
  }

  uint16_t words[4] = {0};
  vimana_icn_words_from_val(value, words, what);
  for (int i = 0; i < 4; i++) {
    rows[i * 2] = (uint8_t)((words[i] >> 8) & 0xFFu);
    rows[i * 2 + 1] = (uint8_t)(words[i] & 0xFFu);
  }
}

static void vimana_sprite_bytes_from_val(YisVal value, int mode,
                                         uint8_t sprite[16],
                                         const char *what) {
  memset(sprite, 0, 16);
  if (mode != 0) {
    uint16_t rows[8] = {0};
    vimana_icn_rows_from_val(value, rows, what);
    for (int row = 0; row < 8; row++) {
      uint8_t plane0 = 0;
      uint8_t plane1 = 0;
      uint16_t packed = rows[row];
      for (int col = 0; col < 8; col++) {
        uint16_t pair = (uint16_t)((packed >> ((7 - col) * 2)) & 0x3u);
        uint8_t mask = (uint8_t)(0x80u >> col);
        if ((pair & 0x1u) != 0)
          plane0 = (uint8_t)(plane0 | mask);
        if ((pair & 0x2u) != 0)
          plane1 = (uint8_t)(plane1 | mask);
      }
      sprite[row] = plane0;
      sprite[row + 8] = plane1;
    }
    return;
  }
  vimana_icn_bytes_from_val(value, sprite,
                            what ? what : "vimana sprite expects 1bpp or 2bpp rows");
}

static void vimana_frame_call(vimana_system *system, vimana_screen *screen,
                              void *user) {
  (void)system;
  (void)screen;
  VimanaFrameCtx *ctx = (VimanaFrameCtx *)user;
  if (!ctx)
    return;
  YisVal ret = yis_call(ctx->frame, 0, NULL);
  yis_release_val(ret);
}

static YisVal __vimana_system(void) {
  return vimana_wrap(vimana_system_new(), VIMANA_HANDLE_SYSTEM);
}

static void __vimana_system_run(YisVal sysv, YisVal scrv, YisVal framev) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.run expects system");
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.system.run expects screen");
  if (framev.tag != EVT_FN)
    yis_trap("vimana.system.run expects function");
  VimanaFrameCtx ctx = {.frame = framev};
  yis_retain_val(framev);
  vimana_system_run(system, screen, vimana_frame_call, &ctx);
  yis_release_val(framev);
}

static void __vimana_system_quit(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.quit expects system");
  vimana_system_quit(system);
}

static YisVal __vimana_system_device(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.device expects system");
  return vimana_wrap(system, VIMANA_HANDLE_DEVICE);
}

static YisVal __vimana_system_datetime(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.datetime expects system");
  return vimana_wrap(system, VIMANA_HANDLE_DATETIME);
}

static YisVal __vimana_system_file(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.file expects system");
  return vimana_wrap(system, VIMANA_HANDLE_FILE);
}

static YisVal __vimana_system_ticks(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.ticks expects system");
  return YV_INT(vimana_system_ticks(system));
}

static void __vimana_system_sleep(YisVal sysv, YisVal msv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.sleep expects system");
  vimana_system_sleep(system, yis_as_int(msv));
}

static YisVal __vimana_system_clipboard_text(YisVal sysv) {
  vimana_system *system = vimana_system_from_val(
      sysv, "vimana.system.clipboard_text expects system");
  char *text = vimana_system_clipboard_text(system);
  if (!text)
    return YV_STR(stdr_str_lit(""));
  YisVal out = YV_STR(stdr_str_lit(text));
  free(text);
  return out;
}

static YisVal __vimana_system_set_clipboard_text(YisVal sysv, YisVal textv) {
  vimana_system *system = vimana_system_from_val(
      sysv, "vimana.system.set_clipboard_text expects system");
  YisStr *tmp = NULL;
  const char *text = vimana_required_cstr(textv, &tmp);
  bool ok = vimana_system_set_clipboard_text(system, text);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_system_home_dir(YisVal sysv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.home_dir expects system");
  char *text = vimana_system_home_dir(system);
  if (!text)
    return YV_STR(stdr_str_lit("/tmp"));
  YisVal out = YV_STR(stdr_str_lit(text));
  free(text);
  return out;
}

static YisVal __vimana_screen(YisVal titlev, YisVal widthv, YisVal heightv,
                              YisVal scalev) {
  YisStr *tmp = NULL;
  const char *title = vimana_required_cstr(titlev, &tmp);
  vimana_screen *screen = vimana_screen_new(title, (int)yis_as_int(widthv),
                                            (int)yis_as_int(heightv),
                                            (int)yis_as_int(scalev));
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return vimana_wrap(screen, VIMANA_HANDLE_SCREEN);
}

static void __vimana_screen_clear(YisVal scrv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.clear expects screen");
  vimana_screen_clear(screen, (int)yis_as_int(bgv));
}

static void __vimana_screen_resize(YisVal scrv, YisVal widthv, YisVal heightv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.resize expects screen");
  vimana_screen_resize(screen, (int)yis_as_int(widthv), (int)yis_as_int(heightv));
}

static void __vimana_screen_set_palette(YisVal scrv, YisVal slotv,
                                        YisVal colorv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_palette expects screen");
  YisStr *tmp = NULL;
  const char *color = vimana_required_cstr(colorv, &tmp);
  vimana_screen_set_palette(screen, (int)yis_as_int(slotv), color);
  if (tmp)
    yis_release_val(YV_STR(tmp));
}

static void __vimana_screen_set_font_glyph(YisVal scrv, YisVal codev,
                                           YisVal icnv) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_font_glyph expects screen");
  uint16_t rows[16] = {0};
  vimana_uf2_rows_from_val(
      icnv, rows,
      "vimana.screen.set_font_glyph expects 16 hex-short rows (UF2)");
  vimana_screen_set_font_glyph(screen, (int)yis_as_int(codev), rows);
}

static void __vimana_screen_set_font_chr(YisVal scrv, YisVal codev,
                                         YisVal chrv) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_font_chr expects screen");
  uint8_t bytes[16] = {0};
  if (chrv.tag != EVT_ARR)
    yis_trap("vimana.screen.set_font_chr expects byte array");
  YisArr *arr = (YisArr *)chrv.as.p;
  int len = 0;
  if (arr) {
    len = (int)(arr->len < 16 ? arr->len : 16);
    for (int i = 0; i < len; i++)
      bytes[i] = vimana_byte_from_val(arr->items[i]);
  }
  vimana_screen_set_font_chr(screen, (int)yis_as_int(codev), bytes, len);
}

static void __vimana_screen_set_font_width(YisVal scrv, YisVal codev,
                                            YisVal widthv) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_font_width expects screen");
  vimana_screen_set_font_width(screen, (int)yis_as_int(codev),
                               (int)yis_as_int(widthv));
}

static void __vimana_screen_set_font_size(YisVal scrv, YisVal sizev) {
  vimana_screen *screen = vimana_screen_from_val(
      scrv, "vimana.screen.set_font_size expects screen");
  vimana_screen_set_font_size(screen, (int)yis_as_int(sizev));
}

static void __vimana_screen_set_sprite(YisVal scrv, YisVal addrv, YisVal spritev,
                                       YisVal modev) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_sprite expects screen");
  int mode = (int)yis_as_int(modev);
  uint8_t sprite[16] = {0};
  vimana_sprite_bytes_from_val(spritev, mode, sprite,
                               "vimana.screen.set_sprite expects 8 byte rows or 8 hex-short 2bpp rows");
  vimana_screen_set_sprite(screen, (int)yis_as_int(addrv), sprite, mode);
}

static void __vimana_screen_set_x(YisVal scrv, YisVal xv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_x expects screen");
  vimana_screen_set_x(screen, (int)yis_as_int(xv));
}

static void __vimana_screen_set_y(YisVal scrv, YisVal yv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_y expects screen");
  vimana_screen_set_y(screen, (int)yis_as_int(yv));
}

static void __vimana_screen_set_addr(YisVal scrv, YisVal addrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_addr expects screen");
  vimana_screen_set_addr(screen, (int)yis_as_int(addrv));
}

static void __vimana_screen_set_auto(YisVal scrv, YisVal autov) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_auto expects screen");
  vimana_screen_set_auto(screen, (int)yis_as_int(autov));
}

static void __vimana_screen_put(YisVal scrv, YisVal xv, YisVal yv,
                                YisVal glyphv, YisVal fgv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.put expects screen");
  YisStr *tmp = NULL;
  const char *glyph = vimana_required_cstr(glyphv, &tmp);
  vimana_screen_put(screen, (int)yis_as_int(xv), (int)yis_as_int(yv), glyph,
                    (int)yis_as_int(fgv), (int)yis_as_int(bgv));
  if (tmp)
    yis_release_val(YV_STR(tmp));
}

static void __vimana_screen_put_icn(YisVal scrv, YisVal xv, YisVal yv,
                                    YisVal icnv, YisVal fgv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.put_icn expects screen");
  uint8_t rows[8] = {0};
  vimana_icn_bytes_from_val(
      icnv, rows,
    "vimana.screen.put_icn expects 4 hex words or 8 byte rows");
  vimana_screen_put_icn(screen, (int)yis_as_int(xv), (int)yis_as_int(yv), rows,
                        (int)yis_as_int(fgv), (int)yis_as_int(bgv));
}

static void __vimana_screen_put_text(YisVal scrv, YisVal xv, YisVal yv,
                                     YisVal textv, YisVal fgv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.put_text expects screen");
  YisStr *tmp = NULL;
  const char *text = vimana_required_cstr(textv, &tmp);
  vimana_screen_put_text(screen, (int)yis_as_int(xv), (int)yis_as_int(yv),
                         text, (int)yis_as_int(fgv), (int)yis_as_int(bgv));
  if (tmp)
    yis_release_val(YV_STR(tmp));
}

static void __vimana_screen_sprite(YisVal scrv, YisVal ctrlv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.sprite expects screen");
  vimana_screen_sprite(screen, (int)yis_as_int(ctrlv));
}

static void __vimana_screen_pixel(YisVal scrv, YisVal ctrlv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.pixel expects screen");
  vimana_screen_pixel(screen, (int)yis_as_int(ctrlv));
}

static void __vimana_screen_present(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.present expects screen");
  vimana_screen_present(screen);
}

static void __vimana_screen_draw_titlebar(YisVal scrv, YisVal bgv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.draw_titlebar expects screen");
  vimana_screen_draw_titlebar(screen, (int)yis_as_int(bgv));
}

static void __vimana_screen_set_titlebar_title(YisVal scrv, YisVal titlev) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_titlebar_title expects screen");
  YisStr *tmp = NULL;
  const char *title = vimana_required_cstr(titlev, &tmp);
  vimana_screen_set_titlebar_title(screen, title);
  if (tmp)
    yis_release_val(YV_STR(tmp));
}

static void __vimana_screen_set_titlebar_button(YisVal scrv, YisVal showv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.set_titlebar_button expects screen");
  vimana_screen_set_titlebar_button(screen, yis_as_int(showv) != 0);
}

static YisVal __vimana_screen_titlebar_button_pressed(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.titlebar_button_pressed expects screen");
  return YV_BOOL(vimana_screen_titlebar_button_pressed(screen));
}

static YisVal __vimana_screen_x(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.x expects screen");
  return YV_INT(vimana_screen_x(screen));
}

static YisVal __vimana_screen_y(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.y expects screen");
  return YV_INT(vimana_screen_y(screen));
}

static YisVal __vimana_screen_addr(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.addr expects screen");
  return YV_INT(vimana_screen_addr(screen));
}

static YisVal __vimana_screen_auto(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.auto expects screen");
  return YV_INT(vimana_screen_auto(screen));
}

static YisVal __vimana_screen_width(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.width expects screen");
  return YV_INT(vimana_screen_width(screen));
}

static YisVal __vimana_screen_height(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.height expects screen");
  return YV_INT(vimana_screen_height(screen));
}

static YisVal __vimana_screen_scale(YisVal scrv) {
  vimana_screen *screen =
      vimana_screen_from_val(scrv, "vimana.screen.scale expects screen");
  return YV_INT(vimana_screen_scale(screen));
}

static void __vimana_device_poll(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.poll expects device");
  vimana_device_poll(system);
}

static YisVal __vimana_device_key_down(YisVal devv, YisVal scancodev) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.key_down expects device");
  return YV_BOOL(vimana_device_key_down(system, (int)yis_as_int(scancodev)));
}

static YisVal __vimana_device_key_pressed(YisVal devv, YisVal scancodev) {
  vimana_system *system = vimana_system_from_val(
      devv, "vimana.device.key_pressed expects device");
  return YV_BOOL(
      vimana_device_key_pressed(system, (int)yis_as_int(scancodev)));
}

static YisVal __vimana_device_mouse_down(YisVal devv, YisVal buttonv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.mouse_down expects device");
  return YV_BOOL(vimana_device_mouse_down(system, (int)yis_as_int(buttonv)));
}

static YisVal __vimana_device_mouse_pressed(YisVal devv, YisVal buttonv) {
  vimana_system *system = vimana_system_from_val(
      devv, "vimana.device.mouse_pressed expects device");
  return YV_BOOL(
      vimana_device_mouse_pressed(system, (int)yis_as_int(buttonv)));
}

static YisVal __vimana_device_pointer_x(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.pointer_x expects device");
  return YV_INT(vimana_device_pointer_x(system));
}

static YisVal __vimana_device_pointer_y(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.pointer_y expects device");
  return YV_INT(vimana_device_pointer_y(system));
}

static YisVal __vimana_device_tile_x(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.tile_x expects device");
  return YV_INT(vimana_device_tile_x(system));
}

static YisVal __vimana_device_tile_y(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.tile_y expects device");
  return YV_INT(vimana_device_tile_y(system));
}

static YisVal __vimana_device_wheel_x(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.wheel_x expects device");
  return YV_INT(vimana_device_wheel_x(system));
}

static YisVal __vimana_device_wheel_y(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.wheel_y expects device");
  return YV_INT(vimana_device_wheel_y(system));
}

static YisVal __vimana_device_text_input(YisVal devv) {
  vimana_system *system =
      vimana_system_from_val(devv, "vimana.device.text_input expects device");
  const char *text = vimana_device_text_input(system);
  return YV_STR(stdr_str_lit(text ? text : ""));
}

static YisVal __vimana_datetime_now(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.now expects datetime");
  return YV_INT(vimana_datetime_now(system));
}

static YisVal __vimana_datetime_year(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.year expects datetime");
  return YV_INT(vimana_datetime_year(system));
}

static YisVal __vimana_datetime_month(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.month expects datetime");
  return YV_INT(vimana_datetime_month(system));
}

static YisVal __vimana_datetime_day(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.day expects datetime");
  return YV_INT(vimana_datetime_day(system));
}

static YisVal __vimana_datetime_hour(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.hour expects datetime");
  return YV_INT(vimana_datetime_hour(system));
}

static YisVal __vimana_datetime_minute(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.minute expects datetime");
  return YV_INT(vimana_datetime_minute(system));
}

static YisVal __vimana_datetime_second(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.second expects datetime");
  return YV_INT(vimana_datetime_second(system));
}

static YisVal __vimana_datetime_weekday(YisVal datetimev) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.weekday expects datetime");
  return YV_INT(vimana_datetime_weekday(system));
}

static YisVal __vimana_datetime_year_at(YisVal datetimev, YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.year_at expects datetime");
  return YV_INT(vimana_datetime_year_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_month_at(YisVal datetimev,
                                         YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.month_at expects datetime");
  return YV_INT(vimana_datetime_month_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_day_at(YisVal datetimev, YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.day_at expects datetime");
  return YV_INT(vimana_datetime_day_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_hour_at(YisVal datetimev, YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.hour_at expects datetime");
  return YV_INT(vimana_datetime_hour_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_minute_at(YisVal datetimev,
                                          YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.minute_at expects datetime");
  return YV_INT(vimana_datetime_minute_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_second_at(YisVal datetimev,
                                          YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.second_at expects datetime");
  return YV_INT(vimana_datetime_second_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_datetime_weekday_at(YisVal datetimev,
                                           YisVal timestampv) {
  vimana_system *system = vimana_system_from_val(
      datetimev, "vimana.datetime.weekday_at expects datetime");
  return YV_INT(vimana_datetime_weekday_at(system, yis_as_int(timestampv)));
}

static YisVal __vimana_file_read_text(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.read_text expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  char *text = vimana_file_read_text(system, path);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  if (!text)
    return YV_STR(stdr_str_lit(""));
  YisVal out = YV_STR(stdr_str_lit(text));
  free(text);
  return out;
}

static YisVal __vimana_file_read_bytes(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.read_bytes expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  size_t size = 0;
  unsigned char *bytes = vimana_file_read_bytes(system, path, &size);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  YisArr *arr = stdr_arr_new((int)size);
  if (arr)
    arr->len = size;
  for (size_t i = 0; i < size; i++)
    yis_arr_set(arr, i, YV_INT(bytes[i]));
  free(bytes);
  return YV_ARR(arr);
}

static YisVal __vimana_file_write_text(YisVal filev, YisVal pathv,
                                       YisVal textv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.write_text expects file");
  YisStr *path_tmp = NULL;
  YisStr *text_tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &path_tmp);
  const char *text = vimana_required_cstr(textv, &text_tmp);
  bool ok = vimana_file_write_text(system, path, text);
  if (path_tmp)
    yis_release_val(YV_STR(path_tmp));
  if (text_tmp)
    yis_release_val(YV_STR(text_tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_write_bytes(YisVal filev, YisVal pathv,
                                        YisVal bytesv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.write_bytes expects file");
  if (bytesv.tag != EVT_ARR)
    yis_trap("vimana.file.write_bytes expects byte array");
  YisStr *path_tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &path_tmp);
  YisArr *arr = (YisArr *)bytesv.as.p;
  size_t len = arr ? arr->len : 0;
  unsigned char *bytes = len > 0 ? (unsigned char *)malloc(len) : NULL;
  if (len > 0 && !bytes) {
    if (path_tmp)
      yis_release_val(YV_STR(path_tmp));
    return YV_BOOL(false);
  }
  for (size_t i = 0; i < len; i++)
    bytes[i] = vimana_byte_from_val(arr->items[i]);
  bool ok = vimana_file_write_bytes(system, path, bytes, len);
  free(bytes);
  if (path_tmp)
    yis_release_val(YV_STR(path_tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_exists(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.exists expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  bool ok = vimana_file_exists(system, path);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_remove(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.remove expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  bool ok = vimana_file_remove(system, path);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_rename(YisVal filev, YisVal pathv,
                                   YisVal new_pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.rename expects file");
  YisStr *path_tmp = NULL;
  YisStr *new_tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &path_tmp);
  const char *new_path = vimana_required_cstr(new_pathv, &new_tmp);
  bool ok = vimana_file_rename(system, path, new_path);
  if (path_tmp)
    yis_release_val(YV_STR(path_tmp));
  if (new_tmp)
    yis_release_val(YV_STR(new_tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_file_list(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.list expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  int count = 0;
  char **items = vimana_file_list(system, path, &count);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  YisArr *arr = stdr_arr_new((int)(count > 0 ? count : 0));
  if (arr)
    arr->len = (size_t)(count > 0 ? count : 0);
  for (int i = 0; i < count; i++) {
    yis_arr_set(arr, (size_t)i,
                YV_STR(stdr_str_lit(items[i] ? items[i] : "")));
  }
  vimana_file_list_free(items, count);
  return YV_ARR(arr);
}

static YisVal __vimana_file_is_dir(YisVal filev, YisVal pathv) {
  vimana_system *system =
      vimana_system_from_val(filev, "vimana.file.is_dir expects file");
  YisStr *tmp = NULL;
  const char *path = vimana_required_cstr(pathv, &tmp);
  bool ok = vimana_file_is_dir(system, path);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

/* ── Subprocess IPC bindings ──────────────────────────────────────────── */

static YisVal __vimana_system_spawn(YisVal sysv, YisVal cmdv) {
  vimana_system *system =
      vimana_system_from_val(sysv, "vimana.system.spawn expects system");
  YisStr *tmp = NULL;
  const char *cmd = vimana_required_cstr(cmdv, &tmp);
  vimana_process *proc = vimana_process_spawn(system, cmd);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  if (!proc)
    return YV_INT(0);
  /* store proc pointer as a number (opaque handle) */
  return YV_INT((uintptr_t)proc);
}

static YisVal __vimana_system_proc_write(YisVal sysv, YisVal procv,
                                         YisVal textv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  YisStr *tmp = NULL;
  const char *text = vimana_required_cstr(textv, &tmp);
  bool ok = vimana_process_write(proc, text);
  if (tmp)
    yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __vimana_system_proc_read_line(YisVal sysv, YisVal procv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  char *line = vimana_process_read_line(proc);
  if (!line)
    return YV_STR(stdr_str_lit(""));
  YisVal result = YV_STR(stdr_str_lit(line));
  free(line);
  return result;
}

static YisVal __vimana_system_proc_running(YisVal sysv, YisVal procv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  return YV_BOOL(vimana_process_running(proc));
}

static void __vimana_system_proc_kill(YisVal sysv, YisVal procv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  vimana_process_kill(proc);
}

static void __vimana_system_proc_free(YisVal sysv, YisVal procv) {
  (void)sysv;
  vimana_process *proc = (vimana_process *)(uintptr_t)yis_as_int(procv);
  vimana_process_free(proc);
}
