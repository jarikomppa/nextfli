#include <stdio.h>
#include "nextfli.h"

int runlength(unsigned char* data, int max)
{
	int l = 1;
	while (l < max && data[0] == data[l]) l++;
	return l;
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

int runlength16(unsigned char* cdata, int max)
{
	unsigned short* data = (unsigned short*)cdata;
	int l = 1;
	while (l < max/2 && data[0] == data[l]) l++;
	return l * 2;
}

int encodeLinearRLE16Frame(unsigned char* data, unsigned char* src, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int l = runlength16(src + ofs, (ofs + 65535) < pixels ? pixels - ofs : 65535);
		data[out++] = (l / 2); // value of 0 means run 0
		data[out++] = (l / 2) >> 8;
		data[out++] = src[ofs];
		data[out++] = src[ofs + 1];
		ofs += l;

		if (ofs < pixels)
		{
			// skip
			// one run + copy segment costs at least 6 bytes, so copy until at least 6 byte run is found
			l = 0;
			while (l < 255 && ofs + l + 6 < pixels && runlength16(src + ofs + l, 3) < 6)
				l++;
			if (ofs + l + 6 >= pixels)
				l = pixels - ofs;
			data[out++] = l;
			for (int i = 0; i < l; i++)
			{
				data[out++] = src[ofs];
				ofs++;
			}
		}
	}
	return out;
}

int skiplength(unsigned char* data, unsigned char* prev, int max)
{
	int l = 0;
	while (l < max && data[l] == prev[l]) l++;
	return l;
}

