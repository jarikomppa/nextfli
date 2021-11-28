#include <stdio.h>
#include "nextfli.h"

int encodeRLELine(unsigned char* aOut, unsigned char* aSrc, int width)
{
	// first byte of line is number of packets (should be ignored in flc)
	int ofs = 0;	
	aOut[ofs] = 0;
	unsigned char* packets = &aOut[ofs];
	ofs++;
	int inp = 0;
	while (inp < width)
	{
		(*packets)++;
		// can we run
		int l = 0;
		while ((inp + l < width) && aSrc[inp + l] == aSrc[inp]) l++;
		if (l > 2)
		{
			// encode run
			if (l > 127) l = 127;
			aOut[ofs] = l;
			ofs++;
			aOut[ofs] = aSrc[inp];
			ofs++;
			inp += l;
		}
		else
		{
			// find out how much we need to copy
			int p = inp;
			// stop copy when we find a run of 4 bytes
			while ((p + 3) < width &&
				!(aSrc[p] == aSrc[p + 1] &&
					aSrc[p] == aSrc[p + 2] &&
					aSrc[p] == aSrc[p + 3])) p++;
			l = p - inp;

			if (l == 0)
				l = width - inp;

			// encode copy
			if (l > 128) l = 128;
			aOut[ofs] = -l;
			ofs++;
			for (int i = 0; i < l; i++)
			{
				aOut[ofs] = aSrc[inp];
				ofs++;
				inp++;
			}
		}
	}
	return ofs;
}

int encodeRLEFrame(unsigned char* aRLEframe, unsigned char* aIndexPixels, int width, int height)
{
	int ofs = 0;
	for (int i = 0; i < height; i++)
	{
		int w = encodeRLELine(aRLEframe + ofs, aIndexPixels + i * width, width);
		ofs += w;
	}
	return ofs;
}

int encodeDelta8Line(unsigned char* aOut, unsigned char* aSrc, unsigned char* aPrev, int width)
{
	int packets = 0;
	int ofs = 0;
	aOut[ofs++] = 0; // number of packets

	// packet: skipcount, run/copy, pixeldata
	int inp = 0;
	while (inp < width)
	{
		// can we skip?
		int skip = 0;
		while ((inp + skip < width) && aSrc[inp + skip] == aPrev[inp + skip]) skip++;
		if (inp + skip >= width)
			break;
		if (skip > 255) skip = 255;
		aOut[ofs++] = skip;
		inp += skip;
		// can we run
		int l = 0;
		while ((inp + l < width) &&
			aSrc[inp + l] == aSrc[inp]) 
			l++;

		if (l > 2)
		{
			// encode run
			if (l > 128) l = 128;			
			aOut[ofs++] = -l;			
			aOut[ofs++] = aSrc[inp];
			inp += l;
		}
		else
		{
			// find out how much we need to copy
			int p = inp;
			// interrupt copy if a run or skip of at least 3 bytes is found
			while ((p + 3) < width &&
				!(aSrc[p] == aSrc[p + 1] &&
					aSrc[p] == aSrc[p + 2] &&
					aSrc[p] == aSrc[p + 3]) &&
				!(aSrc[p] == aPrev[p] &&
					aSrc[p + 1] == aPrev[p + 1] &&
					aSrc[p + 2] == aPrev[p + 2])) 
				p++;

			l = p - inp;

			if (l == 0)
				l = width - inp;
			// encode copy
			if (l > 127) l = 127;
			aOut[ofs++] = l;
			for (int i = 0; i < l; i++)
			{
				aOut[ofs++] = aSrc[inp];
				inp++;
			}
		}
		packets++;
	}
	aOut[0] = packets;
	return ofs;
}

