#include <stdio.h>
#include "nextfli.h"

int runlength(unsigned char* data, int max)
{
	int l = 1;
	while (l < max && data[0] == data[l]) l++;
	return l;
}

int runlength16(unsigned char* cdata, int max)
{
	unsigned short* data = (unsigned short*)cdata;
	int l = 1;
	while (l < max / 2 && data[0] == data[l]) l++;
	return l * 2;
}

int skiplength(unsigned char* data, unsigned char* prev, int max)
{
	int l = 0;
	while (l < max && data[l] == prev[l]) l++;
	return l;
}

int skiplength16(unsigned char* cdata, unsigned char* cprev, int max)
{
	int l = 0;
	unsigned short* data = (unsigned short*)cdata;
	unsigned short* prev = (unsigned short*)cprev;
	while (l < max / 2 && data[l] == prev[l]) l++;
	return l * 2;
}

int calcminspan(int aProposal, int aMinspan)
{
	if (aProposal > aMinspan)
		return aProposal;
	return aMinspan;
}

int bestLZRun0(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs, int max)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
		if (aFrame[ofs + bestlen] == aPrev[i + bestlen])
		{
			int l = 0;
			while (l < max && (ofs + l) < pixels && (i + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
			{
				l++;
			}

			if (l > bestlen)
			{
				bestlen = l;
				bestofs = i;
				if (bestlen > 10)
				{
					runofs = bestofs;
					return bestlen;
				}
			}
		}
	}
	runofs = bestofs;
	return bestlen;
}

int bestLZRun1b(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
		if (aFrame[ofs + bestlen] == aPrev[i + bestlen])
		{
			int l = 0;
			while (l < 127 && (ofs + l) < pixels && (i + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
			{
				l++;
			}

			if (l > bestlen)
			{
				bestlen = l;
				bestofs = i;
				if (bestlen > 126)
				{
					runofs = bestofs;
					return 127;
				}
			}
		}
	}
	runofs = bestofs;
	return bestlen;
}

int encodeLZ1bFrame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels, int minspan, int& spans)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run/z
		int o = 0;
		int lz = bestLZRun1b(aFrame, aPrev, ofs, pixels, o);
		int lr = runlength(aFrame + ofs, ofs + 128 > pixels ? pixels - ofs : 128);
		if (lr >= lz)
		{
			data[out++] = -lr;
			data[out++] = aFrame[ofs];
			ofs += lr;
			spans++;
		}
		else
		{
			data[out++] = lz;
			data[out++] = (o >> 0) & 0xff;
			data[out++] = (o >> 8) & 0xff;
			ofs += lz;
			spans++;
		}

		if (ofs < pixels)
		{
			int lz = bestLZRun1b(aFrame, aPrev, ofs, pixels, o);

			int ms = calcminspan(5, minspan);
			if (lz < ms)
			{
				// copy
				// one run + copy segment costs at least 5 bytes, so skip until at least 5 byte run is found
				int lc = minspan;
				while (lc < 127 && ofs + lc + ms < pixels &&
					bestLZRun0(aFrame, aPrev, ofs + lc, pixels, o, ms) < ms &&
					runlength(aFrame + ofs + lc, ms) < ms)
					lc++;
				if (ofs + lc + ms >= pixels)
					lc = pixels - ofs;
				if (lc > 127) lc = 127;

				data[out++] = lc;
				for (int i = 0; i < lc; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
				spans++;
			}
			else
			{
				if (lz > 128) lz = 128;
				data[out++] = -lz;
				data[out++] = (o >> 0) & 0xff;
				data[out++] = (o >> 8) & 0xff;
				ofs += lz;
				spans++;
			}
		}
	}

	return out;
}

