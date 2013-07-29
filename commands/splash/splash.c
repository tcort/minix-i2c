/*
 * splash
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
//#include <stropts.h>
#include <string.h>

#include <sys/ioctl.h>
#include "splash.h"

#ifdef __minix
#include <sys/ioc_fb.h>
#include <minix/fb.h>
#endif

struct fb_fix_screeninfo fixed_screen_info;
struct fb_var_screeninfo variable_screen_info;

int
open_fb(struct fb_data *fb_d, char *fb_path)
{

	/* copy the file name for future reference */
	fb_d->fb_path = strndup(fb_path, 100);

	/* open the frame buffer */
	fb_d->fd = open(fb_d->fb_path, O_RDWR);

	if (fb_d->fd == -1) {
		perror("open framebuffer");
		return -1;
	}

	fb_d->buf = malloc(sizeof(struct fb_buf));
	if (!fb_d->buf) {
		perror("open_fb alloc buff");
		return -4;
	}

	/* read the fixed and variable framebuffer info */
	if (ioctl(fb_d->fd, FBIOGET_FSCREENINFO, &fixed_screen_info) < 0) {
		perror("get fixed screen info");
		close(fb_d->fd);
		return -2;
	}

	if (ioctl(fb_d->fd, FBIOGET_VSCREENINFO, &variable_screen_info) < 0) {
		perror("get variable screen info");
		close(fb_d->fd);
		return -3;
	}

	/* hard code the settings */
	fb_d->buf->w = 1024;
	fb_d->buf->h = 600;
	fb_d->buf->bpp = 32;
	fb_d->buf->bytes_per_pixel = 4;
	fb_d->buf->line_length = fb_d->buf->w * fb_d->buf->bytes_per_pixel;
	fb_d->buf->fmt.a.shift = 24;
	fb_d->buf->fmt.r.shift = 16;
	fb_d->buf->fmt.g.shift = 8;
	fb_d->buf->fmt.b.shift = 0;

	fb_d->buf->fmt.a.mask = 0xf;
	fb_d->buf->fmt.r.mask = 0xf;
	fb_d->buf->fmt.g.mask = 0xf;
	fb_d->buf->fmt.b.mask = 0xf;
	fb_d->current_page =0;

	fb_d->buf->data = malloc(fb_d->buf->h * fb_d->buf->line_length);
	if (!fb_d->buf->data) {
		perror("open_fb alloc data failed");
		return -4;
	}

	return 0;		/* success */
}

void
close_fb(struct fb_data *fb_d)
{
	free(fb_d->buf->data);
	free(fb_d->buf);
	close(fb_d->fd);
}

int
reopen_fb(struct fb_data *fb_d)
{
	if (fb_d->fd != -1) {
		close(fb_d->fd);
	}
	/* reopen the frame buffer */
	fb_d->fd = open(fb_d->fb_path, O_RDWR);

	if (fb_d->fd == -1) {
		perror("open framebuffer");
		return -1;
	}
	return 0;
}

void
put_pixel(struct fb_buf *buf, int x, int y, unsigned char red,
    unsigned char green, unsigned char blue, unsigned char alpha)
{
	int offset = 0;
	offset = y * buf->line_length + (x * buf->bytes_per_pixel);

	if (buf->bpp == 16) {
		*(volatile unsigned
		    short *) (buf->data + offset) =
		    (((red >> 3) & 0x1F) << buf->fmt.r.shift) |
		    (((green >> 2) & 0x3F) << buf->fmt.g.shift) |
		    (((blue >> 3) & 0x1F) << buf->fmt.b.shift);
	}
	if (buf->bpp == 32) {
		*(long *) (buf->data + offset) =
		    (red << buf->fmt.r.shift) |
		    (green << buf->fmt.g.shift) |
		    (blue << buf->fmt.b.shift) | (alpha << buf->fmt.a.shift);
	}

}

void
put_pixel_fb(struct fb_data *fb_d, int x, int y, unsigned char red,
    unsigned char green, unsigned char blue)
{
	struct fb_buf *buf = fb_d->buf;
	int offset = 0;
	offset = y * buf->line_length + (x * buf->bpp >> 3);

	if (buf->bpp == 16) {
		*(volatile unsigned
		    short *) (buf->data + offset) =
		    (((red >> 3) & 0x1F) << buf->fmt.
		    r.shift) | (((green >> 2) & 0x3F)
		    << buf->fmt.g.shift)
		    | (((blue >> 3) & 0x1F)
		    << buf->fmt.b.shift);
	}

}

void
draw_rle_file(struct fb_data *fb_d, char *name, int align)
{
	struct image_header header;
	unsigned char length;
	unsigned short color;
	unsigned int counter;
	unsigned int l;
	unsigned int base_x, base_y;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	FILE *fp = fopen(name, "rb");
	if (!fp) {
		printf("Failed to open RLE file\n");
		return;
	}

	fread(&header, sizeof(struct image_header), 1, fp);
	printf("Image is w,h,bpp=%i ,%i ,%i\n", header.width, header.height,
	    header.bpp);
	if (align == IMAGE_ALIGN_CENTER) {
		base_x = (fb_d->buf->w - header.width) / 2;
		base_y = (fb_d->buf->h - header.height) / 2;
	} else {
		base_x = 0;
		base_y = 0;
	}
	counter = 0;
	while (counter < header.width * header.height) {
		fread(&length, sizeof(unsigned char), 1, fp);
		fread(&color, sizeof(unsigned short int), 1, fp);
		red = ((color >> 11) & 0x1f) << 3;
		green = ((color >> 5) & 0x1f) << 3;
		blue = (color & 0x1f) << 3;
		for (l = 0; l < length; l++) {
			put_pixel_fb(fb_d,
			    base_x +
			    (counter % header.width),
			    base_y + (counter / header.width), red, green,
			    blue);
			counter++;
		}
	}
	fclose(fp);
}

int
flip(struct fb_data *fb_d)
{
	int offset = 0, written = 0, pagesize = 0;
	pagesize = variable_screen_info.xres *
	    variable_screen_info.yres *
	    (variable_screen_info.bits_per_pixel / 8);

	if (variable_screen_info.yoffset == 0) {
		offset = pagesize;
	}
	if (lseek(fb_d->fd, offset, SEEK_SET) != offset) {
		printf("Couldn't seek to offset %d\n", offset);
		return 1;
		exit(1);
	}

	do {
		int w;
		w = write(fb_d->fd, fb_d->buf->data, pagesize - written);

		if (w == 0)
			break;
		if (w < 0) {
			printf
			    ("Unable to write to /dev/fb0: errno=%d fd=%d bufsz=%d, written=%d\n",
			    errno, fb_d->fd, pagesize - written, written);
			return 1;
		}
		written += w;
	} while (written < pagesize);

	fb_d->current_page = (fb_d->current_page) ? 0 : 1;
	variable_screen_info.yoffset =
	    fb_d->current_page * variable_screen_info.yres;
	ioctl(fb_d->fd, FBIOPAN_DISPLAY, &variable_screen_info);
	return 0;
}
