#include <errno.h>
#include <extra.h>
#include <fb.h>
#include <fcntl.h>
#include <panic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define QOI_OP_INDEX 0x00
#define QOI_OP_DIFF 0x40
#define QOI_OP_LUMA 0x80
#define QOI_OP_RUN 0xc0
#define QOI_OP_RGB 0xfe
#define QOI_OP_RGBA 0xff

#define QOI_MASK_2 0xc0

struct qoi_header {
    char magic[4];
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorspace;
} __attribute__((packed));

typedef union {
    struct {
        unsigned char r, g, b, a;
    } rgba;
    unsigned int v;
} qoi_rgba_t;

static uint8_t hash(const qoi_rgba_t* c) {
    return (c->rgba.r * 3 + c->rgba.g * 5 + c->rgba.b * 7 + c->rgba.a * 11) %
           64;
}

static uint32_t swap_bytes(uint32_t x) {
    return ((x >> 24) & 0xff) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) |
           ((x << 24) & 0xff000000);
}

int main(int argc, char* const argv[]) {
    if (argc != 2) {
        dprintf(STDERR_FILENO, "Usage: imgview FILE\n");
        return EXIT_FAILURE;
    }

    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        if (errno == ENOENT)
            dprintf(STDERR_FILENO, "Framebuffer is not available\n");
        else
            perror("open");
        return EXIT_FAILURE;
    }
    struct fb_info fb_info;
    if (ioctl(fb_fd, FBIOGET_INFO, &fb_info) < 0) {
        perror("ioctl");
        close(fb_fd);
        return EXIT_FAILURE;
    }
    ASSERT(fb_info.bpp == 32);
    void* fb = mmap(NULL, fb_info.pitch * fb_info.height,
                    PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    close(fb_fd);
    if (fb == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    int img_fd = open(argv[1], O_RDONLY);
    if (img_fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    struct qoi_header header;
    ssize_t nread = read(img_fd, &header, sizeof(struct qoi_header));
    if (nread < 0) {
        perror("read");
        close(img_fd);
        return EXIT_FAILURE;
    }
    if ((size_t)nread < sizeof(struct qoi_header) ||
        strncmp(header.magic, "qoif", 4) != 0) {
        dprintf(STDERR_FILENO, "Not a QOI file\n");
        close(img_fd);
        return EXIT_FAILURE;
    }

    header.width = swap_bytes(header.width);
    header.height = swap_bytes(header.height);

    size_t visible_width = MIN(header.width, fb_info.width);
    size_t visible_height = MIN(header.height, fb_info.height);

    qoi_rgba_t index[64];
    memset(index, 0, sizeof(index));

    qoi_rgba_t px = {.rgba = {.r = 0, .g = 0, .b = 0, .a = 255}};

    size_t run = 0;
    uintptr_t fb_row_addr = (uintptr_t)fb;

    for (size_t y = 0; y < visible_height; ++y) {
        uint32_t* pixel = (uint32_t*)fb_row_addr;

        for (size_t x = 0; x < header.width; ++x) {
            if (run > 0) {
                --run;
            } else {
                uint8_t b1;
                read(img_fd, &b1, 1);

                switch (b1) {
                case QOI_OP_RGB:
                    read(img_fd, &px.rgba, 3);
                    break;
                case QOI_OP_RGBA:
                    read(img_fd, &px.rgba, 4);
                    break;
                default:
                    switch (b1 & QOI_MASK_2) {
                    case QOI_OP_INDEX:
                        px = index[b1];
                        break;
                    case QOI_OP_DIFF:
                        px.rgba.r += ((b1 >> 4) & 0x03) - 2;
                        px.rgba.g += ((b1 >> 2) & 0x03) - 2;
                        px.rgba.b += (b1 & 0x03) - 2;
                        break;
                    case QOI_OP_LUMA: {
                        uint8_t b2;
                        read(img_fd, &b2, 1);
                        int vg = (b1 & 0x3f) - 32;
                        px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
                        px.rgba.g += vg;
                        px.rgba.b += vg - 8 + (b2 & 0x0f);
                        break;
                    }
                    case QOI_OP_RUN:
                        run = (b1 & 0x3f);
                        break;
                    }
                    break;
                }

                index[hash(&px)] = px;
            }

            if (x < visible_width)
                *pixel++ = (px.rgba.r << 16) | (px.rgba.g << 8) | px.rgba.b;
        }

        fb_row_addr += fb_info.pitch;
    }

    close(img_fd);

    (void)getchar();

    return EXIT_SUCCESS;
}
