#include "nextfli.h"


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
				data[out++] = l & 0xff;
				data[out++] = (l >> 8) & 0xff;
				data[out++] = src[ofs];
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

int encodeLinearDelta16Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;

	while (ofs < pixels)
	{
		// skip/run
		int ls = skiplength(aFrame + ofs, aPrev + ofs, pixels - ofs);
		int lr = runlength(aFrame + ofs, pixels - ofs);
		if (lr > ls)
		{
			if (lr > 126)
			{
				// go long
				data[out++] = 127;
				data[out++] = (lr >> 0) & 0xff;
				data[out++] = (lr >> 8) & 0xff;
			}
			else
			{
				data[out++] = lr;
			}
			data[out++] = aFrame[ofs];
			ofs += lr;
		}
		else
		{
			if (ls > 127)
			{
				// go long
				data[out++] = -128;
				data[out++] = (ls >> 0) & 0xff;
				data[out++] = (ls >> 8) & 0xff;
			}
			else
			{
				data[out++] = -ls;
			}
			ofs += ls;
		}

		if (ofs < pixels)
		{
			// skip/copy
			int ls = skiplength(aFrame + ofs, aPrev + ofs, pixels - ofs);
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
				if (ls > 127)
				{
					// go long
					data[out++] = -128;
					data[out++] = (ls >> 0) & 0xff;
					data[out++] = (ls >> 8) & 0xff;
				}
				else
				{
					data[out++] = -ls;
				}
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

int encodeLZ3BFrame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
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
		}

		if (ofs < pixels)
		{
			int lz = bestLZRun3(aFrame, aPrev, ofs, pixels, o, pixels - ofs);

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
			}
		}
	}

	return out;
}

int encodeLZ3DFrame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		signed char o;
		int lz = bestLZRun3(aFrame, aPrev, ofs, pixels, o, pixels - ofs);

		if (lz < 4)
		{
			// lz/copy
			// one run + skip segment costs at least 4 bytes, so skip until at least 4 byte run is found			
			int lc = 0;
			while (ofs + lc + 4 < pixels &&
				bestLZRun3(aFrame, aPrev, ofs + lc, pixels, o, 4) < 4)
				lc++;
			if (ofs + lc + 4 >= pixels)
				lc = pixels - ofs;

			if (lc > 127)
			{
				data[out++] = -128;
				data[out++] = (lz >> 0) & 0xff;
				data[out++] = (lz >> 8) & 0xff;
			}
			else
			{
				data[out++] = -lc;
			}
			for (int i = 0; i < lc; i++)
			{
				data[out++] = aFrame[ofs];
				ofs++;
			}
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
		}
	}

	return out;
}


int bestLZRun3e(unsigned char* aFrame, int ofs, int pixels, char& runofs, int max)
{
	int bestlen = 0;
	int bestofs = 0;
	int start = ofs - 255;
	if (start < 0) start = 0;
	int end = ofs;
	if (end > pixels)
		end = pixels - ofs;
	for (int i = start; i < end; i++)
	{
		if (i + bestlen < end && aFrame[ofs + bestlen] == aFrame[i + bestlen])
		{
			int l = 0;
			while (l < max && (ofs + l) < pixels && aFrame[ofs + l] == aFrame[i + l])
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

int encodeLZ3EFrame(unsigned char* data, unsigned char* aFrame, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run/lz
		char o = 0;
		int lz = bestLZRun3e(aFrame, ofs, pixels, o, ofs + 127 > pixels ? pixels - ofs : 127);
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
			int lz = bestLZRun3e(aFrame, ofs, pixels, o, ofs + 127 > pixels ? pixels - ofs : 127);

			if (lz < 4)
			{
				// lz/copy
				// one run + skip segment costs at least 4 bytes, so skip until at least 4 byte run is found			
				int lc = 0;
				while (lc < 128 && ofs + lc + 4 < pixels &&
					bestLZRun3e(aFrame, ofs + lc, pixels, o, 4) < 4 &&
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