int encodeLinearDelta8Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;

	while (ofs < pixels)
	{
		// skip
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
			int ls = skiplength(aFrame + ofs, aPrev + ofs, ofs + 128 > pixels ? pixels - ofs : 128);
			// copy
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


int skiplength16(unsigned char* cdata, unsigned char* cprev, int max)
{
	int l = 0;
	unsigned short* data = (unsigned short*)cdata;
	unsigned short* prev = (unsigned short*)cprev;
	while (l < max/2 && data[l] == prev[l]) l++;
	return l * 2;
}


int encodeLinearDelta16Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// skip
		int ls = skiplength16(aFrame + ofs, aPrev + ofs, ofs + 65536 > pixels ? pixels - ofs : 65536);
		int lr = runlength16(aFrame + ofs, ofs + 65534 > pixels ? pixels - ofs : 65534);
		if (lr > ls)
		{
			data[out++] = (lr / 2);
			data[out++] = (lr / 2) >> 8;
			data[out++] = aFrame[ofs];
			data[out++] = aFrame[ofs+1];
			ofs += lr;
		}
		else
		{
			data[out++] = (-ls / 2) & 0xff;
			data[out++] = ((-ls / 2) >> 8) & 0xff;
			ofs += ls;
		}

		if (ofs < pixels)
		{
			// copy
			// one skip + copy segment costs at least 6 bytes, so skip until at least 6 byte run is found
			int l = 0;
			while (l < 255 && ofs + l + 6 < pixels && 
				skiplength16(aFrame + ofs + l, aPrev + ofs + l, 6) < 6 && 
				runlength16((unsigned char*)aFrame + ofs + l, 6) < 6) 
				l+=2;

			if (ofs + l + 6 >= pixels)
				l = pixels - ofs;
			data[out++] = l;

			for (int i = 0; i < l; i++)
			{
				data[out++] = aFrame[ofs];
				ofs++;
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

int bestLZRun1(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
		if (aFrame[ofs + bestlen] == aPrev[i + bestlen])
		{
			int l = 0;
			while (l < 255 && (ofs + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
			{
				l++;
			}

			if (l > bestlen)
			{
				bestlen = l;
				bestofs = i;
				if (bestlen > 254)
				{
					runofs = bestofs;
					return 255;
				}
			}
		}
	}
	runofs = bestofs;
	return bestlen;
}

int encodeLZ1Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int o = 0;
		int l = bestLZRun1(aFrame, aPrev, ofs, pixels, o);
		data[out++] = (o >> 0) & 0xff;
		data[out++] = (o >> 8) & 0xff;
		data[out++] = l;

		ofs += l;

		if (ofs < pixels)
		{
			// can we run?
			l = runlength(aFrame + ofs, ofs + 128 > pixels ? pixels - ofs : 128);
			if (l > 2)
			{
				// run
				data[out++] = -l;
				data[out++] = aFrame[ofs];
				ofs += l;
			}
			else
			{
				// copy
				// one run + skip segment costs at least 5 bytes, so skip until at least 5 byte run is found
				l = 0;
				while (l < 127 && ofs + l < pixels && bestLZRun0(aFrame, aPrev, ofs + l, pixels, o, 5) < 5) l++;
				data[out++] = l;
				for (int i = 0; i < l; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
			}
		}
	}

	return out;
}

int bestLZRun2(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
		if (aFrame[ofs + bestlen] == aPrev[i + bestlen])
		{
			int l = 0;
			while (aFrame[ofs + l] == aPrev[i + l] && (ofs + l) < pixels)
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
	runofs = bestofs;
	return bestlen;
}

int encodeLZ2Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int o = 0;
		int l = bestLZRun2(aFrame, aPrev, ofs, pixels, o);
		data[out++] = (o >> 0) & 0xff;
		data[out++] = (o >> 8) & 0xff;
		data[out++] = (l >> 0) & 0xff;
		data[out++] = (l >> 8) & 0xff;

		ofs += l;

		if (ofs < pixels)
		{
			// can we run?
			l = runlength(aFrame + ofs, ofs + 128 > pixels ? pixels - ofs : 128);
			if (l > 2)
			{
				// run
				data[out++] = -l;
				data[out++] = aFrame[ofs];
				ofs += l;
			}
			else
			{
				// copy
				// one run + skip segment costs at least 6 bytes, so skip until at least 6 byte run is found
				l = 0;
				while (l < 127 && ofs + l < pixels && bestLZRun0(aFrame, aPrev, ofs + l, pixels, o, 6) < 6) l++;
				data[out++] = l;
				for (int i = 0; i < l; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
			}
		}
	}

	return out;
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
		// run
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
			int l = 0;
			while (l < 255 && ofs + l + 5 < pixels && 
				bestLZRun0(aFrame, aPrev, ofs + l, pixels, o, 5) < 5 &&
				runlength(aFrame + ofs + l, 5) < 5) l++;
			if (ofs + l + 5 > pixels)
				l = pixels - ofs;
			if (l > 255) l = 255;
			data[out++] = l;
			for (int i = 0; i < l; i++)
			{
				data[out++] = aFrame[ofs];
				ofs++;
			}
		}
	}

	return out;
}

int bestLZRun2b(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
		if (aFrame[ofs + bestlen] == aPrev[i + bestlen])
		{
			int l = 0;
			while (l < 32767 && aFrame[ofs + l] == aPrev[i + l] && (ofs + l) < pixels)
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
	runofs = bestofs;
	return bestlen;
}

int encodeLZ2bFrame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int o = 0;
		int lz = bestLZRun2b(aFrame, aPrev, ofs, pixels, o);
		int lr = runlength(aFrame + ofs, ofs + 127 > pixels ? pixels - ofs : 127);
		if (lr >= lz)
		{
			data[out++] = lr;
			data[out++] = aFrame[ofs];
			ofs += lr;
		}
		else
		{
			
			data[out++] = ((-lz) >> 8) & 0xff; // note: reversed order
			data[out++] = ((-lz) >> 0) & 0xff;
			data[out++] = (o >> 0) & 0xff;
			data[out++] = (o >> 8) & 0xff;
			ofs += lz;
		}

		if (ofs < pixels)
		{
			// copy
			// one run + skip segment costs at least 6 bytes, so skip until at least 6 byte run is found
			int l = 0;
			while (l < 255 && ofs + l < pixels && bestLZRun0(aFrame, aPrev, ofs + l, pixels, o, 6) < 6) l++;
			data[out++] = l;
			for (int i = 0; i < l; i++)
			{
				data[out++] = aFrame[ofs];
				ofs++;
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
			while (l < max && (ofs + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
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

int encodeLZ3Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		signed char o = 0;
		int lz = bestLZRun3(aFrame, aPrev, ofs, pixels, o, ofs + 127 > pixels ? pixels - ofs : 127);
		int lr = runlength(aFrame + ofs, ofs + 128 > pixels ? pixels - ofs : 128);
		if (lr > lz)
		{
			data[out++] = -lr;
			data[out++] = aFrame[ofs];
			ofs += lr;
		}
		else
		{
			data[out++] = lz;
			data[out++] = o;
			ofs += lz;
		}
		
		if (ofs < pixels)
		{	
			// copy
			// one run + skip segment costs at least 4 bytes, so skip until at least 4 byte run is found
			int l = 0;
			while (l < 255 && ofs + l + 4 < pixels && 
				bestLZRun3(aFrame, aPrev, ofs + l, pixels, o, 4) < 4 &&
				runlength(aFrame + ofs + l, 4) < 4) 
				l++;
			if (ofs + l + 4 > pixels)
				l = pixels - ofs;
			if (l > 255) l = 255;
			data[out++] = l;
			for (int i = 0; i < l; i++)
			{
				data[out++] = aFrame[ofs];
				ofs++;
			}
		}
	}

	return out;
}
