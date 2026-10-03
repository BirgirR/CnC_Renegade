/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*********************************************************************************************** 
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               *** 
 *********************************************************************************************** 
 *                                                                                             * 
 *                 Project Name : Command & Conquer                                            * 
 *                                                                                             * 
 *                     $Archive:: /G/wwlib/lcw.cpp                                            $* 
 *                                                                                             * 
 *                      $Author:: Neal_k                                                      $*
 *                                                                                             * 
 *                     $Modtime:: 10/04/99 10:25a                                             $*
 *                                                                                             * 
 *                    $Revision:: 4                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 *   LCW_Comp -- Performes LCW compression on a block of data.                                 * 
 *   LCW_Uncomp -- Decompress an LCW encoded data block.                                       *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"always.h"
#include	"lcw.h"
#include	<string.h>

/***************************************************************************
 * LCW_Uncomp -- Decompress an LCW encoded data block.                     *
 *                                                                         *
 * Uncompress data to the following codes in the format b = byte, w = word *
 * n = byte code pulled from compressed data.                              *
 *                                                                         *
 *   Command code, n        |Description                                   *
 * ------------------------------------------------------------------------*
 * n=0xxxyyyy,yyyyyyyy      |short copy back y bytes and run x+3 from dest *
 * n=10xxxxxx,n1,n2,...,nx+1|med length copy the next x+1 bytes from source*
 * n=11xxxxxx,w1            |med copy from dest x+3 bytes from offset w1   *
 * n=11111111,w1,w2         |long copy from dest w1 bytes from offset w2   *
 * n=11111110,w1,b1         |long run of byte b1 for w1 bytes              *
 * n=10000000               |end of data reached                           *
 *                                                                         *
 *                                                                         *
 * INPUT:                                                                  *
 *      void * source ptr                                                  *
 *      void * destination ptr                                             *
 *      unsigned long length of uncompressed data                          *
 *                                                                         *
 *                                                                         *
 * OUTPUT:                                                                 *
 *     unsigned long # of destination bytes written                        *
 *                                                                         *
 * WARNINGS:                                                               *
 *     3rd argument is dummy. It exists to provide cross-platform          *
 *      compatibility. Note therefore that this implementation does not    *
 *      check for corrupt source data by testing the uncompressed length.  *
 *                                                                         *
 * HISTORY:                                                                *
 *    03/20/1995 IML : Created.                                            *
 *=========================================================================*/
int LCW_Uncomp(void const * source, void * dest, unsigned long )
{
	unsigned char * source_ptr, * dest_ptr, * copy_ptr;
	unsigned char op_code, data;
	unsigned count;
	unsigned * word_dest_ptr;
	unsigned word_data;

	/* Copy the source and destination ptrs. */
	source_ptr = (unsigned char*) source;
	dest_ptr   = (unsigned char*) dest;

	for (;;) {

		/* Read in the operation code. */
		op_code = *source_ptr++;

		if (!(op_code & 0x80)) {

			/* Do a short copy from destination. */
			count = (op_code >> 4) + 3;
			copy_ptr = dest_ptr - ((unsigned) *source_ptr++ + (((unsigned) op_code & 0x0f) << 8));

			while (count--) *dest_ptr++ = *copy_ptr++;

		} else {

			if (!(op_code & 0x40)) {

				if (op_code == 0x80) {

					/* Return # of destination bytes written. */
					return ((unsigned long) (dest_ptr - (unsigned char*) dest));

				} else {

					/* Do a medium copy from source. */
					count = op_code & 0x3f;

					while (count--) *dest_ptr++ = *source_ptr++;
				}

			} else {

				if (op_code == 0xfe) {

					/* Do a long run. */
					count = *source_ptr + ((unsigned) *(source_ptr + 1) << 8);
					word_data = data = *(source_ptr + 2);
					word_data  = (word_data << 24) + (word_data << 16) + (word_data << 8) + word_data;
					source_ptr += 3;

					copy_ptr = dest_ptr + 4 - ((unsigned) dest_ptr & 0x3);
					count -= (copy_ptr - dest_ptr);
					while (dest_ptr < copy_ptr) *dest_ptr++ = data;

					word_dest_ptr = (unsigned*) dest_ptr;

					dest_ptr += (count & 0xfffffffc);

					while (word_dest_ptr < (unsigned*) dest_ptr) {
						*word_dest_ptr		= word_data;
						*(word_dest_ptr + 1) = word_data;
						word_dest_ptr += 2;
					}

					copy_ptr = dest_ptr + (count & 0x3);
					while (dest_ptr < copy_ptr) *dest_ptr++ = data;

				} else {

					if (op_code == 0xff) {

						/* Do a long copy from destination. */
						count = *source_ptr + ((unsigned) *(source_ptr + 1) << 8);
						copy_ptr = (unsigned char*) dest + *(source_ptr + 2) + ((unsigned) *(source_ptr + 3) << 8);
						source_ptr += 4;

						while (count--) *dest_ptr++ = *copy_ptr++;

					} else {

						/* Do a medium copy from destination. */
						count = (op_code & 0x3f) + 3;
						copy_ptr = (unsigned char*) dest + *source_ptr + ((unsigned) *(source_ptr + 1) << 8);
						source_ptr += 2;

						while (count--) *dest_ptr++ = *copy_ptr++;
					}
				}
			}
		}
	}
}



