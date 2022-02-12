#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "nextfli.h"

static int rlelen[8192] = {};
static int lzlen[8192] = {};
static int litlen[8192] = {};

int decode_rleframe(unsigned char *buf, unsigned char *data, int datalen, int aWidth, int aHeight)
{
	int idx = 0;	
	for (int y = 0; y < aHeight; ++y) 
	{
		int out = aWidth * y;
		int x = 0;
		int packets = data[idx++];
		while (idx < datalen && packets-- != 0 && x < aWidth) 
		{
			int count = (signed char)(data[idx++]);
			if (count >= 0) 
			{
				unsigned char color = data[idx++];
				for (int i = 0; i < count && x < aWidth; i++)
				{
					buf[out++] = color;
					x++;
				}
			}
			else 
			{
				count = -count;
				for (int i = 0; i < count && x < aWidth; i++)
				{
					buf[out++] = data[idx++];
					x++;
				}
			}
		}
	}
	return idx;
}

int read16(unsigned char* data, int& idx)
{
	int v = 0;
	v |= data[idx++];
	v |= data[idx++] << 8;
	return v;
}

int decode_delta8frame(unsigned char* buf, unsigned char* data, int datalen, int aWidth, int aHeight)
{
	int idx = 0;
	int out = 0;
	int startrow = read16(data, idx);
	int rows = read16(data, idx);

	for (int y = startrow; y < startrow + rows; ++y)
	{
		// Break in case of invalid data
		if (y < 0 || y >= aHeight)
		{
			printf("invalid data (%d)\n", __LINE__);
			break;
		}

		out = aWidth * y;
		int x = 0;
		int packets = data[idx++];
		while (packets-- && x < aWidth) 
		{
			int skip = data[idx++];

			x += skip;
			out += skip;

			int count = (signed char)data[idx++];
			if (count == 0)
			{
				// nop
			}
			else if (count > 0) 
			{
				while (count-- != 0 && out < aWidth * aHeight)
				{
					buf[out++] = data[idx++];
					x++;
				}
				if (out >= aWidth * aHeight)
					return idx;
			}
			else 
			{
				unsigned char c = data[idx++];
				count = -count;
				for (int i = 0; x < aWidth && i < count; i++)
				{
					buf[out++] = c;
					x++;
				}
			}
		}
	}
	return idx;
}

int decode_delta16frame(unsigned char* buf, unsigned char* data, int datalen, int aWidth, int aHeight)
{
	int idx = 0;
	int out = 0;
	int lines = read16(data, idx);
	int y = 0;
	while (lines-- != 0) 
	{
		// Packet count is part of the command word below
		int packets = 0;

		while (idx < datalen) 
		{
			signed short word = (signed short)read16(data, idx);
			
			if (word < 0)
			{         
				// skip lines or update last pixel
				if (word & 0x4000) 
				{      
					y += -word;
				}				
				else 
				{
					if (y < 0 || y >= aHeight)
					{
						printf("Decode error (%d)\n", __LINE__);
					}
					if (y >= 0 && y < aHeight)
					{
						// update last pixel (never used with our encoder)
						buf[y * aWidth + aWidth - 1] = (word & 0xff);
						printf("Update last pixel of line - shouldn't happen (%d)\n", __LINE__);
					}
					y++;
					if (lines <= 0)
						return idx;
					lines--;
				}
			}
			else 
			{
				packets = word;
				if (word > 256)
				{
					printf("Excessive packets (%d)\n", __LINE__);
				}
				break;
			}
		}

		if (y >= aHeight)
		{
			printf("Encoding error (%d)\n", __LINE__);
			break;
		}

		int x = 0;
		while (packets-- != 0) 
		{
			// Skip byte
			x += data[idx++];
			// Command word count
			signed char count = (signed char)data[idx++]; 

			if (y < 0 || y >= aHeight || x < 0 || x >= aWidth)
			{
				printf("Decode error (%d)\n", __LINE__);
			}
			
			out = y * aWidth + x;

			if (count == 0)
			{
				// nop
			}
			else if (count > 0) 
			{
				for (int i = 0; x < aWidth && i < count; i++)
				{
					int a = data[idx++];
					int b = data[idx++];

					buf[out++] = a;
					x++;

					if (x < aWidth) 
					{
						buf[out++] = b;
						x++;
					}
				}
			}
			else 
			{
				int a = data[idx++];
				int b = data[idx++];
				count = -count;
				for (int i = 0; i != count && x < aWidth; i++)
				{
					buf[out++] = a;
					x++;

					if (x < aWidth)
					{
						buf[out++] = b;
						x++;
					}
				}
			}
		}
		y++;
	}
	return idx;
}

