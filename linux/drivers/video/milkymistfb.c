/*
 *      linux/drivers/video/milkymistfb.c
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fb.h>
//#include <video/milkymistfb.h>

#define MILKYMIST_FBMEM_START	(0x43000000)

#define MMPTR(x)		(*((volatile unsigned int *)(x)))
#define CSR_VGA_RESET		MMPTR(0x80003000)

#define VGA_RESET		(0x01)

#define CSR_VGA_HRES		MMPTR(0x80003004)
#define CSR_VGA_HSYNC_START	MMPTR(0x80003008)
#define CSR_VGA_HSYNC_END	MMPTR(0x8000300C)
#define CSR_VGA_HSCAN		MMPTR(0x80003010)

#define CSR_VGA_VRES		MMPTR(0x80003014)
#define CSR_VGA_VSYNC_START	MMPTR(0x80003018)
#define CSR_VGA_VSYNC_END	MMPTR(0x8000301C)
#define CSR_VGA_VSCAN		MMPTR(0x80003020)

#define CSR_VGA_BASEADDRESS	MMPTR(0x80003024)
#define CSR_VGA_BASEADDRESS_ACT	MMPTR(0x80003028)

#define CSR_VGA_BURST_COUNT	MMPTR(0x8000302C)


/* bootinfo.h defines the machine type values, needed when checking */
/* whether are really running on a milkymist, KM                       */
//#include <asm/bootinfo.h>

static struct fb_info fb_info;

static struct fb_var_screeninfo milkymistfb_defined = {
	.xres =		640,
	.yres =		480,
	.xres_virtual =	640,
	.yres_virtual =	480,
	.bits_per_pixel =16,
	.red =		{ 11, 5, 0 },
	.green =	{  5, 6, 0 },
	.blue =		{  0, 5, 0 },
	.activate =	FB_ACTIVATE_NOW,
	.height =	-1,
	.width =	-1,
	.vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo milkymistfb_fix = {
	.id =		"Milkymist",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.smem_len =	(640*480*2),
	.line_length =	480,
	.accel =	FB_ACCEL_NONE,
};

/* Set the palette */
static int milkymistfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			      unsigned blue, unsigned transp, struct fb_info *info)
{
	/* value to be written into the palette reg. */
	unsigned long hw_colorvalue = 0;

	red   >>= 8;    /* The cmap fields are 16 bits    */
	green >>= 8;    /* wide, but the harware colormap */
	blue  >>= 8;    /* registers are only 8 bits wide */

	hw_colorvalue = (blue << 16) + (green << 8) + (red);

	return 0;
}

static struct fb_ops milkymistfb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= milkymistfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

int __init milkymistfb_init(void)
{
	unsigned long fboff;
	unsigned long fb_start;
	int i;

	if (fb_get_options("milkymistfb", NULL))
		return -ENODEV;

#if 0
	/* Validate we're on the proper machine type */
	if (mips_machtype != MACH_DS5000_XX) {
		return -EINVAL;
	}
#endif

	printk(KERN_INFO "Milkymistfb: Milkymist system detected\n");
	printk(KERN_INFO "Milkymistfb: initializing onboard framebuffer\n");

	/* Framebuffer display memory base address */
	fb_start = MILKYMIST_FBMEM_START;

	/* Clear screen */
	for (fboff = fb_start; fboff < fb_start + 640*480*2; fboff+=2)
		*(volatile unsigned short *)fboff = 0x000;

	milkymistfb_fix.smem_start = fb_start;
	CSR_VGA_BASEADDRESS = fb_start;
	

	fb_info.fbops = &milkymistfb_ops;
	fb_info.screen_base = (char *)milkymistfb_fix.smem_start;
	fb_info.var = milkymistfb_defined;
	fb_info.fix = milkymistfb_fix;
	fb_info.flags = FBINFO_DEFAULT;

	fb_alloc_cmap(&fb_info.cmap, 256, 0);

	if (register_framebuffer(&fb_info) < 0)
		return 1;
	return 0;
}

static void __exit milkymistfb_exit(void)
{
	unregister_framebuffer(&fb_info);
}

#ifdef MODULE
MODULE_LICENSE("GPL");
#endif
module_init(milkymistfb_init);
module_exit(milkymistfb_exit);

