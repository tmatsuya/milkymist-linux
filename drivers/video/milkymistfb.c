/*
 *  linux/drivers/video/milkymistfb.c -- Milkymist frame buffer device
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>

    /*
     *  RAM we reserve for the frame buffer. This defines the maximum screen
     *  size
     *
     *  The default can be overridden if the driver is compiled as a module
     */

#define VIDEOMEMSIZE	(1024*800*2)	/* 1.6 MB */

#define	MMPTR(x)		(*((volatile unsigned int *)(x)))
#define	CSR_VGA_RESET		MMPTR(0x80003000)

#define	VGA_RESET		(0x01)

#define	CSR_VGA_HRES		MMPTR(0x80003004)
#define	CSR_VGA_HSYNC_START	MMPTR(0x80003008)
#define	CSR_VGA_HSYNC_END	MMPTR(0x8000300C)
#define	CSR_VGA_HSCAN		MMPTR(0x80003010)

#define	CSR_VGA_VRES		MMPTR(0x80003014)
#define	CSR_VGA_VSYNC_START	MMPTR(0x80003018)
#define	CSR_VGA_VSYNC_END	MMPTR(0x8000301C)
#define	CSR_VGA_VSCAN		MMPTR(0x80003020)

#define	CSR_VGA_BASEADDRESS	MMPTR(0x80003024)
#define	CSR_VGA_BASEADDRESS_ACT	MMPTR(0x80003028)

#define	CSR_VGA_BURST_COUNT	MMPTR(0x8000302C)

#define	CSR_VGA_SOURCE_CLOCK	MMPTR(0x80003030)
#define	VGA_CLOCK_VGA		(0x00)
#define	VGA_CLOCK_SVGA		(0x01)
#define	VGA_CLOCK_XGA		(0x02)

/* TODO: move these into the driver private structure (info->par) */
static void *videomemory;
static u_long videomemorysize = VIDEOMEMSIZE;
module_param(videomemorysize, ulong, 0);

static int already_mmaped = 0;
static unsigned milkymistfb_def_mode = 1;

/*
 *    Predefine Video Modes
 */
struct csr_vga {
	unsigned int csr_vga_hres;
	unsigned int csr_vga_hsync_start;
	unsigned int csr_vga_hsync_end;
	unsigned int csr_vga_hscan;
	unsigned int csr_vga_vres;
	unsigned int csr_vga_vsync_start;
	unsigned int csr_vga_vsync_end;
	unsigned int csr_vga_vscan;
	unsigned int csr_vga_source_clock;
};

