#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "splash.h"

#include "raccoon.h"
#include "raccoon_mask.h"

#define MAX(a,b) ((a)<(b)?(a):(b))
#define MIN(a,b) ((a)>(b)?(a):(b))

void
cls(struct fb_buf *buf, char r, char g, char b)
{
	int x, y;
	for (x = 0; x < buf->w; x++) {
		for (y = 0; y < buf->h; y++) {
			put_pixel(buf, x, y, r, g, b,0x00);
		}
	}
}

int
main(int argc, char **argv)
{
	int ret = 0;
	struct fb_data fb_data;
	ret = open_fb(&fb_data, "/dev/fb0");
	if (ret) {
		printf("Error opening the famebuffer\n");
		exit(EXIT_FAILURE);
	}

	if (fb_data.buf->bpp != 16 && fb_data.buf->bpp != 32) {
		printf("Only 16bpp and 32 bbp displays are supported\n");
		exit(EXIT_FAILURE);
	}

	printf("Opened the screen (width,height,bbp)=(%i,%i,%i)\n",
	    fb_data.buf->w, fb_data.buf->h, fb_data.buf->bpp);

//      draw_rle_file(&fb_data, "logo.rle", IMAGE_ALIGN_CENTER);

	struct fb_buf *tex = malloc(sizeof(struct fb_buf));
	if (!tex) {
		fprintf(stderr, "Failed to allocate texture\n");
	}

	tex->w = 128;
	tex->h = 128;
	tex->bpp = 32;
	tex->bytes_per_pixel = 4;

	char pixel[3];
	char mask_pixel[3];
	char *data = raccoon_data;
	char *mask_data = raccoon_mask_data;

	tex->line_length = tex->w * (tex->bpp >> 3);
	tex->data = malloc(tex->line_length * tex->h);
	memcpy(&tex->fmt, &fb_data.buf->fmt, sizeof(struct pix_fmt));
	int w, h;
	for (h = 0; h < tex->h; h++) {
		for (w = 0; w < tex->w; w++) {
			HEADER_PIXEL(data, pixel)
			HEADER_PIXEL(mask_data, mask_pixel)
		        put_pixel(tex, w, h, pixel[1], pixel[1], pixel[2], 0xff - mask_pixel[1]);
		}
	}

	/* finished drawing the data on the texture */

	int count =0;

	int k;
	int p[3][3] = {
		{0, 0, 1},	/* texture */
		{0, 128, 1},
		{128, 0, 1},
	};
	int z = 1;
	for (k = 110; ; k += z) {

		//cls(fb_data.buf,0xff,0xff,0xff);
		if (k > 140 || k < 100) {
			z = -z;
		}
		p[0][1] = 0;
		p[1][1] = k;
		p[2][0] = k;
		int px, py, pz, mx, my, mz, nx, ny, nz;

		px = p[0][0];
		py = p[0][1];
		pz = p[0][2];

		mx = p[1][0] - px;
		my = p[1][1] - py;
		mz = p[1][2] - pz;

		nx = px - p[2][0];
		ny = py - p[2][1];
		nz = pz - p[2][2];

		int hx, vx, ox, hy, vy, oy, hz, vz, oz;

		hx = (py * mz) - (pz * my);
		vx = (pz * mx) - (px * mz);
		ox = (px * my) - (py * mx);

		hy = (py * nz) - (pz * ny);
		vy = (pz * nx) - (px * nz);
		oy = (px * ny) - (py * nx);

		hz = (ny * mz) - (nz * my);
		vz = (nz * mx) - (nx * mz);
		oz = (nx * my) - (ny * mx);

		int x, y, z;
		int tu, tv;

		int fx, fy;
		for (fy = 0; fy < fb_data.buf->h; fy++) {
//		for (fy = 200; fy < fb_data.buf->h - 200; fy++) {  
			for (fx = 0; fx < fb_data.buf->w; fx++) {
//			for (fx = 200; fx < fb_data.buf->w - 200; fx++) {
				x = ox + (hx * fx) + (vx * fy);
				y = oy + (hy * fx) + (vy * fy);
				z = oz + (hz * fx) + (vz * fy);
				tu = ((int) (x * 128 / z)) & 0x7F;	/* x / * z * => * u */
				tv = ((int) (y * 128 / z)) & 0x7F;	/* y / * z * => * v */

				int t_offset =
				    (tv * tex->line_length) +
				    (tu * tex->bytes_per_pixel);

				unsigned int value = *((unsigned long *) (tex->data + t_offset));
				//if (value >> fb_data.buf->fmt.a.shift  > 127 ) {
					*(volatile unsigned long *) (fb_data.buf->data + ((fx * tex->bytes_per_pixel ) +
						(fy * fb_data.buf->line_length))) = *((unsigned long *) (tex->data +
						t_offset));
				//}
			}
		}
		while ( flip(&fb_data) ){
			sleep(.3);
			printf("reopen device \n");
			reopen_fb(&fb_data);
		}
	}

	close_fb(&fb_data);
	return 0;
}
