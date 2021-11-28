#include <stdio.h>
#include "nextfli.h"

int encodeLinearRLE8Frame(unsigned char* data, unsigned char* src, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int l = 0;
		while (l < 255 && ofs + l < pixels && src[ofs] == src[ofs + l]) l++;
		data[out++] = l; // value of 0 means run of 0 bytes 
		data[out++] = src[ofs];
		ofs += l;

		if (ofs < pixels)
		{
			// copy
			// one run + copy segment costs at least 3 bytes, so skip until at least 3 byte run is found
			l = 0;
			while (l < 255 && ofs + l + 3 < pixels && 
				!(src[ofs + l] == src[ofs + l + 1] && src[ofs + l] == src[ofs + l + 2] && src[ofs + l] == src[ofs + l + 3])) 
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

int encodeLinearRLE16Frame(unsigned short* data, unsigned short* src, int pixels)
{
	pixels /= 2;
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int l = 0;
		while (l < 65536 && ofs + l < pixels && src[ofs] == src[ofs + l]) l++;
		data[out] = l; // value of 0 means run of 65536 bytes 
		out++;
		data[out] = src[ofs];
		out++;
		ofs += l;

		if (ofs < pixels)
		{
			// skip
			// one run + skip segment costs at least 3 bytes, so skip until at least 3 byte run is found
			l = 0;
			while (l < 256 && ofs + l + 3 < pixels && !(src[ofs + l] == src[ofs + l + 1] && src[ofs + l] == src[ofs + l + 2] && src[ofs + l] == src[ofs + l + 3])) l++;
			if (ofs + l + 3 >= pixels)
				l = pixels - ofs;
			data[out] = l;
			out++;
			for (int i = 0; i < l; i++)
			{
				data[out] = src[ofs];
				out++;
				ofs++;
			}
		}
	}
	return out * 2;
}

int encodeLinearDelta8Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels)
{
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int l = 0;
		while (l < 255 && ofs + l < pixels && aFrame[ofs + l] == aPrev[ofs + l]) l++;
		data[out] = l; // value of 0 means run of 0 bytes 
		out++;
		ofs += l;

		if (ofs < pixels)
		{
			// copy
			// one run + skip segment costs at least 2 bytes, so skip until at least 2 byte run is found
			l = 0;
			while (l < 255 && ofs + l + 2 < pixels && !(aFrame[ofs + l] == aPrev[ofs + l] && aFrame[ofs + l + 1] == aPrev[ofs + l + 1])) l++;
			if (ofs + l + 2 >= pixels)
				l = pixels - ofs;
			data[out] = l;
			out++;
			for (int i = 0; i < l; i++)
			{
				data[out] = aFrame[ofs];
				out++;
				ofs++;
			}
		}
	}
	return out;
}

int encodeLinearDelta16Frame(unsigned short* data, unsigned short* aFrame, unsigned short* aPrev, int pixels)
{
	pixels /= 2;
	int ofs = 0;
	int out = 0;
	while (ofs < pixels)
	{
		// run
		int l = 0;
		while (l < 65535 && ofs + l < pixels && aFrame[ofs + l] == aPrev[ofs + l]) l++;
		data[out] = l; // value of 0 means run of 0 bytes 
		out++;
		ofs += l;

		if (ofs < pixels)
		{
			// skip
			// one run + skip segment costs at least 2 bytes, so skip until at least 2 byte run is found
			l = 0;
			while (l < 65535 && ofs + l + 2 < pixels && !(aFrame[ofs + l] == aPrev[ofs + l] && aFrame[ofs + l + 1] == aPrev[ofs + l + 1])) l++;
			if (ofs + l + 2 >= pixels)
				l = pixels - ofs;
			data[out] = l;
			out++;
			for (int i = 0; i < l; i++)
			{
				data[out] = aFrame[ofs];
				out++;
				ofs++;
			}
		}
	}
	return out * 2;
}

int bestLZRun0(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs)
{
	int bestlen = 0;
	int bestofs = 0;
	for (int i = 0; i < pixels - bestlen; i++)
	{
		{
			if (aFrame[ofs + bestlen] == aPrev[i + bestlen])
			{
				int l = 0;
				while (l < 10 && (ofs + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
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
		data[out] = (o >> 0) & 0xff;
		out++;
		data[out] = (o >> 8) & 0xff;
		out++;
		data[out] = l;
		out++;
		//printf("%d\n", l);

		ofs += l;

		if (ofs < pixels)
		{
			// copy
			// one run + skip segment costs at least 4 bytes, so skip until at least 6 byte run is found			
			l = 0;
			while (l < 255 && ofs + l < pixels && bestLZRun0(aFrame, aPrev, ofs + l, pixels, o) < 6) l++;
			data[out] = l;
			out++;
			for (int i = 0; i < l; i++)
			{
				data[out] = aFrame[ofs];
				out++;
				ofs++;
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
		data[out] = (o >> 0) & 0xff;
		out++;
		data[out] = (o >> 8) & 0xff;
		out++;
		data[out] = (l >> 0) & 0xff;
		out++;
		data[out] = (l >> 8) & 0xff;
		out++;
		//printf("%d\n", l);

		ofs += l;

		if (ofs < pixels)
		{
			// copy
			// one run + skip segment costs at least 5 bytes, so skip until at least 7 byte run is found
			l = 0;
			while (l < 255 && ofs + l < pixels && bestLZRun0(aFrame, aPrev, ofs + l, pixels, o) < 7) l++;
			data[out] = l;
			out++;
			for (int i = 0; i < l; i++)
			{
				data[out] = aFrame[ofs];
				out++;
				ofs++;
			}
		}
	}
	//	auto end = std::chrono::steady_clock::now();
	//	std::chrono::duration<double> elapsed_seconds = end - start;
		//printf("Time elapsed: %3.3fs - %d bytes\n", elapsed_seconds.count(), out);

	return out;
}

int bestLZRun3(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, signed char& runofs)
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
				while (l < 255 && (ofs + l) < pixels && aFrame[ofs + l] == aPrev[i + l])
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
		int l = bestLZRun3(aFrame, aPrev, ofs, pixels, o);
		data[out] = o;
		out++;
		data[out] = l;
		out++;
		//printf("%d\n", l);

		ofs += l;

		if (ofs < pixels)
		{
			// copy
			// one run + skip segment costs at least 3 bytes, so skip until at least 5 byte run is found
			l = 0;
			while (l < 255 && ofs + l < pixels && bestLZRun3(aFrame, aPrev, ofs + l, pixels, o) < 5) l++;
			data[out] = l;
			out++;
			for (int i = 0; i < l; i++)
			{
				data[out] = aFrame[ofs];
				out++;
				ofs++;
			}
		}
	}
	//	auto end = std::chrono::steady_clock::now();
	//	std::chrono::duration<double> elapsed_seconds = end - start;
		//printf("Time elapsed: %3.3fs - %d bytes\n", elapsed_seconds.count(), out);

	return out;
}