int decode_linearrle8(unsigned char *buf, unsigned char *data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runlen = data[idx++];
		int rundata = data[idx++];
		memset(buf + ofs, rundata, runlen);
		ofs += runlen;
		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen > 0)
			{
				memcpy(buf + ofs, data + idx, copylen);
				ofs += copylen;
				idx += copylen;
			}
			else
			{
				rundata = data[idx++];
				copylen = -copylen;
				memset(buf + ofs, rundata, copylen);
				ofs += copylen;
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer Lrle8 %d %d\n", ofs, pixels);
	return idx;
}

int decode_linearrle16(unsigned char* buf, unsigned char* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int copylen = (signed char)data[idx++];
		if (copylen >= 0)
		{
			memcpy(buf + ofs, data + idx, copylen);
			ofs += copylen;
			idx += copylen;
		}
		else
		{
			if (copylen == -128)
			{
				copylen = data[idx++];
				copylen |= data[idx++] << 8;
			}
			else
			{
				copylen = -copylen;
			}
			int rundata = data[idx++];

			memset(buf + ofs, rundata, copylen);
			ofs += copylen;
		}
	}
	if (ofs > pixels) printf("wrote over buffer Lrle8 %d %d\n", ofs, pixels);
	return idx;
}

int decode_lineardelta8(unsigned char* buf, unsigned char *prev, unsigned char* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen < 0)
		{
			runlen = -runlen;
			memcpy(buf + ofs, prev + ofs, runlen);
			ofs += runlen;
		}
		else
		{
			int color = data[idx++];
			memset(buf + ofs, color, runlen);
			ofs += runlen;
		}

		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen > 0)
			{
				memcpy(buf + ofs, data + idx, copylen);
				ofs += copylen;
				idx += copylen;
			}
			else
			{
				copylen = -copylen;
				memcpy(buf + ofs, prev + ofs, copylen);
				ofs += copylen;
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer Ldelta8 %d %d\n", ofs, pixels);
	return idx;
}

int decode_lineardelta16(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen < 0)
		{
			if (runlen == -128)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			else
			{
				runlen = -runlen;
			}
			memcpy(buf + ofs, prev + ofs, runlen);
			ofs += runlen;
		}
		else
		{
			if (runlen == 127)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			int color = data[idx++];
			memset(buf + ofs, color, runlen);
			ofs += runlen;
		}

		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen > 0)
			{
				memcpy(buf + ofs, data + idx, copylen);
				ofs += copylen;
				idx += copylen;
			}
			else
			{
				if (copylen == -128)
				{
					copylen = data[idx++];
					copylen += data[idx++] << 8;
				}
				else
				{
					copylen = -copylen;
				}
				memcpy(buf + ofs, prev + ofs, copylen);
				ofs += copylen;
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer Ldelta16 %d %d\n", ofs, pixels);
	return idx;
}

int decode_lz1(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runofs = data[idx++];
		runofs |= data[idx++] << 8;
		int runlen = data[idx++];
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs++] = prev[runofs++];
		}
		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen >= 0)
			{
				for (int i = 0; i < copylen; i++)
				{
					buf[ofs++] = data[idx++];
				}
			}
			else
			{
				unsigned char c = data[idx++];
				copylen = -copylen;
				for (int i = 0; i < copylen; i++)
				{
					buf[ofs++] = c;
				}
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz1 %d %d\n", ofs, pixels);
	return idx;
}