/*********************************************************************************************** 
 * LCW_Comp -- Performes LCW compression on a block of data.                                   * 
 *                                                                                             * 
 *    This routine will compress a block of data using the LCW compression method. LCW has     * 
 *    the primary characteristic of very fast uncompression at the expense of very slow        * 
 *    compression times.                                                                       * 
 *                                                                                             * 
 * INPUT:   source   -- Pointer to the source data to compress.                                * 
 *                                                                                             * 
 *          dest     -- Pointer to the destination location to store the compressed data       * 
 *                      to.                                                                    * 
 *                                                                                             * 
 *          datasize -- The size (in bytes) of the source data to compress.                    * 
 *                                                                                             * 
 * OUTPUT:  Returns with the number of bytes of output data stored into the destination        * 
 *          buffer.                                                                            * 
 *                                                                                             * 
 * WARNINGS:   Be sure that the destination buffer is big enough. The maximum size required    * 
 *             for the destination buffer is (datasize + datasize/128).                        * 
 *                                                                                             * 
 * HISTORY:                                                                                    * 
 *   05/20/1997 JLB : Created.                                                                 * 
 *=============================================================================================*/
int LCW_Comp(void const * source, void * dest, int datasize)
{
	/*
	**	This was x86 assembly; it is the same algorithm in C, and produces the same bytes
	**	(Code/Tests/unit/test_asm_replacements.cpp checks that against the original). Its
	**	quirks are kept with it: offsets and run lengths are stored as 16 bits, so input
	**	past 64K does not survive the round trip; of equally long matches the later one
	**	wins; and the history is searched from the start of the data every time.
	**
	**	The one difference is input of a single byte, where the assembly went on to read
	**	past the end of the source. Here it is one literal and the end code.
	**
	**	Compressed data is built from these codes (b = byte, w = word, n = code byte):
	**		n=0xxxyyyy,yyyyyyyy		short run	back y bytes and run x+3
	**		n=10xxxxxx,n1,n2,...,nx+1	med length	copy the next x+1 bytes
	**		n=11xxxxxx,w1			med run		run x+3 bytes from offset w1
	**		n=11111110,w1,b1		fill		run w1 bytes of b1
	**		n=11111111,w1,w2		long run	run w1 bytes from offset w2
	**		n=10000000			end		end of data reached
	*/
	unsigned char const * const first = (unsigned char const *)source;
	unsigned char const * const end_of_data = first + datasize;
	unsigned char const * src = first;
	unsigned char * const first_dest = (unsigned char *)dest;
	unsigned char * dst = first_dest;

	if (datasize > 0) {

		/*
		**	The first byte is always a literal run of one.
		*/
		unsigned char * lenoff = dst;
		bool inlen = true;
		*dst++ = 0x81;
		*dst++ = *src++;

		while (src < end_of_data) {

			/*
			**	A run of 65 or more of one byte is written as a fill. The test against
			**	the byte 64 ahead is only a quick reject; the assembly made it even past
			**	the end of the data, but a run that short could never reach 65.
			*/
			while (src + 64 < end_of_data && src[0] == src[64]) {
				unsigned char const * run = src;
				while (run < end_of_data && *run == *src) {
					run++;
				}
				if (run == end_of_data) {
					run--;			// a run to the very end stops one short of it
				}
				unsigned long runlen = (unsigned long)(run - src);
				if (runlen < 65) break;

				*dst++ = 0xFE;
				*dst++ = (unsigned char)runlen;
				*dst++ = (unsigned char)(runlen >> 8);
				*dst++ = *src;
				src = run;
				inlen = false;
			}

			/*
			**	Find the longest earlier match for the data at src.
			*/
			unsigned long count = 1;
			unsigned char const * matchoff = NULL;
			for (unsigned char const * search = first; search < src; ) {
				unsigned char const * candidate = (unsigned char const *)memchr(search, *src, src - search);
				if (candidate == NULL) break;
				search = candidate + 1;

				if (src[count-1] != candidate[count-1]) continue;	// cannot beat the best so far

				unsigned long limit = (unsigned long)(end_of_data - src);
				unsigned long matched = 0;
				while (matched < limit && src[matched] == candidate[matched]) {
					matched++;
				}
				if (matched >= count) {
					count = matched;
					matchoff = candidate;
				}
			}

			if (count <= 2) {

				/*
				**	Too short to be worth a run: add the byte to a literal run, starting a
				**	new one if none is open or the open one is full (63 bytes).
				*/
				if (!inlen || *lenoff == 0xBF) {
					lenoff = dst;
					*dst++ = 0x80;
				}
				(*lenoff)++;
				*dst++ = *src++;
				inlen = true;
				continue;
			}

			unsigned long distance = (unsigned long)(src - matchoff);
			if (count <= 10 && distance <= 0xFFF) {
				*dst++ = (unsigned char)(((count - 3) << 4) + (distance >> 8));
				*dst++ = (unsigned char)distance;
			} else {
				if (count <= 64) {
					*dst++ = (unsigned char)(0xC0 | (count - 3));
				} else {
					*dst++ = 0xFF;
					*dst++ = (unsigned char)count;
					*dst++ = (unsigned char)(count >> 8);
				}
				unsigned long offset = (unsigned long)(matchoff - first);
				*dst++ = (unsigned char)offset;
				*dst++ = (unsigned char)(offset >> 8);
			}
			src += count;
			inlen = false;
		}
	}

	*dst++ = 0x80;		// end of data
	return(int)(dst - first_dest);
}