int encodeDelta8Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int width, int height)
{
	int ofs = 0;
	int startrow = 0;
	int match = 1;
	while (match && startrow < height)
	{
		for (int i = 0; match && i < width; i++)
			match = (aFrame[startrow * width + i] == aPrev[startrow * width + i]);
		if (match)
			startrow++;
	}
	match = 1;
	int rows = height - startrow;

	while (match && rows)
	{
		for (int i = 0; match && i < width; i++)
			match = (aFrame[(rows + startrow - 1) * width + i] == aPrev[(rows + startrow - 1) * width + i]);
		if (match)
			rows--;
	}

	data[ofs++] = startrow & 0xff;
	data[ofs++] = (startrow >> 8) & 0xff;

	data[ofs++] = (rows) & 0xff;
	data[ofs++] = (rows >> 8) & 0xff;

	for (int i = 0; i < rows; i++)
	{
		match = 1;
		int skip = 0;
		int w = encodeDelta8Line(data + ofs, aFrame + (i + startrow) * width, aPrev + (i + startrow) * width, width);
		ofs += w;
	}

	return ofs;
}

int encodeDelta16Line(unsigned char* aOut, unsigned char* aSrc, unsigned char* aPrev, int width)
{
	int packets = 0;
	int ofs = 0;
	aOut[ofs++] = 0; // number of packets (16 bit)
	aOut[ofs++] = 0;
	// packet: skipcount, run/copy, pixeldata
	int inp = 0;
	while (inp < width)
	{
		// can we skip?
		int skip = 0;
		while ((inp + skip < width) && aSrc[inp + skip] == aPrev[inp + skip]) skip++;
		if (inp + skip >= width)
			break;
		if (skip > 255) skip = 255;
		aOut[ofs++] = skip;
		inp += skip;
		// can we run?
		int l = 0;
		while ((inp + l < width) &&
			aSrc[inp + l] == aSrc[inp])
			l++;

		if (l > 3)
		{
			// encode run
			if (l > 256) l = 256;
			l /= 2;
			aOut[ofs++] = -l;
			aOut[ofs++] = aSrc[inp++];
			aOut[ofs++] = aSrc[inp++];
			inp += l * 2 - 2;
		}
		else
		{
			// find out how much we need to copy
			int p = inp;
			// interrupt copy if a run or skip of at least 6 bytes is found
			while ((p + 6) < width &&
				!(aSrc[p] == aSrc[p + 1] &&
					aSrc[p] == aSrc[p + 2] &&
					aSrc[p] == aSrc[p + 3] &&
					aSrc[p] == aSrc[p + 4] &&
					aSrc[p] == aSrc[p + 5] &&
					aSrc[p] == aSrc[p + 6]) &&
				!(aSrc[p] == aPrev[p] &&
					aSrc[p + 1] == aPrev[p + 1] &&
					aSrc[p + 2] == aPrev[p + 2] &&
					aSrc[p + 3] == aPrev[p + 3] &&
					aSrc[p + 4] == aPrev[p + 4] &&
					aSrc[p + 5] == aPrev[p + 5]))
				p++;

			l = p - inp;

			if (l == 0)
				l = width - inp;
			// encode copy
			if (l > 127*2) l = 127*2;
			l /= 2;
			if (l == 0) l = 1;
			aOut[ofs++] = l;
			for (int i = 0; i < l; i++)
			{
				aOut[ofs++] = aSrc[inp++];
				if (inp < width)
					aOut[ofs++] = aSrc[inp++];
				else
					aOut[ofs++] = 0;

			}
		}
		packets++;
	}
	aOut[0] = packets & 0xff;
	aOut[1] = (packets >> 8) & 0xff;
	return ofs;
}


int encodeDelta16Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int width, int height)
{
	int out_rows = 0;
	int ofs = 0;
	int row = 0;
	data[ofs] = 0;
	ofs++;
	data[ofs] = 0;
	ofs++;

	while (row < height)
	{
		int match = 1;
		int skip = 0;
		while (match && (row + skip) < height)
		{
			for (int i = 0; match && i < width; i++)
				match = (aFrame[(row + skip) * width + i] == aPrev[(row + skip) * width + i]);
			if (match)
				skip++;
		}
		if (row + skip >= height)
			break;
		if (skip)
		{
			unsigned short s = -skip;
			data[ofs++] = s & 0xff;
			data[ofs++] = ((s >> 8) & 0xff);
		}
		row += skip;
		if (row < height)
		{
			int w = encodeDelta16Line(data + ofs, aFrame + row * width, aPrev + row * width, width);
			out_rows++;
			ofs += w;
		}
		row++;
	}
	data[0] = (out_rows) & 0xff;
	data[1] = (out_rows >> 8) & 0xff;

	return ofs;
}