int bestLZRun4(unsigned char* aFrame, int ofs, int pixels, int& runofs)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < ofs; i++)
	{
		if (aFrame[ofs + bestlen] == aFrame[i + bestlen])
		{
			int l = 0;
			while ((ofs + l) < pixels && (i + l) < pixels && aFrame[ofs + l] == aFrame[i + l])
			{
				l++;
			}

			if (l > bestlen)
			{
				bestlen = l;
				bestofs = i;
				if (ofs + l == pixels)
				{
					runofs = bestofs;
					return bestlen;
				}
			}
		}
	}
	runofs = bestofs;
	return bestlen;
}

int bestLZRun4check(unsigned char* aFrame, int ofs, int pixels, int bytes)
{
	int bestlen = 0;
	for (int i = 0; i < ofs; i++)
	{
		if (aFrame[ofs + bestlen] == aFrame[i + bestlen])
		{
			int l = 0;
			while ((l < bytes) && (ofs + l) < pixels && (i + l) < pixels && aFrame[ofs + l] == aFrame[i + l])
			{
				l++;
			}

			if (l > bestlen)
			{
				bestlen = l;
				if (l == bytes)
				{
					return l;
				}
			}
		}
	}
	return bestlen;
}

int encodeLZ4Frame(unsigned char* data, unsigned char* aFrame, int pixels, int minspan, int& spans)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run/z
		int o = 0;
		int lz = bestLZRun4(aFrame, ofs, pixels, o);
		int lr = runlength(aFrame + ofs, pixels - ofs);
		if (lr >= lz)
		{
			if (lr > 127)
			{
				data[out++] = -128;
				data[out++] = (lr >> 0) & 0xff;
				data[out++] = (lr >> 8) & 0xff;
			}
			else
			{
				data[out++] = -lr;
			}
			data[out++] = aFrame[ofs];
			ofs += lr;
			spans++;
		}
		else
		{
			if (lz > 126)
			{
				data[out++] = 127;
				data[out++] = (lz >> 0) & 0xff;
				data[out++] = (lz >> 8) & 0xff;
			}
			else
			{
				data[out++] = lz;
			}
			data[out++] = (o >> 0) & 0xff;
			data[out++] = (o >> 8) & 0xff;
			ofs += lz;
			spans++;
		}

		if (ofs < pixels)
		{
			int lz = bestLZRun4(aFrame, ofs, pixels, o);

			int ms = calcminspan(5, minspan);
			if (lz < ms)
			{
				// copy
				// one run + copy segment costs at least 5 bytes, so skip until at least 5 byte run is found
				int lc = minspan;
				while (lc < 127 && ofs + lc + ms < pixels &&
					bestLZRun4check(aFrame, ofs + lc, pixels, ms) < ms &&
					runlength(aFrame + ofs + lc, ms) < ms)
					lc++;
				if (ofs + lc + ms >= pixels)
					lc = pixels - ofs;
				if (lc > 127) lc = 127;

				data[out++] = lc;

				for (int i = 0; i < lc; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
				spans++;
			}
			else
			{
				if (lz > 127)
				{
					data[out++] = -128;
					data[out++] = (lz >> 0) & 0xff;
					data[out++] = (lz >> 8) & 0xff;
				}
				else
				{
					data[out++] = -lz;
				}
				data[out++] = (o >> 0) & 0xff;
				data[out++] = (o >> 8) & 0xff;
				ofs += lz;
				spans++;
			}
		}
	}

	return out;
}


