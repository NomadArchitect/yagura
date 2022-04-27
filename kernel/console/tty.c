#include "psf.h"
#include <common/extra.h>
#include <common/stdlib.h>
#include <common/string.h>
#include <kernel/api/fb.h>
#include <kernel/api/fcntl.h>
#include <kernel/api/hid.h>
#include <kernel/api/sys/mman.h>
#include <kernel/api/sys/stat.h>
#include <kernel/api/sys/sysmacros.h>
#include <kernel/boot_defs.h>
#include <kernel/fs/fs.h>
#include <kernel/interrupts.h>
#include <kernel/kmalloc.h>
#include <kernel/lock.h>
#include <kernel/memory/memory.h>
#include <kernel/panic.h>
#include <kernel/scheduler.h>
#include <string.h>

#define TAB_STOP 8

#define DEFAULT_FG_COLOR 0
#define DEFAULT_BG_COLOR 7

static const uint32_t palette[] = {
    0xd0d0d0, 0xcc0000, 0x4e9a06, 0xc4a000, 0x3465a4, 0x75507b,
    0x06989a, 0x191919, 0x555753, 0xef2929, 0x8ae234, 0xfce94f,
    0x729fcf, 0xad7fa8, 0x34e2e2, 0xeeeeec,
};

struct cell {
    char ch;
    uint8_t fg_color;
    uint8_t bg_color;
};

static struct font* font;
static uintptr_t fb_addr;
static struct fb_info fb_info;
static size_t console_width;
static size_t console_height;
static size_t cursor_x = 0;
static size_t cursor_y = 0;
static bool is_cursor_visible = true;
static enum { STATE_GROUND, STATE_ESC, STATE_CSI } state = STATE_GROUND;
static uint8_t fg_color = DEFAULT_FG_COLOR;
static uint8_t bg_color = DEFAULT_BG_COLOR;
static struct cell* cells;
static bool* line_is_dirty;
static bool whole_screen_should_be_cleared = false;

static void set_cursor(size_t x, size_t y) {
    line_is_dirty[cursor_y] = true;
    line_is_dirty[y] = true;
    cursor_x = x;
    cursor_y = y;
}

static void clear_line_at(size_t x, size_t y, size_t length) {
    struct cell* cell = cells + x + y * console_width;
    for (size_t i = 0; i < length; ++i) {
        cell->ch = ' ';
        cell->fg_color = DEFAULT_FG_COLOR;
        cell->bg_color = DEFAULT_BG_COLOR;
        ++cell;
    }
    line_is_dirty[y] = true;
}

static void clear_screen(void) {
    for (size_t y = 0; y < console_height; ++y)
        clear_line_at(0, y, console_width);
    whole_screen_should_be_cleared = true;
}

static void write_char_at(size_t x, size_t y, char c) {
    struct cell* cell = cells + x + y * console_width;
    cell->ch = c;
    cell->fg_color = fg_color;
    cell->bg_color = bg_color;
    line_is_dirty[y] = true;
}

static void scroll_up(void) {
    memmove(cells, cells + console_width,
            console_width * (console_height - 1) * sizeof(struct cell));
    for (size_t y = 0; y < console_height - 1; ++y)
        line_is_dirty[y] = true;
    clear_line_at(0, console_height - 1, console_width);
}

static void flush_cell_at(size_t x, size_t y, struct cell* cell) {
    bool is_cursor = is_cursor_visible && x == cursor_x && y == cursor_y;
    uint32_t fg = palette[is_cursor ? cell->bg_color : cell->fg_color];
    uint32_t bg = palette[is_cursor ? cell->fg_color : cell->bg_color];

    const unsigned char* glyph =
        font->glyphs +
        font->ascii_to_glyph[(size_t)cell->ch] * font->bytes_per_glyph;
    uintptr_t row_addr = fb_addr + x * font->glyph_width * sizeof(uint32_t) +
                         y * font->glyph_height * fb_info.pitch;
    for (size_t py = 0; py < font->glyph_height; ++py) {
        uint32_t* pixel = (uint32_t*)row_addr;
        for (size_t px = 0; px < font->glyph_width; ++px) {
            uint32_t val = *(const uint32_t*)glyph;
            uint32_t swapped = ((val >> 24) & 0xff) | ((val << 8) & 0xff0000) |
                               ((val >> 8) & 0xff00) |
                               ((val << 24) & 0xff000000);
            *pixel++ = swapped & (1 << (32 - px - 1)) ? fg : bg;
        }
        glyph += font->bytes_per_glyph / font->glyph_height;
        row_addr += fb_info.pitch;
    }
}

