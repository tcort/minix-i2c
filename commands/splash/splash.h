/*
 * splash
 */

#ifndef __SPLASH_H__
#define __SPLASH_H__

struct color_fmt
{
	u8_t shift;
	u8_t mask;
};

struct pix_fmt
{
	struct color_fmt r;
	struct color_fmt g;
	struct color_fmt b;
	struct color_fmt a;
};

struct fb_buf
{
	int w, h;		/* with and height */
	unsigned char *data;	/* the data itself */
	unsigned int bpp;	/* bits per pixel */
	unsigned int bytes_per_pixel;	/* bytes per pixel */
	unsigned int line_length;	/* length of a line in bytes */
	struct pix_fmt fmt;
};

struct fb_data
{
	int fd;
	char * fb_path;
	struct fb_buf *buf;
	unsigned int current_page;
};

struct image_header
{
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
};

#define IMAGE_ALIGN_TOP_LEFT 0
#define IMAGE_ALIGN_CENTER 1

/* open frambuffer and allocate structures */
int open_fb(struct fb_data *fb_d, char *fb_dev_name);
/* reopen frambuffer */
int reopen_fb(struct fb_data *fb_d);

/* close framebuffer and free resources */
void close_fb(struct fb_data *fb_d);

void put_pixel(struct fb_buf *buf, int x, int y, unsigned char red,
    unsigned char green, unsigned char blue,unsigned char alpha);
void put_pixel_fb(struct fb_data *fb_d, int x, int y, unsigned char red,
    unsigned char green, unsigned char blue);

void draw_rle_file(struct fb_data *fb_d, char *name, int align);

int flip(struct fb_data *fb_d);

#endif /* __SPLASH_H__ */
