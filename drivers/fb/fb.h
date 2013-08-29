#ifndef __FB_H__
#define __FB_H__

#include <minix/fb.h>
#include <stdint.h>                                                             
#include <dev/videomode/videomode.h>                                            
#include <dev/videomode/edidvar.h>                                              
#include <dev/videomode/edidreg.h>

struct arch_fb {
	int (*fb_init)(int minor, struct device *dev, struct edid_info *info);
	int (*get_device)(int minor, struct device *dev);
	int (*get_varscreeninfo)(int minor, struct fb_var_screeninfo *fbvsp);
	int (*put_varscreeninfo)(int minor, struct fb_var_screeninfo *fbvs_copy);
	int (*get_fixscreeninfo)(int minor, struct fb_fix_screeninfo *fbfsp);
	int (*pan_display)(int minor, struct fb_var_screeninfo *fbvs_copy);
	int (*intr_handler)(void);
};

int arch_fb_setup(struct arch_fb *arch_fb);

#define FB_MESSAGE "Hello, world! From framebuffer!\n"
#define FB_DEV_NR 1

#endif /* __FB_H__ */