static void flush(void) {
    if (whole_screen_should_be_cleared) {
        memset32((uint32_t*)fb_addr, palette[bg_color],
                 fb_info.pitch * fb_info.height / sizeof(uint32_t));
        whole_screen_should_be_cleared = false;
    }

    struct cell* row_cells = cells;
    bool* dirty = line_is_dirty;
    for (size_t y = 0; y < console_height; ++y) {
        if (*dirty) {
            struct cell* cell = row_cells;
            for (size_t x = 0; x < console_width; ++x)
                flush_cell_at(x, y, cell++);
            *dirty = false;
        }
        row_cells += console_width;
        ++dirty;
    }
}

static void handle_ground(char c) {
    switch (c) {
    case '\x1b':
        state = STATE_ESC;
        return;
    case '\r':
        set_cursor(0, cursor_y);
        break;
    case '\n':
        set_cursor(0, cursor_y + 1);
        break;
    case '\b':
        if (cursor_x > 0)
            set_cursor(cursor_x - 1, cursor_y);
        break;
    case '\t':
        set_cursor(round_up(cursor_x + 1, TAB_STOP), cursor_y);
        break;
    default:
        if ((unsigned)c > 127)
            return;
        write_char_at(cursor_x, cursor_y, c);
        set_cursor(cursor_x + 1, cursor_y);
        break;
    }
    if (cursor_x >= console_width)
        set_cursor(0, cursor_y + 1);
    if (cursor_y >= console_height) {
        scroll_up();
        set_cursor(cursor_x, cursor_y - 1);
    }
}

static char param_buf[1024];
static size_t param_buf_idx = 0;

static void handle_state_esc(char c) {
    switch (c) {
    case '[':
        param_buf_idx = 0;
        state = STATE_CSI;
        return;
    }
    state = STATE_GROUND;
    handle_ground(c);
}

// Cursor Up
static void handle_csi_cuu(void) {
    unsigned dy = atoi(param_buf);
    if (dy == 0)
        dy = 1;
    if (dy > cursor_y)
        set_cursor(cursor_x, 0);
    else
        set_cursor(cursor_x, cursor_y - dy);
}

// Cursor Down
static void handle_csi_cud(void) {
    unsigned dy = atoi(param_buf);
    if (dy == 0)
        dy = 1;
    if (dy + cursor_y >= console_height)
        set_cursor(cursor_x, console_height - 1);
    else
        set_cursor(cursor_x, cursor_y + dy);
}

// Cursor Forward
static void handle_csi_cuf(void) {
    unsigned dx = atoi(param_buf);
    if (dx == 0)
        dx = 1;
    if (dx + cursor_x >= console_width)
        set_cursor(console_width - 1, cursor_y);
    else
        set_cursor(cursor_x + dx, cursor_y);
}

// Cursor Back
static void handle_csi_cub(void) {
    unsigned dx = atoi(param_buf);
    if (dx == 0)
        dx = 1;
    if (dx > cursor_x)
        set_cursor(0, cursor_y);
    else
        set_cursor(cursor_x - dx, cursor_y);
}

// Cursor Horizontal Absolute
static void handle_csi_cha(void) {
    unsigned x = atoi(param_buf);
    if (x > 0)
        --x;
    set_cursor(x, cursor_y);
}

// Cursor Position
static void handle_csi_cup(void) {
    size_t x = 0;
    size_t y = 0;

    static const char* sep = ";";
    char* saved_ptr;
    const char* param = strtok_r(param_buf, sep, &saved_ptr);
    for (size_t i = 0; param; ++i) {
        switch (i) {
        case 0:
            x = atoi(param) - 1;
            break;
        case 1:
            y = atoi(param) - 1;
            break;
        }
        param = strtok_r(NULL, sep, &saved_ptr);
    }

    set_cursor(x, y);
}