int decode_lz2(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runofs = data[idx++];
		runofs |= data[idx++] << 8;
		int runlen = data[idx++];
		runlen |= data[idx++] << 8;
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs++] = prev[runofs++];
		}
		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen >= 0)
			{
				for (int i = 0; i < copylen; i++)
				{
					buf[ofs++] = data[idx++];
				}
			}
			else
			{
				unsigned char c = data[idx++];
				copylen = -copylen;
				for (int i = 0; i < copylen; i++)
				{
					buf[ofs++] = c;
				}
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz2 %d %d\n", ofs, pixels);
	return idx;
}

int decode_lz1b(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels, FILE* lf = 0)
{
	int idx = 0;
	int ofs = 0;
	int spans = 0, lz1 = 0, rle = 0, lit = 0, lz2 = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen > 0)
		{
			int runofs = data[idx++];
			runofs |= data[idx++] << 8;
			memcpy(buf + ofs, prev + runofs, runlen);
			ofs += runlen;
			spans++; lz1++;
			lzlen[runlen < 8192 ? runlen : 8191]++;
		}
		else
		{
			runlen = -runlen;
			unsigned char c = data[idx++];
			memset(buf + ofs, c, runlen);
			ofs += runlen;
			spans++; rle++;
			rlelen[runlen < 8192 ? runlen : 8191]++;
		}

		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen >= 0)
			{
				memcpy(buf + ofs, data + idx, copylen);
				ofs += copylen;
				idx += copylen;
				spans++; lit++;
				litlen[copylen < 8192 ? copylen : 8191]++;
			}
			else
			{
				int runlen = -copylen;
				int runofs = data[idx++];
				runofs |= data[idx++] << 8;
				memcpy(buf + ofs, prev + runofs, runlen);
				ofs += runlen;
				spans++; lz2++;
				lzlen[runlen < 8192 ? runlen : 8191]++;
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz1b %d %d\n", ofs, pixels);
	if (lf) fprintf(lf, "spans:%5d lz1:%5d rle:%5d lit:%5d lz2:%5d", spans, lz1, rle, lit, lz2);
	return idx;
}

