#include "tetris.h"
#include <device/framebuffer.h>
#include <device/shared.h>
#include <lib/PixelCore/backbuffer.h>
#include <lib/PixelCore/rect.h>
#include <lib/PixelCore/surface.h>
#include <lib/kalloc.h>
#include <lib/print.h>
#include <lib/timer.h>
#include <lib/time.h>
#include <lib/spinlock.h>
#include <proc/scheduler.h>

#include <device/virtio/virtio_keycode.h>
#include <device/virtio/drivers/input.h>

// Minimal scaffold for a Tetris app using PixelCore
// This draws a board grid and advances a timer; input hookup will follow.

typedef struct {
  int32_t width;
  int32_t height;
  int32_t cell_size;
  int32_t board_cols;
  int32_t board_rows;
  PCBackBuffer *bb;
  PCSurface *bg;
  PCSurface *board;
  PCSurface *grid;

  // input
  struct spinlock input_lock;
  g_bool left_down;
  g_bool right_down;
  g_bool down_down;
  g_bool rotate_cw_once;
  g_bool rotate_ccw_once;
  g_bool hard_drop_once;
  g_bool left_tap;
  g_bool right_tap;

  // board state
  uint32_t *cells; // board_rows * board_cols, ARGB, 0 = empty

  // current piece
  g_bool has_piece;
  int32_t piece_x;
  int32_t piece_y;
  uint8_t piece_type;
  uint8_t piece_rot;
  uint32_t piece_color;
} TetrisContext;

// --- gameplay helpers (file-scope) ---
typedef struct { int8_t dx; int8_t dy; } TOffset;

enum { TT_I = 0, TT_O, TT_T, TT_S, TT_Z, TT_J, TT_L, TT_COUNT };

static const TOffset SHAPES[7][4][4] = {
  // I
  { {{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}} },
  // O
  { {{1,1},{2,1},{1,2},{2,2}}, {{1,1},{2,1},{1,2},{2,2}}, {{1,1},{2,1},{1,2},{2,2}}, {{1,1},{2,1},{1,2},{2,2}} },
  // T
  { {{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}} },
  // S
  { {{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}}, {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}} },
  // Z
  { {{0,0},{1,0},{1,1},{2,1}}, {{2,0},{2,1},{1,1},{1,2}}, {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{1,1},{0,1},{0,2}} },
  // J
  { {{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}} },
  // L
  { {{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}}, {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}} },
};

static inline uint32_t color_for_type(uint8_t t) {
  switch (t) {
    case TT_I: return 0xFF00FFFFu; case TT_O: return 0xFFFFFF00u; case TT_T: return 0xFFAA00FFu;
    case TT_S: return 0xFF00FF00u; case TT_Z: return 0xFFFF0000u; case TT_J: return 0xFF0000FFu;
    case TT_L: return 0xFFFFA500u; default: return 0xFFFFFFFFu;
  }
}

static inline int32_t idx(TetrisContext *t, int32_t x, int32_t y) { return y * t->board_cols + x; }

static inline g_bool cell_occupied(TetrisContext *t, int32_t x, int32_t y) {
  if (x < 0 || x >= t->board_cols) return 1;
  if (y >= t->board_rows) return 1;
  if (y < 0) return 0;
  return t->cells[idx(t, x, y)] != 0;
}

static g_bool piece_collides(TetrisContext *t, int32_t px, int32_t py, uint8_t type, uint8_t rot) {
  for (int i = 0; i < 4; ++i) {
    int32_t x = px + SHAPES[type][rot][i].dx;
    int32_t y = py + SHAPES[type][rot][i].dy;
    if (cell_occupied(t, x, y)) return 1;
  }
  return 0;
}

static void spawn_piece(TetrisContext *t) {
  uint64_t r = get_csrr_time();
  uint8_t type = (uint8_t)(r % TT_COUNT);
  t->piece_type = type;
  t->piece_rot = 0;
  t->piece_x = (t->board_cols / 2) - 2;
  t->piece_y = -2;
  t->piece_color = color_for_type(type);
  t->has_piece = 1;
  if (piece_collides(t, t->piece_x, t->piece_y, t->piece_type, t->piece_rot)) {
    for (int32_t i = 0; i < t->board_cols * t->board_rows; ++i) t->cells[i] = 0;
    t->piece_y = 0;
  }
}

