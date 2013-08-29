#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/fb.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <minix/log.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>
#include <dev/videomode/edidreg.h>
#include "am335x.h"
#include "dm37xx.h"
#include "fb.h"

int arch_fb_setup(struct arch_fb *arch_fb) {

#ifdef DM37XX
	arch_fb->fb_init = dm37xx_fb_init;
	arch_fb->get_device = dm37xx_get_device;
	arch_fb->get_varscreeninfo = dm37xx_get_varscreeninfo;
	arch_fb->put_varscreeninfo = dm37xx_put_varscreeninfo;
	arch_fb->get_fixscreeninfo = dm37xx_get_fixscreeninfo;
	arch_fb->pan_display = dm37xx_pan_display;
	arch_fb->intr_handler = NULL;
#elif AM335X
	arch_fb->fb_init = am335x_fb_init;
	arch_fb->get_device = am335x_get_device;
	arch_fb->get_varscreeninfo = am335x_get_varscreeninfo;
	arch_fb->put_varscreeninfo = am335x_put_varscreeninfo;
	arch_fb->get_fixscreeninfo = am335x_get_fixscreeninfo;
	arch_fb->pan_display = am335x_pan_display;
	arch_fb->intr_handler = am335x_intr_handler;
#endif

	return OK;
}

