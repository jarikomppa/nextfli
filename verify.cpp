#include <stdio.h>
#include <string.h>
#include "nextfli.h"

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
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs++] = prev[runofs++];
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
		int runofs = ((signed char)data[idx++]) + ofs;
		int runlen = data[idx++];
		for (int i = 0; i < runlen; i++)
		{
			buf[ofs++] = prev[runofs++];
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
	if (ofs > pixels) printf("wrote over buffer lz3 %d %d\n", ofs, pixels);
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
	case LZ2:
		readbytes = decode_lz2(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
		break;
	case LZ3:
		readbytes = decode_lz3(buf, aPrev->mIndexPixels, aFrame->mFrameData, aFrame->mFrameDataSize, aWidth * aHeight);
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