int bestLZRun5(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
		if (aFrame[ofs + bestlen] == aPrev[i + bestlen])
		{
			int l = 0;
			while ((ofs + l) < pixels && (i + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
			{
				l++;
			}

			if (l > bestlen)
			{
				bestlen = l;
				bestofs = i;
				if ((ofs + l) == pixels)
				{
					runofs = bestofs;
					return bestlen;
				}
			}
		}
	}
	runofs = bestofs;
	return bestlen;
}

int bestLZRun5check(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int bytes)
{
	int bestlen = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
		if (aFrame[ofs + bestlen] == aPrev[i + bestlen])
		{
			int l = 0;
			while ((ofs + l) < pixels && (i + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
			{
				l++;
			}

			if (l > bestlen)
			{
				bestlen = l;
				if (l > bytes)
				{
					return bestlen;
				}
			}
		}
	}
	return bestlen;
}

int encodeLZ5Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels, int minspan, int& spans)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run/z
		int o = 0;
		int lz = bestLZRun5(aFrame, aPrev, ofs, pixels, o);
		int lr = runlength(aFrame + ofs, pixels - ofs);
		if (lr >= lz)
		{
			if (lr > 127)
			{
				data[out++] = -128;
				data[out++] = (lr >> 0) & 0xff;
				data[out++] = (lr >> 8) & 0xff;
			}
			else
			{
				data[out++] = -lr;
			}
			data[out++] = aFrame[ofs];
			ofs += lr;
			spans++;
		}
		else
		{
			if (lz > 126)
			{
				data[out++] = 127;
				data[out++] = (lz >> 0) & 0xff;
				data[out++] = (lz >> 8) & 0xff;
			}
			else
			{
				data[out++] = lz;
			}
			data[out++] = (o >> 0) & 0xff;
			data[out++] = (o >> 8) & 0xff;
			ofs += lz;
			spans++;
		}

		if (ofs < pixels)
		{
			int lz = bestLZRun5(aFrame, aPrev, ofs, pixels, o);

			int ms = calcminspan(5, minspan);
			if (lz < ms)
			{
				// copy
				// one run + copy segment costs at least 5 bytes, so skip until at least 5 byte run is found
				int lc = minspan;
				while (ofs + lc + ms < pixels &&
					bestLZRun5check(aFrame, aPrev, ofs + lc, pixels, ms) < ms &&
					runlength(aFrame + ofs + lc, ms) < ms)
					lc++;
				if (ofs + lc + ms >= pixels)
					lc = pixels - ofs;

				if (lc > 126)
				{
					data[out++] = 127;
					data[out++] = (lc >> 0) & 0xff;
					data[out++] = (lc >> 8) & 0xff;
				}
				else
				{
					data[out++] = lc;
				}
				for (int i = 0; i < lc; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
				spans++;
			}
			else
			{
				if (lz > 127)
				{
					data[out++] = -128;
					data[out++] = (lz >> 0) & 0xff;
					data[out++] = (lz >> 8) & 0xff;
				}
				else
				{
					data[out++] = -lz;
				}
				data[out++] = (o >> 0) & 0xff;
				data[out++] = (o >> 8) & 0xff;
				ofs += lz;
				spans++;
			}
		}
	}

	return out;
}

int encodeLZ6Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels, int minspan, int& spans)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run/z
		int o = 0;
		int lz = bestLZRun5(aFrame, aPrev, ofs, pixels, o);
		int lr = runlength(aFrame + ofs, pixels - ofs);
		if (lr < lz)
		{
			if (lz > 126)
			{
				data[out++] = 127;
				data[out++] = (lz >> 0) & 0xff;
				data[out++] = (lz >> 8) & 0xff;
			}
			else
			{
				data[out++] = lz;
			}
			data[out++] = (o >> 0) & 0xff;
			data[out++] = (o >> 8) & 0xff;
			ofs += lz;
			spans++;
		}
		else
		{
			if (lr > 127)
			{
				data[out++] = -128;
				data[out++] = (lr >> 0) & 0xff;
				data[out++] = (lr >> 8) & 0xff;
			}
			else
			{
				data[out++] = -lr;
			}
			data[out++] = aFrame[ofs];
			ofs += lr;
			spans++;
		}

		if (ofs < pixels)
		{
			int lz = bestLZRun4(aFrame, ofs, pixels, o);

			int ms = calcminspan(5, minspan);
			if (lz < ms)
			{
				// copy
				// one run + copy segment costs at least 5 bytes, so skip until at least 5 byte run is found
				int lc = minspan;
				while (ofs + lc + ms < pixels &&
					bestLZRun4check(aFrame, ofs + lc, pixels, ms) < ms &&
					runlength(aFrame + ofs + lc, ms) < ms)
					lc++;

				if (ofs + lc + ms >= pixels)
					lc = pixels - ofs;

				if (lc > 126)
				{
					data[out++] = 127;
					data[out++] = (lc >> 0) & 0xff;
					data[out++] = (lc >> 8) & 0xff;
				}
				else
				{
					data[out++] = lc;
				}
				for (int i = 0; i < lc; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
				spans++;
			}
			else
			{
				if (lz > 127)
				{
					data[out++] = -128;
					data[out++] = (lz >> 0) & 0xff;
					data[out++] = (lz >> 8) & 0xff;
				}
				else
				{
					data[out++] = -lz;
				}
				data[out++] = (o >> 0) & 0xff;
				data[out++] = (o >> 8) & 0xff;
				ofs += lz;
				spans++;
			}
		}
	}

	return out;
}