static const struct {
	const char *name;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	struct csr_vga vga;
} milkymistfb_predefined[] = {
	{
		/* autodetect mode */
		.name	= "Autodetect",
	}, {
		/* 640x480, 31.29, KHz 59.8 Hz, 25 MHz PixClock */
		.name	= "640x480",
		.var	= {
			.xres =		640,
			.yres =		480,
			.xres_virtual =	640,
			.yres_virtual =	480,
			.bits_per_pixel = 16,
			.red =		{11, 5, 0 },
   		   	.green =	{ 5, 6, 0 },
 		     	.blue =		{ 0, 5, 0 },
  		    	.activate =	FB_ACTIVATE_NOW,
		      	.height =	-1,
		      	.width =	-1,
      			.vmode =	FB_VMODE_NONINTERLACED,
		},
		.fix	= {
			.id =		"MilkymistFB",
			.type =		FB_TYPE_PACKED_PIXELS,
			.visual =	FB_VISUAL_TRUECOLOR,
			.smem_len = 	(640*480*2),
			.line_length =	640*2,
			.accel =	FB_ACCEL_NONE,
		},
		.vga= {
			.csr_vga_hres =	640,
			.csr_vga_hsync_start = 656,
			.csr_vga_hsync_end = 752,
			.csr_vga_hscan = 799,
			.csr_vga_vres = 480,
			.csr_vga_vsync_start = 491,
			.csr_vga_vsync_end = 493,
			.csr_vga_vscan = 523,
			.csr_vga_source_clock = VGA_CLOCK_VGA,
		}
	}, {
		/* 800x600, 48 KHz, 72.2 Hz, 50 MHz PixClock */
		.name	= "800x600",
		.var	= {
			.xres =		800,
			.yres =		600,
			.xres_virtual =	800,
			.yres_virtual =	600,
			.bits_per_pixel = 16,
			.red =		{11, 5, 0 },
   		   	.green =	{ 5, 6, 0 },
 		     	.blue =		{ 0, 5, 0 },
  		    	.activate =	FB_ACTIVATE_NOW,
		      	.height =	-1,
		      	.width =	-1,
      			.vmode =	FB_VMODE_NONINTERLACED,
		},
		.fix	= {
			.id =		"MilkymistFB",
			.type =		FB_TYPE_PACKED_PIXELS,
			.visual =	FB_VISUAL_TRUECOLOR,
			.smem_len = 	(800*600*2),
			.line_length =	800*2,
			.accel =	FB_ACCEL_NONE,
		},
		.vga= {
			.csr_vga_hres =	800,
			.csr_vga_hsync_start = 848,
			.csr_vga_hsync_end = 976,
			.csr_vga_hscan = 1040,
			.csr_vga_vres = 600,
			.csr_vga_vsync_start = 637,
			.csr_vga_vsync_end = 643,
			.csr_vga_vscan = 666,
			.csr_vga_source_clock = VGA_CLOCK_SVGA,
		}
	}, {
		/* 1024x768, 48.363 KHz, 60 Hz, 65 MHz PixClock */
		.name	= "1024x768",
		.var	= {
			.xres =		1024,
			.yres =		768,
			.xres_virtual =	1024,
			.yres_virtual =	768,
			.bits_per_pixel = 16,
			.red =		{11, 5, 0 },
   		   	.green =	{ 5, 6, 0 },
 		     	.blue =		{ 0, 5, 0 },
  		    	.activate =	FB_ACTIVATE_NOW,
		      	.height =	-1,
		      	.width =	-1,
      			.vmode =	FB_VMODE_NONINTERLACED,
		},
		.fix	= {
			.id =		"MilkymistFB",
			.type =		FB_TYPE_PACKED_PIXELS,
			.visual =	FB_VISUAL_TRUECOLOR,
			.smem_len = 	(1024*768*2),
			.line_length =	1024*2,
			.accel =	FB_ACCEL_NONE,
		},
		.vga= {
			.csr_vga_hres =	1024,
			.csr_vga_hsync_start = 1040,
			.csr_vga_hsync_end = 1184,
			.csr_vga_hscan = 1344,
			.csr_vga_vres = 768,
			.csr_vga_vsync_start = 771,
			.csr_vga_vsync_end = 777,
			.csr_vga_vscan = 806,
			.csr_vga_source_clock = VGA_CLOCK_XGA,
		}
	}
};

#define	NUM_TOTAL_MODES	ARRAY_SIZE(milkymistfb_predefined)
 
static int milkymistfb_enable __initdata = 0;	/* disabled by default */
module_param(milkymistfb_enable, bool, 0);

static int milkymistfb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info);
static int milkymistfb_set_par(struct fb_info *info);
static int milkymistfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info);
static int milkymistfb_pan_display(struct fb_var_screeninfo *var,
			   struct fb_info *info);
static int milkymistfb_mmap(struct fb_info *info,
		    struct vm_area_struct *vma);
static int milkymistfb_release(struct fb_info *info, int user);

static struct fb_ops milkymistfb_ops = {
	.owner		= THIS_MODULE,
	.fb_read	= fb_sys_read,
	.fb_write	= fb_sys_write,
	.fb_release	= milkymistfb_release,
	.fb_check_var	= milkymistfb_check_var,
	.fb_set_par	= milkymistfb_set_par,
	.fb_setcolreg	= milkymistfb_setcolreg,
	.fb_pan_display	= milkymistfb_pan_display,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
	.fb_mmap	= milkymistfb_mmap,
};

    /*
     *  Internal routines
     */

static u_long get_line_length(int xres_virtual, int bpp)
{
	u_long length;

	length = xres_virtual * bpp;
	length = (length + 31) & ~31;
	length >>= 3;
	return (length);
}

    /*
     *  Setting the video mode has been split into two parts.
     *  First part, xxxfb_check_var, must not write anything
     *  to hardware, it should only verify and adjust var.
     *  This means it doesn't alter par but it does use hardware
     *  data from it to check this var. 
     */