static void lock_piece(TetrisContext *t) {
  for (int i = 0; i < 4; ++i) {
    int32_t x = t->piece_x + SHAPES[t->piece_type][t->piece_rot][i].dx;
    int32_t y = t->piece_y + SHAPES[t->piece_type][t->piece_rot][i].dy;
    if (y >= 0 && y < t->board_rows && x >= 0 && x < t->board_cols) t->cells[idx(t, x, y)] = t->piece_color;
  }
  t->has_piece = 0;
}

static void clear_full_lines(TetrisContext *t) {
  for (int32_t y = t->board_rows - 1; y >= 0; --y) {
    g_bool full = 1;
    for (int32_t x = 0; x < t->board_cols; ++x) { if (t->cells[idx(t, x, y)] == 0) { full = 0; break; } }
    if (full) {
      for (int32_t yy = y; yy > 0; --yy) {
        for (int32_t x = 0; x < t->board_cols; ++x) t->cells[idx(t, x, yy)] = t->cells[idx(t, x, yy - 1)];
      }
      for (int32_t x = 0; x < t->board_cols; ++x) t->cells[idx(t, x, 0)] = 0;
      y++;
    }
  }
}

static void draw_board_surface(TetrisContext *t) {
  PCSurface *s = t->board;
  if (!s) return;
  for (uint32_t y = 0; y < s->rect.height; ++y) {
    uint32_t *row = s->pixels + (size_t)y * (size_t)s->stride;
    for (uint32_t x = 0; x < s->rect.width; ++x) row[x] = 0x00000000u;
  }
  const int cs = t->cell_size;
  for (int32_t by = 0; by < t->board_rows; ++by) {
    for (int32_t bx = 0; bx < t->board_cols; ++bx) {
      uint32_t col = t->cells[idx(t, bx, by)];
      if (!col) continue;
      int32_t px = bx * cs;
      int32_t py = by * cs;
      for (int32_t y = 0; y < cs; ++y) {
        uint32_t *row = s->pixels + (size_t)(py + y) * (size_t)s->stride;
        for (int32_t x = 0; x < cs; ++x) row[px + x] = col;
      }
    }
  }
  if (t->has_piece) {
    for (int i = 0; i < 4; ++i) {
      int32_t bx = t->piece_x + SHAPES[t->piece_type][t->piece_rot][i].dx;
      int32_t by = t->piece_y + SHAPES[t->piece_type][t->piece_rot][i].dy;
      if (by < 0) continue;
      if (bx < 0 || bx >= t->board_cols || by >= t->board_rows) continue;
      int32_t px = bx * cs;
      int32_t py = by * cs;
      for (int32_t y = 0; y < cs; ++y) {
        uint32_t *row = s->pixels + (size_t)(py + y) * (size_t)s->stride;
        for (int32_t x = 0; x < cs; ++x) row[px + x] = t->piece_color;
      }
    }
  }
  PCRect full = (PCRect){0,0,s->rect.width,s->rect.height};
  PCSurface_mark_dirty(s, full);
}

static void fill_surface_solid(PCSurface *s, uint32_t argb) {
  if (!s) return;
  for (uint32_t y = 0; y < s->rect.height; ++y) {
    uint32_t *row = s->pixels + (size_t)y * (size_t)s->stride;
    for (uint32_t x = 0; x < s->rect.width; ++x) {
      row[x] = argb;
    }
  }
  PCRect full = (PCRect){0,0,s->rect.width,s->rect.height};
  PCSurface_mark_dirty(s, full);
}

static void tetris_input_cb(const struct virtio_input_event *ev, void *user) {
  TetrisContext *tc = (TetrisContext *)user;
  if (!tc || !ev) return;

  // Keyboard event
  if (ev->type == 0x01) {
    int pressed = (ev->value != 0);
    acquire(&tc->input_lock);
    switch (ev->code) {
      case KEY_LEFT:  if (pressed) tc->left_tap = 1;  tc->left_down  = pressed ? 1 : 0; break;
      case KEY_RIGHT: if (pressed) tc->right_tap = 1; tc->right_down = pressed ? 1 : 0; break;
      case KEY_DOWN:  tc->down_down  = pressed ? 1 : 0; break;
      case KEY_UP:    if (pressed) tc->rotate_cw_once = 1; break;
      case KEY_Z:     if (pressed) tc->rotate_ccw_once = 1; break;
      case KEY_X:     if (pressed) tc->rotate_cw_once = 1; break;
      case KEY_SPACE: if (pressed) tc->hard_drop_once = 1; break;
      default: break;
    }
    release(&tc->input_lock);
  }
}