int bestLZRun3(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, signed char& runofs, int max)
{
	int bestlen = 0;
	int bestofs = 0;
	int start = ofs - 128;
	if (start < 0) start = 0;
	int end = ofs + 127;
	if (end > pixels)
		end = pixels - ofs;
	for (int i = start; i < end; i++)
	{
		if (i + bestlen < end && aFrame[ofs + bestlen] == aPrev[i + bestlen])
		{
			int l = 0;
			while (l < max && (ofs + l) < pixels && (i + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
			{
				l++;
			}

			if (l > bestlen)
			{
				bestlen = l;
				bestofs = i;
			}
		}
	}
	runofs = bestofs - ofs;
	return bestlen;
}

int encodeLZ3CFrame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels, int minspan, int& spans)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run/lz
		signed char o = 0;
		int lz = bestLZRun3(aFrame, aPrev, ofs, pixels, o, pixels - ofs);
		int lr = runlength(aFrame + ofs, pixels - ofs);
		if (lr > lz)
		{
			if (lr > 127)
			{
				data[out++] = -128;
				data[out++] = (lr >> 0) & 0xff;
				data[out++] = (lr >> 8) & 0xff;
			}
			else
			{
				data[out++] = -lr;
			}
			data[out++] = aFrame[ofs];
			ofs += lr;
			spans++;
		}
		else
		{
			if (lz > 126)
			{
				data[out++] = 127;
				data[out++] = (lz >> 0) & 0xff;
				data[out++] = (lz >> 8) & 0xff;
			}
			else
			{
				data[out++] = lz;
			}
			data[out++] = o;
			ofs += lz;
			spans++;
		}

		if (ofs < pixels)
		{
			int lz = bestLZRun3(aFrame, aPrev, ofs, pixels, o, pixels - ofs);

			int ms = calcminspan(4, minspan);
			if (lz < ms)
			{
				// lz/copy
				// one run + skip segment costs at least 4 bytes, so skip until at least 4 byte run is found			
				int lc = minspan;
				while (ofs + lc + ms < pixels &&
					bestLZRun3(aFrame, aPrev, ofs + lc, pixels, o, ms) < ms &&
					runlength(aFrame + ofs + lc, ms) < ms)
					lc++;

				if (ofs + lc + ms > pixels)
					lc = pixels - ofs;

				if (lc > 127)
				{
					data[out++] = -128;
					data[out++] = (lc >> 0) & 0xff;
					data[out++] = (lc >> 8) & 0xff;
				}
				else
				{
					data[out++] = -lc;
				}

				for (int i = 0; i < lc; i++)
				{
					data[out] = aFrame[ofs];
					out++;
					ofs++;
				}
				spans++;
			}
			else
			{
				if (lz > 126)
				{
					data[out++] = 127;
					data[out++] = (lz >> 0) & 0xff;
					data[out++] = (lz >> 8) & 0xff;
				}
				else
				{
					data[out++] = lz;
				}
				data[out++] = o;
				ofs += lz;
				spans++;
			}
		}
	}

	return out;
}
