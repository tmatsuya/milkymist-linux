/*
 * (C) Copyright 2007
 *     Theobroma Systems <www.theobroma-systems.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Based on:
 * include/asm-m68k/checksum.h
 */

#ifndef _LM32_ASM_CHECKSUM_H
#define _LM32_ASM_CHECKSUM_H

#include <linux/in6.h>

/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
__wsum csum_partial(const void *buff, int len, __wsum sum);

/*
 * the same as csum_partial, but copies from src while it
 * checksums
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

__wsum csum_partial_copy_nocheck(const void *src, void *dst,
	int len, __wsum sum);


/*
 * the same as csum_partial_copy, but copies from user space.
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

extern __wsum csum_partial_copy_from_user(const void __user *src,
	void *dst, int len, __wsum sum, int *csum_err);

__sum16 ip_fast_csum(const void *iph, unsigned int ihl);

/*
 *	Fold a partial checksum
 */

static inline __sum16 csum_fold(__wsum sum)
{
#if 0
        while (sum >> 16)
                sum = (sum & 0xffff) + (sum >> 16);
        return ((~(sum << 16)) >> 16);
#endif

	/* This C function will generate is equivalent to the following
	   assembly output:

	   srui     r2, r1, 16
	   andi     r1, r1, 65535
	   add      r2, r1, r2
	   cmpgui   r1, r2, 0xffff
	   add      r2, r2, r1
	   xori     r2, r2, 0xffff
	*/
	   
        u32   lo = sum & 0xffff ;
        u32   hi = sum >> 16 ;
        u32   tmp;
        u32   carry;

        tmp = lo + hi;
     /* carry = tmp & 0x10000;    if bit 17 is set there was a
																	carry from adding 16 bit to 16 bit */
				carry = (tmp > 0xffff);
				asm volatile( "cmpgui %0, %1, 0xffff": "=r"(carry) : "r"(tmp) );
        tmp = tmp + carry;
        asm volatile ( "xori %0, %1, 0xffff" : "=r"(tmp) : "0"(tmp) );
        return tmp;
}


/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */

static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
		  unsigned short proto, __wsum sum)
{
  u32  s = sum;
  u32  tmp;
  u32  carry;

  /* add saddr */
  tmp   = s + saddr;
  carry = (s > tmp);
  s     = tmp + carry;
  /* add daddr */
  tmp   = s + daddr;
  carry = (s > tmp);
  s     = tmp + carry;
  /* addr len and proto */
  tmp   = s + (proto + len);  /* this is correct for big-endian only! */
  carry = (s > tmp);  /* this will generate a single cmpgu insn */
  s     = tmp + carry;

  return (__force __wsum) s;
}

static inline __sum16
csum_tcpudp_magic(__be32 saddr, __be32 daddr, unsigned short len,
		  unsigned short proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}

/*
 * this routine is used for miscellaneous IP-like checksums, mainly
 * in icmp.c
 */

extern __sum16 ip_compute_csum(const void *buff, int len);

//#define _HAVE_ARCH_IPV6_CSUM
//TODO activate _HAVE_ARCH_IPV6_CSUM

#endif