static int milkymistfb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info)
{
	u_long line_length;

	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = info->var.xoffset;
		var->yoffset = info->var.yoffset;
	}

	/*
	 *  Some very basic checks
	 */
	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 1)
		var->bits_per_pixel = 1;
	else if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 24)
		var->bits_per_pixel = 24;
	else if (var->bits_per_pixel <= 32)
		var->bits_per_pixel = 32;
	else
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	/*
	 *  Memory limit
	 */
	line_length =
	    get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (line_length * var->yres_virtual > videomemorysize)
		return -ENOMEM;

	/*
	 * Now that we checked it we alter var. The reason being is that the video
	 * mode passed in might not work but slight changes to it might make it 
	 * work. This way we let the user know what is acceptable.
	 */
	switch (var->bits_per_pixel) {
	case 1:
	case 8:
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 16:		/* RGBA 5551 */
		if (var->transp.length) {
			var->red.offset = 0;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 5;
			var->blue.offset = 10;
			var->blue.length = 5;
			var->transp.offset = 15;
			var->transp.length = 1;
		} else {	/* RGB 565 */
			var->red.offset = 0;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 6;
			var->blue.offset = 11;
			var->blue.length = 5;
			var->transp.offset = 0;
			var->transp.length = 0;
		}
		break;
	case 24:		/* RGB 888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:		/* RGBA 8888 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	return 0;
}

/* This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the 
 * change in par. For this driver it doesn't do much. 
 */
static int milkymistfb_set_par(struct fb_info *info)
{
	info->fix.line_length = get_line_length(info->var.xres_virtual,
						info->var.bits_per_pixel);
	return 0;
}

    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */

static int milkymistfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info)
{
	if (regno >= 256)	/* no. of hw registers */
		return 1;
	/*
	 * Program hardware... do anything you want with transp
	 */

	/* grayscale works only partially under directcolor */
	if (info->var.grayscale) {
		/* grayscale = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue =
		    (red * 77 + green * 151 + blue * 28) >> 8;
	}

	/* Directcolor:
	 *   var->{color}.offset contains start of bitfield
	 *   var->{color}.length contains length of bitfield
	 *   {hardwarespecific} contains width of RAMDAC
	 *   cmap[X] is programmed to (X << red.offset) | (X << green.offset) | (X << blue.offset)
	 *   RAMDAC[X] is programmed to (red, green, blue)
	 * 
	 * Pseudocolor:
	 *    uses offset = 0 && length = RAMDAC register width.
	 *    var->{color}.offset is 0
	 *    var->{color}.length contains widht of DAC
	 *    cmap is not used
	 *    RAMDAC[X] is programmed to (red, green, blue)
	 * Truecolor:
	 *    does not use DAC. Usually 3 are present.
	 *    var->{color}.offset contains start of bitfield
	 *    var->{color}.length contains length of bitfield
	 *    cmap is programmed to (red << red.offset) | (green << green.offset) |
	 *                      (blue << blue.offset) | (transp << transp.offset)
	 *    RAMDAC does not exist
	 */
#define CNVT_TOHW(val,width) ((((val)<<(width))+0x7FFF-(val))>>16)
	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		red = CNVT_TOHW(red, info->var.red.length);
		green = CNVT_TOHW(green, info->var.green.length);
		blue = CNVT_TOHW(blue, info->var.blue.length);
		transp = CNVT_TOHW(transp, info->var.transp.length);
		break;
	case FB_VISUAL_DIRECTCOLOR:
		red = CNVT_TOHW(red, 8);	/* expect 8 bit DAC */
		green = CNVT_TOHW(green, 8);
		blue = CNVT_TOHW(blue, 8);
		/* hey, there is bug in transp handling... */
		transp = CNVT_TOHW(transp, 8);
		break;
	}
#undef CNVT_TOHW
	/* Truecolor has hardware independent palette */
	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 v;

		if (regno >= 16)
			return 1;

		v = (red << info->var.red.offset) |
		    (green << info->var.green.offset) |
		    (blue << info->var.blue.offset) |
		    (transp << info->var.transp.offset);
		switch (info->var.bits_per_pixel) {
		case 8:
			break;
		case 16:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		case 24:
		case 32:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		}
		return 0;
	}
	return 0;
}

    /*
     *  Pan or Wrap the Display
     *
     *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
     */

static int milkymistfb_pan_display(struct fb_var_screeninfo *var,
			   struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0
		    || var->yoffset >= info->var.yres_virtual
		    || var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + var->xres > info->var.xres_virtual ||
		    var->yoffset + var->yres > info->var.yres_virtual)
			return -EINVAL;
	}
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

/* Based on bf54x-lq043fb.c */
static int milkymistfb_mmap(struct fb_info *info,
		    struct vm_area_struct *vma)
{
	if (already_mmaped)
		return -1;
	already_mmaped = 1;

	vma->vm_start = (unsigned long)(videomemory);
	vma->vm_end = vma->vm_start + info->fix.smem_len;
	vma->vm_flags |=  VM_MAYSHARE | VM_SHARED;
	return 0;
}

static int milkymistfb_release(struct fb_info *info, int user)
{
	already_mmaped = 0;
	return 0;
}


#ifndef MODULE
static int __init milkymistfb_setup(char *options)
{
	char *this_opt, s[32];
	int i;

	milkymistfb_enable = 1;

	if (!options || !*options)
		return 1;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
		for (i=0; i<NUM_TOTAL_MODES; ++i) {
			sprintf(s, "mode:%s", milkymistfb_predefined[i].name);
			if (!strcmp(this_opt, s))
				milkymistfb_def_mode = i;
		}
		if (!strncmp(this_opt, "disable", 7))
			milkymistfb_enable = 0;
	}
	return 1;
}
#endif  /*  MODULE  */

    /*
     *  Initialisation
     */

