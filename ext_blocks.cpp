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
		// run
		int l = runlength(src + ofs, (ofs + 255) > pixels ? pixels - ofs : 255);
		data[out++] = l; // value of 0 means run of 0 bytes 
		data[out++] = src[ofs]; // could skip this on 0, but that hardly ever happens
		ofs += l;

		if (ofs < pixels)
		{
			// copy
			// one run + copy segment costs at least 3 bytes, so skip until at least 3 byte run is found
			l = 0;
			while (l < 255 && ofs + l + 3 < pixels && runlength(src + ofs + l, 3) < 3)
				l++;

			if (ofs + l + 3 >= pixels)
				l = pixels - ofs;

			if (l > 255) l = 255;

			data[out++] = l;

			for (int i = 0; i < l; i++)
			{
				data[out++] = src[ofs++];
			}
		}
	}
	return out;
}

int runlength16(unsigned short* data, int max)
{
	int l = 1;
	while (l < max && data[0] == data[l]) l++;
	return l;
}

int encodeLinearRLE16Frame(unsigned short* data, unsigned short* src, int pixels)
{
	pixels /= 2;
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int l = runlength16(src + ofs, (ofs + 65535) < pixels ? pixels - ofs : 65535);
		data[out++] = l; // value of 0 means run 0
		data[out++] = src[ofs];
		ofs += l;

		if (ofs < pixels)
		{
			// skip
			// one run + skip segment costs at least 3 bytes, so skip until at least 3 byte run is found
			l = 0;
			while (l < 65535 && ofs + l + 3 < pixels && runlength16(src + ofs + l, 3) < 3)
				l++;
			if (ofs + l + 3 >= pixels)
				l = pixels - ofs;
			data[out++] = l;
			for (int i = 0; i < l; i++)
			{
				data[out++] = src[ofs];
				ofs++;
			}
		}
	}
	return out * 2;
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
		int l = skiplength(aFrame + ofs, aPrev + ofs, ofs + 255 > pixels ? pixels - ofs : 255);
		data[out] = l; // value of 0 means run of 0 bytes 
		out++;
		ofs += l;

		if (ofs < pixels)
		{
			// can we run?
			int l = runlength(aFrame + ofs, ofs + 128 > pixels ? pixels - ofs : 128);
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
				// one skip + copy segment costs at least 2 bytes, so skip until at least 2 byte run is found
				l = 0;
				while (l < 127 && ofs + l + 2 < pixels && skiplength(aFrame + ofs + l, aPrev + ofs + l, 2) < 2) l++;
								
				if (ofs + l + 2 >= pixels)
					l = pixels - ofs;
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


int skiplength16(unsigned short* data, unsigned short* prev, int max)
{
	int l = 0;
	while (l < max && data[l] == prev[l]) l++;
	return l;
}


int encodeLinearDelta16Frame(unsigned short* data, unsigned short* aFrame, unsigned short* aPrev, int pixels)
{
	pixels /= 2;
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// skip
		int l = skiplength16(aFrame + ofs, aPrev + ofs, ofs + 65535 > pixels ? pixels - ofs : 65535);
		data[out++] = l; // value of 0 means run of 0 bytes 
		ofs += l;

		if (ofs < pixels)
		{
			// can we run?
			l = runlength16(aFrame + ofs, ofs + 32768 > pixels ? pixels - ofs : 32768);
			if (l > 2)
			{
				// run
				data[out++] = -l;
				data[out++] = aFrame[ofs];
				ofs += l;
			}
			else
			{
				// skip
				// one run + skip segment costs at least 2 bytes, so skip until at least 2 byte run is found
				l = 0;
				while (l < 32767 && ofs + l + 2 < pixels && skiplength16(aFrame + ofs + l, aPrev + ofs + l, 2) < 2) l++;
				if (ofs + l + 2 >= pixels)
					l = pixels - ofs;
				data[out++] = l;
				for (int i = 0; i < l; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
			}
		}
	}
	return out * 2;
}

int bestLZRun0(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs, int max)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
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
	}
	runofs = bestofs;
	return bestlen;
}

int encodeLZ1Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
//	auto start = std::chrono::steady_clock::now();
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
		//printf("%d\n", l);

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
//	auto end = std::chrono::steady_clock::now();
//	std::chrono::duration<double> elapsed_seconds = end - start;
	//printf("Time elapsed: %3.3fs - %d bytes\n", elapsed_seconds.count(), out);

	return out;
}

int bestLZRun2(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
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
	}
	runofs = bestofs;
	return bestlen;
}

int encodeLZ2Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	//	auto start = std::chrono::steady_clock::now();
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
		//printf("%d\n", l);

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
	//	auto end = std::chrono::steady_clock::now();
	//	std::chrono::duration<double> elapsed_seconds = end - start;
		//printf("Time elapsed: %3.3fs - %d bytes\n", elapsed_seconds.count(), out);

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
	}
	runofs = bestofs - ofs;
	return bestlen;
}

int encodeLZ3Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	//	auto start = std::chrono::steady_clock::now();
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		signed char o = 0;
		int l = bestLZRun3(aFrame, aPrev, ofs, pixels, o, 255);
		data[out++] = o;		
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
				// one run + skip segment costs at least 4 bytes, so skip until at least 4 byte run is found
				l = 0;
				while (l < 127 && ofs + l < pixels && bestLZRun3(aFrame, aPrev, ofs + l, pixels, o, 4) < 4) l++;
				data[out++] = l;
				for (int i = 0; i < l; i++)
				{
					data[out++] = aFrame[ofs];
					ofs++;
				}
			}
		}
	}
	//	auto end = std::chrono::steady_clock::now();
	//	std::chrono::duration<double> elapsed_seconds = end - start;
		//printf("Time elapsed: %3.3fs - %d bytes\n", elapsed_seconds.count(), out);

	return out;
}
