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


int encodeLinearRLE8Frame(unsigned char* data, unsigned char* src, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		int l = runlength(src + ofs, (ofs + 255) > pixels ? pixels - ofs : 255);
		data[out++] = l; // value of 0 means run of 0 bytes 
		data[out++] = src[ofs]; // could skip this on 0, but that hardly ever happens
		ofs += l;

		if (ofs < pixels)
		{
			// run/copy
			l = runlength(src + ofs, (ofs + 128) > pixels ? pixels - ofs : 128);
			if (l > 2)
			{
				data[out++] = -l; // value of 0 means run of 0 bytes 
				data[out++] = src[ofs]; // could skip this on 0, but that hardly ever happens
				ofs += l;
			}
			else
			{
				l = 0;
				while (l < 127 && ofs + l + 3 < pixels && runlength(src + ofs + l, 3) < 3)
					l++;

				if (ofs + l + 3 >= pixels)
					l = pixels - ofs;

				if (l > 127) l = 127;

				data[out++] = l;

				for (int i = 0; i < l; i++)
				{
					data[out++] = src[ofs++];
				}
			}
		}
	}
	return out;
}


int encodeLinearRLE16Frame(unsigned char* data, unsigned char* src, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run/copy
		int l = runlength(src + ofs, (ofs + 65535) > pixels ? pixels - ofs : 65535);
		if (l > 2)
		{
			if (l < 128)
			{
				data[out++] = -l; // value of 0 means run of 0 bytes 
				data[out++] = src[ofs]; // could skip this on 0, but that hardly ever happens
			}
			else
			{
				data[out++] = -128; // mark as "long run"
				data[out++] = src[ofs]; 
				data[out++] = l & 0xff;
				data[out++] = (l >> 8) & 0xff;
			}
			ofs += l;
		}
		else
		{
			l = 0;
			while (l < 127 && ofs + l + 3 < pixels && runlength(src + ofs + l, 3) < 3)
				l++;

			if (ofs + l + 3 >= pixels)
				l = pixels - ofs;

			if (l > 127) l = 127;

			data[out++] = l;

			for (int i = 0; i < l; i++)
			{
				data[out++] = src[ofs++];
			}
		}
	}
	return out;
}


int encodeLinearDelta8Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;

	while (ofs < pixels)
	{
		// skip/run
		int ls = skiplength(aFrame + ofs, aPrev + ofs, ofs + 128 > pixels ? pixels - ofs : 128);
		int lr = runlength(aFrame + ofs, ofs + 127 > pixels ? pixels - ofs : 127);
		if (lr > ls)
		{
			data[out++] = lr;
			data[out++] = aFrame[ofs];
			ofs += lr;
		}
		else
		{
			data[out++] = -ls;
			ofs += ls;
		}

		if (ofs < pixels)
		{
			// skip/copy
			int ls = skiplength(aFrame + ofs, aPrev + ofs, ofs + 128 > pixels ? pixels - ofs : 128);			
			// one skip + copy segment costs at least 2 bytes, so skip until at least 2 byte run is found
			int lc = 0;
			while (lc < 127 && ofs + lc + 2 < pixels && 
				skiplength(aFrame + ofs + lc, aPrev + ofs + lc, 2) < 2 && 
				runlength(aFrame + ofs + lc, 2) < 2) 
				lc++;
								
			if (ofs + lc + 2 >= pixels)
				lc = pixels - ofs;

			if (ls > lc)
			{
				data[out++] = -ls;
				ofs += ls;
			}
			else
			{
				data[out++] = lc;
				for (int i = 0; i < lc; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
			}
		}
	}
	return out;
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
			while (l < max && (ofs + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
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
			while (l < 127 && (ofs + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
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

int encodeLZ1bFrame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
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
		}
		else
		{
			data[out++] = lz;
			data[out++] = (o >> 0) & 0xff;
			data[out++] = (o >> 8) & 0xff;
			ofs += lz;
		}

		if (ofs < pixels)
		{
			// copy
			// one run + copy segment costs at least 5 bytes, so skip until at least 5 byte run is found
			int lc = 0;
			while (lc < 127 && ofs + lc + 5 < pixels && 
				bestLZRun0(aFrame, aPrev, ofs + lc, pixels, o, 5) < 5 &&
				runlength(aFrame + ofs + lc, 5) < 5) 
				lc++;
			if (ofs + lc + 5 > pixels)
				lc = pixels - ofs;
			if (lc > 127) lc = 127;

			int lz = bestLZRun1b(aFrame, aPrev, ofs, pixels, o);

			if (lz < 5)
			{
				data[out++] = lc;
				for (int i = 0; i < lc; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
			}
			else
			{
				if (lz > 128) lz = 128;
				data[out++] = -lz;
				data[out++] = (o >> 0) & 0xff;
				data[out++] = (o >> 8) & 0xff;
				ofs += lz;
			}
		}
	}

	return out;
}