static void draw_grid(TetrisContext *tc) {
  if (!tc || !tc->grid) return;
  PCSurface *s = tc->grid;
  // Clear to transparent
  for (uint32_t y = 0; y < s->rect.height; ++y) {
    uint32_t *row = s->pixels + (size_t)y * (size_t)s->stride;
    for (uint32_t x = 0; x < s->rect.width; ++x) {
      row[x] = 0x00000000u;
    }
  }

  const uint32_t line = 0x40FFFFFFu; // faint white grid lines
  const int32_t cs = tc->cell_size;
  // Vertical lines
  for (int32_t c = 0; c <= tc->board_cols; ++c) {
    int32_t gx = c * cs;
    if (gx < 0 || gx >= (int32_t)s->rect.width) continue;
    for (uint32_t y = 0; y < s->rect.height; ++y) {
      s->pixels[(size_t)y * (size_t)s->stride + (size_t)gx] = line;
    }
  }
  // Horizontal lines
  for (int32_t r = 0; r <= tc->board_rows; ++r) {
    int32_t gy = r * cs;
    if (gy < 0 || gy >= (int32_t)s->rect.height) continue;
    uint32_t *row = s->pixels + (size_t)gy * (size_t)s->stride;
    for (uint32_t x = 0; x < s->rect.width; ++x) {
      row[x] = line;
    }
  }

  PCRect full = (PCRect){0,0,s->rect.width,s->rect.height};
  PCSurface_mark_dirty(s, full);
}