// Erase in Display
static void handle_csi_ed() {
    switch (atoi(param_buf)) {
    case 0:
        clear_line_at(cursor_x, cursor_y, console_width - cursor_x);
        for (size_t y = cursor_y + 1; y < console_height; ++y)
            clear_line_at(0, y, console_width);
        break;
    case 1:
        if (cursor_y > 0) {
            for (size_t y = 0; y < cursor_y - 1; ++y)
                clear_line_at(0, y, console_width);
        }
        clear_line_at(0, cursor_y, cursor_x + 1);
        break;
    case 2:
        clear_screen();
        break;
    }
}

// Erase in Line
static void handle_csi_el() {
    switch (atoi(param_buf)) {
    case 0:
        clear_line_at(cursor_x, cursor_y, console_width - cursor_x);
        break;
    case 1:
        clear_line_at(0, cursor_y, cursor_x + 1);
        break;
    case 2:
        clear_line_at(0, cursor_y, console_width);
        break;
    }
}

// Select Graphic Rendition
static void handle_csi_sgr(void) {
    if (param_buf[0] == '\0') {
        fg_color = DEFAULT_FG_COLOR;
        bg_color = DEFAULT_BG_COLOR;
        return;
    }

    static const char* sep = ";";
    char* saved_ptr;
    bool bold = false;
    for (const char* param = strtok_r(param_buf, sep, &saved_ptr); param;
         param = strtok_r(NULL, sep, &saved_ptr)) {
        int num = atoi(param);
        if (num == 0) {
            fg_color = DEFAULT_FG_COLOR;
            bg_color = DEFAULT_BG_COLOR;
            bold = false;
        } else if (num == 1) {
            bold = true;
        } else if (num == 7) {
            uint8_t tmp = fg_color;
            fg_color = bg_color;
            bg_color = tmp;
        } else if (num == 22) {
            fg_color = DEFAULT_FG_COLOR;
            bold = false;
        } else if (30 <= num && num <= 37) {
            fg_color = num - 30 + (bold ? 8 : 0);
        } else if (num == 38) {
            fg_color = DEFAULT_FG_COLOR;
        } else if (40 <= num && num <= 47) {
            bg_color = num - 40 + (bold ? 8 : 0);
        } else if (num == 48) {
            bg_color = DEFAULT_BG_COLOR;
        } else if (90 <= num && num <= 97) {
            fg_color = num - 90 + 8;
        } else if (100 <= num && num <= 107) {
            bg_color = num - 100 + 8;
        }
    }
}

// Text Cursor Enable Mode
static void handle_csi_dectcem(char c) {
    if (strcmp(param_buf, "?25") != 0)
        return;
    switch (c) {
    case 'h':
        is_cursor_visible = true;
        return;
    case 'l':
        is_cursor_visible = false;
        return;
    }
}

static void handle_state_csi(char c) {
    if (c < 0x40) {
        param_buf[param_buf_idx++] = c;
        return;
    }
    param_buf[param_buf_idx] = '\0';

    switch (c) {
    case 'A':
        handle_csi_cuu();
        break;
    case 'B':
        handle_csi_cud();
        break;
    case 'C':
        handle_csi_cuf();
        break;
    case 'D':
        handle_csi_cub();
        break;
    case 'G':
        handle_csi_cha();
        break;
    case 'H':
        handle_csi_cup();
        break;
    case 'J':
        handle_csi_ed();
        break;
    case 'K':
        handle_csi_el();
        break;
    case 'm':
        handle_csi_sgr();
        break;
    case 'h':
    case 'l':
        handle_csi_dectcem(c);
        break;
    }

    state = STATE_GROUND;
}

static void on_char(char c) {
    switch (state) {
    case STATE_GROUND:
        handle_ground(c);
        return;
    case STATE_ESC:
        handle_state_esc(c);
        return;
    case STATE_CSI:
        handle_state_csi(c);
        return;
    }
    UNREACHABLE();
}

static bool initialized = false;