static int __init milkymistfb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	struct csr_vga *vga;
	int retval = -ENOMEM;

	/*
	 * For real video cards we use ioremap.
	 */
	if (!(videomemory = vmalloc(videomemorysize)))
		return retval;

	/*
	 * VFB must clear memory to prevent kernel info
	 * leakage into userspace
	 * VGA-based drivers MUST NOT clear memory if
	 * they want to be able to take over vgacon
	 */
	memset(videomemory, 0, videomemorysize);

	info = framebuffer_alloc(sizeof(u32) * 256, &dev->dev);
	if (!info)
		goto err;

	info->screen_base = (char __iomem *)videomemory;
	info->fbops = &milkymistfb_ops;

	CSR_VGA_RESET = 1;
	vga = &milkymistfb_predefined[milkymistfb_def_mode].vga;
	CSR_VGA_SOURCE_CLOCK = vga->csr_vga_source_clock;
	if ( CSR_VGA_SOURCE_CLOCK != vga->csr_vga_source_clock ) {
		milkymistfb_def_mode = 1;
		vga = &milkymistfb_predefined[milkymistfb_def_mode].vga;
	}

	info->var = milkymistfb_predefined[milkymistfb_def_mode].var;
	info->fix = milkymistfb_predefined[milkymistfb_def_mode].fix;
	info->pseudo_palette = info->par;
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->fix.smem_start = (char *)videomemory;

	retval = fb_alloc_cmap(&info->cmap, 256, 0);
	if (retval < 0)
		goto err1;

	retval = register_framebuffer(info);
	if (retval < 0)
		goto err2;
	platform_set_drvdata(dev, info);

	printk(KERN_INFO
	       "fb%d: Milkymist frame buffer at %p, size %ld k\n",
	       info->node, videomemory, videomemorysize >> 10);
	printk(KERN_INFO
	       "fb%d: mode is %s\n",
		info->node,
		milkymistfb_predefined[milkymistfb_def_mode].name);

	CSR_VGA_BASEADDRESS = videomemory;
	CSR_VGA_HRES = vga->csr_vga_hres;
	CSR_VGA_HSYNC_START = vga->csr_vga_hsync_start;
	CSR_VGA_HSYNC_END = vga->csr_vga_hsync_end;
	CSR_VGA_HSCAN = vga->csr_vga_hscan;
	CSR_VGA_VRES = vga->csr_vga_vres;
	CSR_VGA_VSYNC_START = vga->csr_vga_vsync_start;
	CSR_VGA_VSYNC_END = vga->csr_vga_vsync_end;
	CSR_VGA_VSCAN = vga->csr_vga_vscan;
	CSR_VGA_BURST_COUNT = (vga->csr_vga_hres*vga->csr_vga_vres*16)/(4*64);
	CSR_VGA_RESET = 0;
       
	return 0;
err2:
	fb_dealloc_cmap(&info->cmap);
err1:
	framebuffer_release(info);
err:
	vfree(videomemory);
	return retval;
}

static int milkymistfb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	CSR_VGA_RESET = VGA_RESET;

	if (info) {
		unregister_framebuffer(info);
		vfree(videomemory);
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver milkymistfb_driver = {
	.probe	= milkymistfb_probe,
	.remove = milkymistfb_remove,
	.driver = {
		.name	= "milkymistfb",
	},
};

static struct platform_device *milkymistfb_device;

static int __init milkymistfb_init(void)
{
	int ret = 0;

#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("milkymistfb", &option))
		return -ENODEV;
	milkymistfb_setup(option);
#endif

	if (!milkymistfb_enable)
		return -ENXIO;

	ret = platform_driver_register(&milkymistfb_driver);

	if (!ret) {
		milkymistfb_device = platform_device_alloc("milkymistfb", 0);

		if (milkymistfb_device)
			ret = platform_device_add(milkymistfb_device);
		else
			ret = -ENOMEM;

		if (ret) {
			platform_device_put(milkymistfb_device);
			platform_driver_unregister(&milkymistfb_driver);
		}
	}

	return ret;
}

module_init(milkymistfb_init);

#ifdef MODULE
static void __exit milkymistfb_exit(void)
{
	platform_device_unregister(milkymistfb_device);
	platform_driver_unregister(&milkymistfb_driver);
}

module_exit(milkymistfb_exit);

MODULE_LICENSE("GPL");
#endif				/* MODULE */
