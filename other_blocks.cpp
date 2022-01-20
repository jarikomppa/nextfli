#include "nextfli.h"



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
		// run/lz
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
			int lz = bestLZRun3(aFrame, aPrev, ofs, pixels, o, ofs + 127 > pixels ? pixels - ofs : 127);

			if (lz < 4)
			{
				// lz/copy
				// one run + skip segment costs at least 4 bytes, so skip until at least 4 byte run is found			
				int lc = 0;
				while (lc < 128 && ofs + lc + 4 < pixels &&
					bestLZRun3(aFrame, aPrev, ofs + lc, pixels, o, 4) < 4 &&
					runlength(aFrame + ofs + lc, 4) < 4)
					lc++;
				if (ofs + lc + 4 > pixels)
					lc = pixels - ofs;
				
				data[out++] = -lc;
				for (int i = 0; i < lc; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
			}
			else
			{
				data[out++] = lz;
				data[out++] = o;
				ofs += lz;
			}
		}
	}

	return out;
}