void tetris_app(void *arg) {
  framebuffer_t *fb = (framebuffer_t *)arg;
  if (!fb || !fb->is_initialized || !fb->framebuffer) {
    panic("tetris: framebuffer not initialized");
  }

  TetrisContext tc = {0};
  tc.width = (int32_t)fb->framebuffer->width;
  tc.height = (int32_t)fb->framebuffer->height;
  tc.cell_size = 24;
  tc.board_cols = 10;
  tc.board_rows = 20;

  tc.bb = PCBackBuffer_create((uint32_t)tc.width, (uint32_t)tc.height);
  if (!tc.bb) panic("tetris: failed to create backbuffer");

  PCRect bg_rect; PCRect_init(&bg_rect, 0, 0, (uint32_t)tc.width, (uint32_t)tc.height);
  tc.bg = PCSurface_create(&bg_rect, 0);
  if (!tc.bg) panic("tetris: failed to create bg surface");
  tc.bg->flags |= PC_SURFACE_OPAQUE | PC_SURFACE_VISIBLE;
  fill_surface_solid(tc.bg, 0xFF101018u);

  int32_t board_w = tc.board_cols * tc.cell_size;
  int32_t board_h = tc.board_rows * tc.cell_size;
  int32_t board_x = (tc.width - board_w) / 2;
  int32_t board_y = (tc.height - board_h) / 2;

  // Board surface (placed blocks + current piece)
  PCRect board_rect; PCRect_init(&board_rect, (int32_t)board_x, (int32_t)board_y, (uint32_t)board_w, (uint32_t)board_h);
  tc.board = PCSurface_create(&board_rect, 5);
  if (!tc.board) panic("tetris: failed to create board surface");
  tc.board->flags |= PC_SURFACE_VISIBLE;

  PCRect grid_rect; PCRect_init(&grid_rect, (int32_t)board_x, (int32_t)board_y, (uint32_t)board_w, (uint32_t)board_h);
  tc.grid = PCSurface_create(&grid_rect, 10);
  if (!tc.grid) panic("tetris: failed to create grid surface");
  tc.grid->flags |= PC_SURFACE_VISIBLE;
  draw_grid(&tc);

  if (!PCBackBuffer_add_surface(tc.bb, tc.bg)) panic("tetris: add bg failed");
  if (!PCBackBuffer_add_surface(tc.bb, tc.board)) panic("tetris: add board failed");
  if (!PCBackBuffer_add_surface(tc.bb, tc.grid)) panic("tetris: add grid failed");

  // Initial compose
  PCBackBuffer_compose(tc.bb);
  PCBackBuffer_flush_to_framebuffer(tc.bb, fb);

  const uint64_t frame_us = 1000000ULL / 60ULL; // 60 FPS
  // Register input listener (singleton input dev)
  virtio_input_dev_t *inp = virtio_input_get();
  if (inp) {
    virtio_input_add_listener(inp, tetris_input_cb, &tc);
  }

  initlock(&tc.input_lock, "tetris_input");

  // Allocate board cells
  tc.cells = (uint32_t *)kalloc((g_usize)(tc.board_cols * tc.board_rows * (int32_t)sizeof(uint32_t)));
  if (!tc.cells) panic("tetris: cells alloc failed");
  for (int32_t i = 0; i < tc.board_cols * tc.board_rows; ++i) tc.cells[i] = 0;

  // piece & timing
  tc.has_piece = 0;
  uint64_t last_tick_cycles = get_csrr_time();
  uint64_t tick_interval_us = 700000ULL; // gravity (0.7s)

  while (0) {}

  if (!tc.has_piece) spawn_piece(&tc);

  while (1) {
    // Attempt late registration if input wasn't available during init
    if (!inp) {
      inp = virtio_input_get();
      if (inp) {
        virtio_input_add_listener(inp, tetris_input_cb, &tc);
      }
    }

    // Handle keyboard input
    acquire(&tc.input_lock);
    g_bool left_press = tc.left_tap; tc.left_tap = 0;
    g_bool right_press = tc.right_tap; tc.right_tap = 0;
    g_bool down = tc.down_down;
    g_bool do_rot_cw = tc.rotate_cw_once; tc.rotate_cw_once = 0;
    g_bool do_rot_ccw = tc.rotate_ccw_once; tc.rotate_ccw_once = 0;
    g_bool do_hard_drop = tc.hard_drop_once; tc.hard_drop_once = 0;
    release(&tc.input_lock);

    // soft drop affects gravity
    tick_interval_us = down ? 50000ULL : 700000ULL;

    if (!tc.has_piece) spawn_piece(&tc);

    if (do_rot_cw || do_rot_ccw) {
      uint8_t next_rot = (uint8_t)((tc.piece_rot + (do_rot_cw ? 1 : 3)) & 3);
      if (!piece_collides(&tc, tc.piece_x, tc.piece_y, tc.piece_type, next_rot)) tc.piece_rot = next_rot;
      else if (!piece_collides(&tc, tc.piece_x + 1, tc.piece_y, tc.piece_type, next_rot)) { tc.piece_x += 1; tc.piece_rot = next_rot; }
      else if (!piece_collides(&tc, tc.piece_x - 1, tc.piece_y, tc.piece_type, next_rot)) { tc.piece_x -= 1; tc.piece_rot = next_rot; }
    }

    if (left_press && !piece_collides(&tc, tc.piece_x - 1, tc.piece_y, tc.piece_type, tc.piece_rot)) tc.piece_x -= 1;
    if (right_press && !piece_collides(&tc, tc.piece_x + 1, tc.piece_y, tc.piece_type, tc.piece_rot)) tc.piece_x += 1;

    if (do_hard_drop) { while (!piece_collides(&tc, tc.piece_x, tc.piece_y + 1, tc.piece_type, tc.piece_rot)) tc.piece_y += 1; lock_piece(&tc); clear_full_lines(&tc); spawn_piece(&tc); }

    // Gravity tick placeholder
    uint64_t now_cycles = get_csrr_time();
    uint64_t elapsed_us = (now_cycles - last_tick_cycles) / (TIMER_FREQUENCY / 1000000ULL);
    if (elapsed_us >= tick_interval_us) {
      last_tick_cycles = now_cycles;
      if (!piece_collides(&tc, tc.piece_x, tc.piece_y + 1, tc.piece_type, tc.piece_rot)) tc.piece_y += 1; else { lock_piece(&tc); clear_full_lines(&tc); spawn_piece(&tc); }
    }

    // Draw and flush
    draw_board_surface(&tc);
    PCBackBuffer_compose(tc.bb);
    PCBackBuffer_flush_to_framebuffer(tc.bb, fb);

    sleep_us(frame_us);
    yield();
  }
}


