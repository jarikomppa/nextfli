#include <stdio.h>
#include <string.h>
#include "nextfli.h"

int decode_rleframe(unsigned char *buf, unsigned char *data, int datalen, int aWidth, int aHeight)
{
	int idx = 0;
	for (int y = 0; y < aHeight; ++y) 
	{
		unsigned char* it = buf + aWidth * y;
		int x = 0;
		int npackets = data[idx++];
		while (idx < datalen && npackets-- != 0 && x < aWidth) 
		{
			int count = (signed char)(data[idx++]);
			if (count >= 0) 
			{
				unsigned char color = data[idx++];
				while (count-- != 0 && x < aWidth) 
				{
					*it = color;
					++it;
					++x;
				}
			}
			else 
			{
				while (count++ != 0 && x < aWidth) 
				{
					*it = data[idx++];
					++it;
					++x;
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
	int skipLines = read16(data, idx);
	int nlines = read16(data, idx);

	for (int y = skipLines; y < skipLines + nlines; ++y) 
	{
		// Break in case of invalid data
		if (y < 0 || y >= aHeight)
		{
			printf("invalid data\n");
			break;
		}

		unsigned char* it = buf + aWidth * y;
		int x = 0;
		int npackets = data[idx++];
		int yskip = data[idx++];
		y += yskip;
		while (npackets-- && x < aWidth) 
		{
			int skip = data[idx++];

			x += skip;
			it += skip;

			int count = (signed char)data[idx++];
			if (count >= 0) 
			{
				unsigned char* end = buf + aWidth * aHeight;
				while (count-- != 0 && it < end) 
				{
					*it = data[idx++];
					++it;
					++x;
				}
				// Broken file? More bytes than available buffer
				if (it == end)
					return idx;
			}
			else 
			{
				unsigned char color = data[idx++];
				while (count++ != 0 && x < aWidth) {
					*it = color;
					++it;
					++x;
				}
			}
		}
	}
	return idx;
}

int decode_delta16frame(unsigned char* buf, unsigned char* data, int datalen, int aWidth, int aHeight)
{
	int idx = 0;
	int nlines = read16(data, idx);
	int y = 0;
	while (nlines-- != 0) 
	{
		int npackets = 0;

		while (idx < datalen) 
		{
			signed short word = (signed short)read16(data, idx);
			if (word < 0) 
			{          // Has bit 15 (0x8000)
				if (word & 0x4000) 
				{      // Has bit 14 (0x4000)
					y += -word;          // Skip lines
				}
				// Only last pixel has changed
				else 
				{
					if (y < 0 || y >= aHeight)
					{
						printf("Decode error\n");
					}
					if (y >= 0 && y < aHeight) 
					{
						unsigned char* it = buf + y * aWidth + aWidth - 1;
						*it = (word & 0xff);
					}
					++y;
					if (nlines-- == 0)
						return idx;             // We are done
				}
			}
			else 
			{
				npackets = word;
				if (word > 256)
				{
					printf("Excessive packets\n");
				}
				break;
			}
		}

		// Avoid invalid data to skip more lines than what is available.
		if (y >= aHeight)
		{
			printf("Encoding error\n");
			break;
		}

		int x = 0;
		while (npackets-- != 0) 
		{
			x += data[idx++];           // Skip pixels
			signed char count = (signed char)data[idx++]; // Number of words

			if (y < 0 || y >= aHeight || x < 0 || x >= aWidth)
			{
				printf("Decode error\n");
			}
			
			unsigned char* it = buf + y * aWidth + x;

			if (count >= 0) 
			{
				while (count-- != 0 && x < aWidth) 
				{
					int color1 = data[idx++];
					int color2 = data[idx++];

					*it = color1;
					++it;
					++x;

					if (x < aWidth) 
					{
						*it = color2;
						++it;
						++x;
					}
				}
			}
			else 
			{
				int color1 = data[idx++];
				int color2 = data[idx++];

				while (count++ != 0 && x < aWidth)
				{
					*it = color1;
					++it;
					++x;

					if (x < aWidth) 
					{
						*it = color2;
						++it;
						++x;
					}
				}
			}
		}

		++y;
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
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs++] = rundata;
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
	if (ofs > pixels) printf("wrote over buffer Lrle8 %d %d\n", ofs, pixels);
	return idx;
}

int decode_linearrle16(unsigned short* buf, unsigned short* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runlen = data[idx++];
		int rundata = data[idx++];
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs++] = rundata;
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
	if (ofs > pixels) printf("wrote over buffer Lrle16 %d %d\n", ofs, pixels);
	return idx * 2;
}

int decode_lineardelta8(unsigned char* buf, unsigned char *prev, unsigned char* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runlen = data[idx++];
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs] = prev[ofs];
			ofs++;
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
	if (ofs > pixels) printf("wrote over buffer Ldelta8 %d %d\n", ofs, pixels);
	return idx;
}

int decode_lineardelta16(unsigned short* buf, unsigned short* prev, unsigned short* data, int datasize, int pixels)
{
	int idx = 0;
	int ofs = 0;
	while (ofs < pixels)
	{
		int runlen = data[idx++];
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs] = prev[ofs];
			ofs++;
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
	if (ofs > pixels) printf("wrote over buffer Ldelta16 %d %d\n", ofs, pixels);
	return idx * 2;
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
		if (runlen + ofs > pixels) printf("boo %d %d %d\n", runlen, ofs, pixels);
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs++] = prev[runofs++];
		}
		if (ofs < pixels)
		{
			int copylen = data[idx++];
			if (copylen + ofs > pixels) printf("hiss");
			for (int i = 0; i < copylen; i++)
			{
				buf[ofs++] = data[idx++];
			}
		}
	}
	if (ofs > pixels) printf("wrote over buffer lz1 %d %d\n", ofs, pixels);
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
	case ONECOLOR:
		memset(buf, aFrame->mFrameData[0], aWidth * aHeight);
		readbytes = 1;
		break;
	case RLEFRAME:
		readbytes = decode_rleframe(buf, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth, aHeight);
		break;
	case DELTA8FRAME:
		memcpy(buf, aFrame->mIndexPixels, aWidth * aHeight);
		readbytes = decode_delta8frame(buf, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth, aHeight);
		break;		
	case DELTA16FRAME:
		memcpy(buf, aFrame->mIndexPixels, aWidth * aHeight);
		readbytes = decode_delta16frame(buf, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth, aHeight);
		break;		
	case LINEARRLE8:
		readbytes = decode_linearrle8(buf,  aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LINEARRLE16:
		readbytes = decode_linearrle16((unsigned short*)buf, (unsigned short*)aFrame->mFrameData, aFrame->mFrameDataSize / 2, aWidth * aHeight / 2);
		break;
	case LINEARDELTA8:
		readbytes = decode_lineardelta8(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LINEARDELTA16:
		readbytes = decode_lineardelta16((unsigned short*)buf, (unsigned short*)aPrev->mIndexPixels, (unsigned short*)aFrame->mFrameData, aFrame->mFrameDataSize / 2, aWidth * aHeight / 2);
		break;
	case LZ1:
		readbytes = decode_lz1(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	default:
		delete[] buf;
		return 0;
	}

	if (readbytes != aFrame->mFrameDataSize)
	{
		printf("Block length mismatch\n");
	}

	for (int i = 0; i < aWidth * aHeight; i++)
		if (aFrame->mIndexPixels[i] != buf[i])
		{
			printf("Encoding error\n");
		}
	delete[] buf;
	return 0;
}