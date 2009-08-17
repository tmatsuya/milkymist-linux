/*
 * Copyright (C) 2005 Philippe Gerum <rpm@xenomai.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <stdio.h>
#include <native/syscall.h>
#include <native/intr.h>

extern int __native_muxid;

int rt_misc_get_io_region(unsigned long start,
			  unsigned long len, const char *label)
{
	return XENOMAI_SKINCALL3(__native_muxid,
				 __native_misc_get_io_region, start, len,
				 label);
}

int rt_misc_put_io_region(unsigned long start, unsigned long len)
{
	return XENOMAI_SKINCALL2(__native_muxid,
				 __native_misc_put_io_region, start, len);
}