void tty_init(void) {
    font = load_psf("/usr/share/fonts/ter-u16n.psf");
    ASSERT_OK(font);

    file_description* desc = vfs_open("/dev/fb0", O_RDWR, 0);
    ASSERT_OK(desc);

    ASSERT_OK(fs_ioctl(desc, FBIOGET_INFO, &fb_info));
    ASSERT(fb_info.bpp == 32);

    console_width = fb_info.width / font->glyph_width;
    console_height = fb_info.height / font->glyph_height;

    cells = kmalloc(console_width * console_height * sizeof(struct cell));
    ASSERT(cells);
    line_is_dirty = kmalloc(console_height * sizeof(bool));
    ASSERT(line_is_dirty);

    size_t fb_size = fb_info.pitch * fb_info.height;
    uintptr_t vaddr = kernel_vaddr_allocator_alloc(fb_size);
    ASSERT_OK(vaddr);
    fb_addr = fs_mmap(desc, vaddr, fb_size, 0,
                      PAGE_WRITE | PAGE_SHARED | PAGE_GLOBAL);
    ASSERT_OK(fb_addr);

    ASSERT_OK(fs_close(desc));

    clear_screen();
    flush();

    initialized = true;
}

#define QUEUE_SIZE 1024

static char queue[QUEUE_SIZE];
static size_t queue_read_idx = 0;
static size_t queue_write_idx = 0;

static void queue_push_char(char c) {
    queue[queue_write_idx] = c;
    queue_write_idx = (queue_write_idx + 1) % QUEUE_SIZE;
}

static void queue_push_str(const char* s) {
    for (; *s; ++s)
        queue_push_char(*s);
}

void tty_on_key(const key_event* event) {
    if (!event->pressed)
        return;
    switch (event->keycode) {
    case KEYCODE_UP:
        queue_push_str("\x1b[A");
        return;
    case KEYCODE_DOWN:
        queue_push_str("\x1b[B");
        return;
    case KEYCODE_RIGHT:
        queue_push_str("\x1b[C");
        return;
    case KEYCODE_LEFT:
        queue_push_str("\x1b[D");
        return;
    case KEYCODE_HOME:
        queue_push_str("\x1b[H");
        return;
    case KEYCODE_END:
        queue_push_str("\x1b[F");
        return;
    case KEYCODE_DELETE:
        queue_push_str("\x1b[3~");
        return;
    default:
        break;
    }

    if (!event->key)
        return;
    char key = event->key;
    if (event->modifiers & KEY_MODIFIER_CTRL) {
        if ('a' <= key && key <= 'z')
            key -= '`';
        else if (key == '\\')
            key = 0x1c;
    }
    queue_push_char(key);
}

typedef struct tty_device {
    struct file base_file;
    mutex lock;
} tty_device;

static bool read_should_unblock(void) {
    bool int_flag = push_cli();
    bool should_unblock = queue_read_idx != queue_write_idx;
    pop_cli(int_flag);
    return should_unblock;
}

static ssize_t tty_device_read(file_description* desc, void* buffer,
                               size_t count) {
    (void)desc;

    size_t nread = 0;
    char* out = (char*)buffer;
    scheduler_block(read_should_unblock, NULL);

    bool int_flag = push_cli();

    while (count > 0) {
        if (queue_read_idx == queue_write_idx)
            break;
        *out++ = queue[queue_read_idx];
        ++nread;
        --count;
        queue_read_idx = (queue_read_idx + 1) % QUEUE_SIZE;
    }

    pop_cli(int_flag);

    return nread;
}

static ssize_t tty_device_write(file_description* desc, const void* buffer,
                                size_t count) {
    tty_device* dev = (tty_device*)desc->file;
    const char* chars = (char*)buffer;

    mutex_lock(&dev->lock);

    for (size_t i = 0; i < count; ++i)
        on_char(chars[i]);
    flush();

    mutex_unlock(&dev->lock);
    return count;
}

struct file* tty_device_create(void) {
    tty_device* dev = kmalloc(sizeof(tty_device));
    if (!dev)
        return ERR_PTR(-ENOMEM);
    *dev = (tty_device){0};
    mutex_init(&dev->lock);

    struct file* file = (struct file*)dev;
    file->name = kstrdup("tty_device");
    if (!file->name)
        return ERR_PTR(-ENOMEM);
    file->mode = S_IFCHR;
    file->read = tty_device_read;
    file->write = tty_device_write;
    file->device_id = makedev(5, 0);
    return file;
}
