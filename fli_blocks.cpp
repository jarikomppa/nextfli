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
			l = 0;
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

int encodeDelta8Line(unsigned char* aOut, unsigned char* aSrc, unsigned char* aPrev, int width, int skiplines)
{
	int packets = 0;
	int ofs = 0;
	aOut[ofs++] = 0; // number of packets
	aOut[ofs++] = skiplines; // line skip count

	// packet: skipcount, run/copy, pixeldata
	int inp = 0;
	while (inp < width)
	{
		// can we skip?
		int skip = 0;
		while ((inp + skip < width) && aSrc[inp + skip] == aPrev[inp + skip]) skip++;
		if (inp + skip > width)
			break;
		if (skip > 255) skip = 255;
		aOut[ofs++] = skip;
		inp += skip;
		// can we run; interrupt run if we can skip at least 4 bytes
		int l = 0;
		while ((inp + l < width) &&
			aSrc[inp + l] == aSrc[inp] &&
			!(inp + l + 3 < width &&
				aSrc[inp + l] == aPrev[inp + l] &&
				aSrc[inp + l + 1] == aPrev[inp + l + 1] &&
				aSrc[inp + l + 2] == aPrev[inp + l + 2] &&
				aSrc[inp + l + 3] == aPrev[inp + l + 3])) 
			l++;

		if (l > 3)
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
			l = 0;
			// interrupt copy if a run or skip of at least 4 bytes is found
			while ((p + 4) < width &&
				!(aSrc[p] == aSrc[p + 1] &&
					aSrc[p] == aSrc[p + 2] &&
					aSrc[p] == aSrc[p + 3] &&
					aSrc[p] == aSrc[p + 4]) &&
				!(aSrc[p] == aPrev[p] &&
					aSrc[p + 1] == aPrev[p + 1] &&
					aSrc[p + 2] == aPrev[p + 2] &&
					aSrc[p + 3] == aPrev[p + 3])) 
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
	int row = 0;
	int match = 1;
	while (match && row < height)
	{
		for (int i = 0; match && i < width; i++)
			match = (aFrame[row * width + i] == aPrev[row * width + i]);
		if (match)
			row++;
	}
	data[ofs] = row & 0xff;
	ofs++;
	data[ofs] = (row >> 8) & 0xff;
	ofs++;

	data[ofs] = (height - row) & 0xff;
	ofs++;
	data[ofs] = ((height - row) >> 8) & 0xff;
	ofs++;

	while (row < height)
	{
		match = 1;
		int skip = 0;
		row++;
		while (match && (row + skip) < height)
		{
			for (int i = 0; match && i < width; i++)
				match = (aFrame[(row + skip) * width + i] == aPrev[(row + skip) * width + i]);
			if (match)
				skip++;
		}
		int w = encodeDelta8Line(data + ofs, aFrame + (row - 1) * width, aPrev + (row - 1) * width, width, skip);
		ofs += w;
		row += skip;
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
		// can we run; interrupt run if we can skip at least 4 bytes
		int l = 0;
		while ((inp + l < width) &&
			aSrc[inp + l] == aSrc[inp] &&
			!(inp + l + 3 < width &&
				aSrc[inp + l] == aPrev[inp + l] &&
				aSrc[inp + l + 1] == aPrev[inp + l + 1] &&
				aSrc[inp + l + 2] == aPrev[inp + l + 2] &&
				aSrc[inp + l + 3] == aPrev[inp + l + 3]))
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
			l = 0;
			// interrupt copy if a run or skip of at least 4 bytes is found
			while ((p + 4) < width &&
				!(aSrc[p] == aSrc[p + 1] &&
					aSrc[p] == aSrc[p + 2] &&
					aSrc[p] == aSrc[p + 3] &&
					aSrc[p] == aSrc[p + 4]) &&
				!(aSrc[p] == aPrev[p] &&
					aSrc[p + 1] == aPrev[p + 1] &&
					aSrc[p + 2] == aPrev[p + 2] &&
					aSrc[p + 3] == aPrev[p + 3]))
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
			data[ofs++] = skip & 0xff;
			data[ofs++] = ((skip >> 8) & 0xff) | 0xc0;
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