int decode_lz2b(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen < 0)
		{
			runlen <<= 8;
			runlen |= data[idx++];
			runlen = -runlen;
			int runofs = data[idx++];
			runofs |= data[idx++] << 8;
			for (int i = 0; i < runlen; i++)
			{
				buf[ofs++] = prev[runofs++];
			}
		}
		else
		{
			unsigned char c = data[idx++];
			for (int i = 0; i < runlen; i++)
			{
				buf[ofs++] = c;
			}
		}
		if (ofs < pixels)
		{
			int copylen = data[idx++];
			for (int i = 0; i < copylen; i++)
			{
				buf[ofs++] = data[idx++];
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz2 %d %d\n", ofs, pixels);
	return idx;
}

int decode_lz3(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen > 0)
		{
			int runofs = ((signed char)data[idx++]) + ofs;
			for (int i = 0; i < runlen; i++)
			{
				buf[ofs++] = prev[runofs++];
			}
		}
		else
		{
			runlen = -runlen;
			unsigned char c = data[idx++];
			for (int i = 0; i < runlen; i++)
			{
				buf[ofs++] = c;
			}
		}

		if (ofs < pixels)
		{
			int runlen = (signed char)data[idx++];
			
			if (runlen > 0)
			{
				int runofs = ((signed char)data[idx++]) + ofs;
				for (int i = 0; i < runlen; i++)
				{
					buf[ofs++] = prev[runofs++];
				}
			}
			else
			{
				runlen = -runlen;
				for (int i = 0; i < runlen; i++)
				{
					buf[ofs++] = data[idx++];
				}
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz3 %d %d\n", ofs, pixels);
	return idx;
}

void mymemcpy(unsigned char* dst, unsigned char* src, int count)
{
	for (int i = 0; i < count; i++)
		*dst++ = *src++;
}

int decode_lz4(unsigned char* buf, unsigned char* data, int datasize, int pixels, FILE* lf = 0)
{
	int idx = 0;
	int ofs = 0;
	int spans = 0, lz1 = 0, rle = 0, lit = 0, lz2 = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen > 0)
		{
			if (runlen == 127)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			int runofs = data[idx++];
			runofs |= data[idx++] << 8;
			mymemcpy(buf + ofs, buf + runofs, runlen);
			
			ofs += runlen;

			spans++; lz1++;
			lzlen[runlen < 8192 ? runlen : 8191]++;
		}
		else
		{
			if (runlen == -128)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			else
			{
				runlen = -runlen;
			}
			unsigned char c = data[idx++];
			memset(buf + ofs, c, runlen);
			ofs += runlen;
			spans++; rle++;
			rlelen[runlen < 8192 ? runlen : 8191]++;
		}

		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen >= 0)
			{
				memcpy(buf + ofs, data + idx, copylen);
				ofs += copylen;
				idx += copylen;
				spans++; lit++;
				litlen[copylen < 8192 ? copylen : 8191]++;
			}
			else
			{
				if (copylen == -128)
				{
					copylen = data[idx++];
					copylen += data[idx++] << 8;
				}
				else
				{
					copylen = -copylen;
				}
				int copyofs = data[idx++];
				copyofs |= data[idx++] << 8;
				mymemcpy(buf + ofs, buf + copyofs, copylen);
				ofs += copylen;
				spans++; lz2++;
				lzlen[copylen < 8192 ? copylen : 8191]++;
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz4 %d %d\n", ofs, pixels);
	if (lf) fprintf(lf, "spans:%5d lz1:%5d rle:%5d lit:%5d lz2:%5d", spans, lz1, rle, lit, lz2);
	return idx;
}

int decode_lz5(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels, FILE* lf = 0)
{
	/*
; op <=0 [-op][runvalue] or [-128][2 byte size][runvalue] - RLE
; op > 0 [op][2 byte offset] or [127][2 byte size][2 byte offset] - Copy from previous frame
; op < 0 [-op][2 byte offset] or [-128][2 byte size][2 byte offset] - Copy from current frame
; op >=0 [op][literal bytes] or [127][2 byte size][literal bytes] - Copy literal values
*/
	int idx = 0;
	int ofs = 0;
	int spans = 0, lz1 = 0, rle = 0, lit = 0, lz2 = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen > 0)
		{
			if (runlen == 127)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			int runofs = data[idx++];
			runofs |= data[idx++] << 8;
			mymemcpy(buf + ofs, prev + runofs, runlen);

			ofs += runlen;

			spans++; lz1++;
			lzlen[runlen < 8192 ? runlen : 8191]++;
		}
		else
		{
			if (runlen == -128)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			else
			{
				runlen = -runlen;
			}
			unsigned char c = data[idx++];
			memset(buf + ofs, c, runlen);
			ofs += runlen;
			spans++; rle++;
			rlelen[runlen < 8192 ? runlen : 8191]++;
		}

		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen >= 0)
			{
				if (copylen == 127)
				{
					copylen = data[idx++];
					copylen |= data[idx++] << 8;
				}
				memcpy(buf + ofs, data + idx, copylen);
				ofs += copylen;
				idx += copylen;
				spans++; lit++;
				litlen[copylen < 8192 ? copylen : 8191]++;
			}
			else
			{
				if (copylen == -128)
				{
					copylen = data[idx++];
					copylen += data[idx++] << 8;
				}
				else
				{
					copylen = -copylen;
				}
				int copyofs = data[idx++];
				copyofs |= data[idx++] << 8;
				mymemcpy(buf + ofs, prev + copyofs, copylen);
				ofs += copylen;
				spans++; lz2++;
				lzlen[copylen < 8192 ? copylen : 8191]++;
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz5 %d %d\n", ofs, pixels);
	if (lf) fprintf(lf, "spans:%5d lz1:%5d rle:%5d lit:%5d lz2:%5d", spans, lz1, rle, lit, lz2);
	return idx;
}

int decode_lz6(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels, FILE* lf = 0)
{
	/*
; op <=0 [-op][runvalue] or [-128][2 byte size][runvalue] - RLE
; op > 0 [op][2 byte offset] or [127][2 byte size][2 byte offset] - Copy from previous frame
; op < 0 [-op][2 byte offset] or [-128][2 byte size][2 byte offset] - Copy from current frame
; op >=0 [op][literal bytes] or [127][2 byte size][literal bytes] - Copy literal values
	*/
	int idx = 0;
	int ofs = 0;
	int spans = 0, lz1 = 0, rle = 0, lit = 0, lz2 = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen > 0)
		{
			if (runlen == 127)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			int runofs = data[idx++];
			runofs |= data[idx++] << 8;
			mymemcpy(buf + ofs, prev + runofs, runlen);

			ofs += runlen;

			spans++; lz1++;
			lzlen[runlen < 8192 ? runlen : 8191]++;
		}
		else
		{
			if (runlen == -128)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			else
			{
				runlen = -runlen;
			}
			unsigned char c = data[idx++];
			memset(buf + ofs, c, runlen);
			ofs += runlen;
			spans++; rle++;
			rlelen[runlen < 8192 ? runlen : 8191]++;
		}

		if (ofs < pixels)
		{
			int copylen = (signed char)data[idx++];
			if (copylen >= 0)
			{
				if (copylen == 127)
				{
					copylen = data[idx++];
					copylen |= data[idx++] << 8; 
				}
				memcpy(buf + ofs, data + idx, copylen);
				ofs += copylen;
				idx += copylen;
				spans++; lit++;
				litlen[copylen < 8192 ? copylen : 8191]++;
			}
			else
			{
				if (copylen == -128)
				{
					copylen = data[idx++];
					copylen += data[idx++] << 8;
				}
				else
				{
					copylen = -copylen;
				}
				int copyofs = data[idx++];
				copyofs |= data[idx++] << 8;
				mymemcpy(buf + ofs, buf + copyofs, copylen);
				ofs += copylen;
				spans++; lz2++;
				lzlen[copylen < 8192 ? copylen : 8191]++;
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz6 %d %d\n", ofs, pixels);
	if (lf) fprintf(lf, "spans:%5d lz1:%5d rle:%5d lit:%5d lz2:%5d", spans, lz1, rle, lit, lz2);
	return idx;
}

int decode_lz3c(unsigned char* buf, unsigned char* prev, unsigned char* data, int datasize, int pixels, FILE * lf = 0)
{
	/*
; op >  0 [127][2 byte len] or [op][current ofs +/- signed byte] copy from previous
; op <= 0 [-128][2 byte len] or [-op][run byte]
; op >  0 [127][2 byte len] or [op][current ofs +/- signed byte] copy from previous
; op <= 0 [-128][2 byte len] or [-op][.. bytes ..]
	*/
	int idx = 0;
	int ofs = 0;
	int spans = 0, lz1 = 0, rle = 0, lz2 = 0, lit = 0;
	while (ofs < pixels)
	{
		int runlen = (signed char)data[idx++];
		if (runlen > 0)
		{
			if (runlen == 127)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			int runofs = ofs + (signed char)data[idx++];
			mymemcpy(buf + ofs, prev + runofs, runlen);

			ofs += runlen;
			spans++; lz1++;
			lzlen[runlen < 8192 ? runlen : 8191]++;
		}
		else
		{
			if (runlen == -128)
			{
				runlen = data[idx++];
				runlen += data[idx++] << 8;
			}
			else
			{
				runlen = -runlen;
			}
			unsigned char c = data[idx++];
			memset(buf + ofs, c, runlen);
			ofs += runlen;
			spans++; rle++;
			rlelen[runlen < 8192 ? runlen : 8191]++;
		}

		if (ofs < pixels)
		{
			int runlen = (signed char)data[idx++];
			if (runlen > 0)
			{
				if (runlen == 127)
				{
					runlen = data[idx++];
					runlen += data[idx++] << 8;
				}
				int runofs = ofs + (signed char)data[idx++];
				mymemcpy(buf + ofs, prev + runofs, runlen);

				ofs += runlen;
				spans++; lz2++;
				lzlen[runlen < 8192 ? runlen : 8191]++;
			}
			else
			{
				if (runlen == -128)
				{
					runlen = data[idx++];
					runlen += data[idx++] << 8;
				}
				else
				{
					runlen = -runlen;
				}
				memcpy(buf + ofs, data + idx, runlen);
				ofs += runlen;
				idx += runlen;
				spans++; lit++;
				litlen[runlen < 8192 ? runlen : 8191]++;
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz3c %d %d\n", ofs, pixels);
	if (lf) fprintf(lf, "spans:%5d lz1:%5d rle:%5d lit:%5d lz2:%5d", spans, lz1, rle, lit, lz2);
	return idx;
}

int verify_frame(Frame* aFrame, Frame* aPrev, int aWidth, int aHeight)
{
	unsigned char* buf = new unsigned char[aWidth * aHeight];
	memset(buf, 0xcd, aWidth * aHeight);
	int readbytes = 0;
	switch (aFrame->mFrameType)
	{
	case SAMEFRAME:
		memcpy(buf, aFrame->mIndexPixels, aWidth * aHeight);
		break;
	case BLACKFRAME:
		memset(buf, 0, aWidth * aHeight);
		break;
	case FLI_COPY:
		memcpy(buf, aFrame->mIndexPixels, aWidth * aHeight);
		readbytes = aWidth * aHeight;
		break;
	case ONECOLOR:
		memset(buf, aFrame->mFrameData[0], aWidth * aHeight);
		readbytes = 1;
		break;
	case RLEFRAME:
		readbytes = decode_rleframe(buf, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth, aHeight);
		break;	
	case DELTA8FRAME:
		memcpy(buf, aPrev->mIndexPixels, aWidth * aHeight);
		readbytes = decode_delta8frame(buf, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth, aHeight);
		break;				
	case DELTA16FRAME:
		memcpy(buf, aPrev->mIndexPixels, aWidth * aHeight);
		readbytes = decode_delta16frame(buf, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth, aHeight);
		break;		
	case LINEARRLE8:
		readbytes = decode_linearrle8(buf,  aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LINEARRLE16:
		readbytes = decode_linearrle16(buf, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LINEARDELTA8:
		readbytes = decode_lineardelta8(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LINEARDELTA16:
		readbytes = decode_lineardelta16(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ1:
		readbytes = decode_lz1(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ2:
		readbytes = decode_lz2(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ1B:
		readbytes = decode_lz1b(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ2B:
		readbytes = decode_lz2b(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ3:
		readbytes = decode_lz3(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ4:
		readbytes = decode_lz4(buf, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ5:
		readbytes = decode_lz5(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ6:
		readbytes = decode_lz6(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ3C:
		readbytes = decode_lz3c(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ3B:
	case LZ3D:
	case LZ3E:
		assert(0); // verify not implemented
		/* fallthrough */
	default:
		delete[] buf;
		return 0;
	}

	if (readbytes != aFrame->mFrameDataSize)
	{
		printf("Block length mismatch (%d)\n", __LINE__);
	}

	for (int i = 0; i < aWidth * aHeight; i++)
		if (aFrame->mIndexPixels[i] != buf[i])
		{
			printf("Encoding error (%d)\n", __LINE__);
		}
	delete[] buf;
	return 0;
}


int readbyte(unsigned char* &f)
{
	return *f++;
}

int readword(unsigned char* &f)
{
	int a = *f++;
	int b = *f++;
	return a + (b << 8);
}

void verifyfile(const char* fn, const char* logfilename)
{
	FILE* f = fopen(fn, "rb");
	FILE* lf = fopen(logfilename, "w");
	if (!f)
	{
		printf("Can't verify \"%s\" - file not found\n", fn);
		return;
	}
	fseek(f, 0, SEEK_END);
	int len = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char* data = new unsigned char[len];
	unsigned char* p = data;
	fread(data, 1, len, f);
	fclose(f);
	if (readbyte(p) != 'F' ||
		readbyte(p) != 'L' ||
		readbyte(p) != 'X' ||
		readbyte(p) != '!')
	{
		printf("Header tag wrong in \"%s\"\n", fn);
		delete[] data;
		return;
	}
	int frames = readword(p);
	int speed = readword(p);
	int config = readword(p);
	int drawoffset = readword(p);
	int loopoffset = readword(p);
	
	if (frames == 0 || speed == 0)
	{
		printf("%d frames, %d speed in \"%s\"\n", frames, speed, fn);
		delete[] data;
		return;
	}
	int count = 0;
	for (int i = 0; i < 512; i++)
		count += readbyte(p);
	if (count == 0)
	{
		printf("All black palette in \"%s\"\n", fn);
		delete[] data;
		return;	
	}
	int pixels = 192 * 256;
	unsigned char* f1 = new unsigned char[pixels];
	unsigned char* f2 = new unsigned char[pixels];
	memset(f1, 0, pixels);
	memset(f2, 0, pixels);

	printf("Verifying \"%s\", %d frames, %d speed, %d config, %d drawoffs, %d loopoffs\n", fn, frames, speed, config, drawoffset, loopoffset);

	int dstofs = 0, srcofs = 0;
	int frame = 0;
	while ((p-data) < len && frame < frames)
	{
		int ftype = readbyte(p);
		int flen = 0;
		if (ftype != FLX_NEXT && ftype != FLX_SUBFRAME)
			flen = readword(p);
		if (lf) fprintf(lf, "%5d:", frame);
		switch (ftype)
		{
		case FLX_SUBFRAME:
			if (lf) fprintf(lf, "LFX_SUBFRAME ");
			break;
		case FLX_NEXT:
			if (lf) fprintf(lf, "LFX_NEXT     ");
			break;
		case FLX_SAME:
			if (lf) fprintf(lf, "LFX_SAME     ");
			memcpy(f1 + dstofs, f2 + srcofs, pixels);
			break;
		case FLX_BLACK:
			if (lf) fprintf(lf, "LFX_BLACK    ");
			memset(f1 + dstofs, 0, pixels);
			break;
		case FLX_ONE:
			if (lf) fprintf(lf, "LFX_ONE      ");
			memset(f1 + dstofs, readbyte(p), pixels);
			break;
		case FLX_LZ1B:
			if (lf) fprintf(lf, "LFX_LZ1B     ");
			p += decode_lz1b(f1 + dstofs, f2 + srcofs, p, flen, pixels, lf);
			break;
		case FLX_LZ4:
			if (lf) fprintf(lf, "LFX_LZ4      ");
			p += decode_lz4(f1 + dstofs, p, flen, pixels, lf);
			break;
		case FLX_LZ5:
			if (lf) fprintf(lf, "LFX_LZ5      ");
			p += decode_lz5(f1 + dstofs, f2 + srcofs, p, flen, pixels, lf);
			break;
		case FLX_LZ6:
			if (lf) fprintf(lf, "LFX_LZ6      ");
			p += decode_lz6(f1 + dstofs, f2 + srcofs, p, flen, pixels, lf);
			break;
		case FLX_LZ3C:
			if (lf) fprintf(lf, "LFX_LZ3C     ");
			p += decode_lz3c(f1 + dstofs, f2 + srcofs, p, flen, pixels, lf);
			break;
		default:
			assert(0); // verify not implemented
		}
		if (lf) fprintf(lf, "\n");

		if (ftype == FLX_SUBFRAME)
		{
			dstofs += 160 * 256;
			srcofs += 16 * 1024;
		}
		else
		if (ftype == FLX_NEXT)
		{
			unsigned char* t = f1;
			f1 = f2;
			f2 = t;
			srcofs = 0;
			dstofs = 0;
			frame++;
		}
		else
		{
			int hash1 = readbyte(p);
			int hash2 = readbyte(p);

			unsigned char sum1 = 0;
			unsigned char sum2 = 0;
			for (int i = 0; i < pixels; i++)
			{
				sum1 ^= f1[i];
				sum2 += sum1;
			}
			if (sum1 != hash1 || sum2 != hash2)
			{
				printf("Frame hash mismatch - frame %d, \"%s\" - frametype %d - %d,%d vs %d,%d\n", frame, fn, ftype, sum1, sum2, hash1, hash2);
			}
		}
	}
	if (frame < frames)
	{
		printf("didn't read all frames?\n");
	}
	if ((p - data) != len)
	{
		printf("didn't reach end of file?\n");
	}
	delete[] data;
	printf("Verify done.\n");
	
	if (lf)
	{
		fprintf(lf, "\nLen, Spanlen, RLElen, LZlen, Litlen\n");
		for (int i = 0; i < 8192; i++)
			fprintf(lf, "%d, %d, %d, %d, %d\n", i, lzlen[i] + rlelen[i] + litlen[i], rlelen[i],lzlen[i],litlen[i]);
		fprintf(lf, "\n\n");
	}
	
	if (lf) fclose(lf);
}
