#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SOL_QMEDIAN_IMPLEMENTATION
#include "sol_qmedian.h"

#include "threadpool.h"

#include "nextfli.h"


// ba
int framestep = 1;
int frames = 2037;
// cube
//int framestep = 1;
//int frames = 240;
// ba_hvy
//int framestep = 2;
//int frames = 3671;


Frame* gRoot = NULL, * gLast = NULL;
int gHalfRes = 0;
int gPointSample = 0;
int gDither = 0;
int gExtendedBlocks = 1;
int gSlowBlocks = 1;

class AddFrameTask : public Thread::PoolTask
{
public:
	Frame* fr;
	char* fn;
	const FliHeader& header;

	AddFrameTask(char* aFn, Frame* aFr, const FliHeader& aHeader) : fr(aFr), header(aHeader)
	{
		fn = _strdup(aFn);
	}
	
	virtual ~AddFrameTask()
	{
		free(fn);
	}

	virtual void work()
	{
		int n, x, y;
		unsigned int* data = (unsigned int*)stbi_load(fn, &x, &y, &n, 4);

		int xofs = 0;
		int yofs = 0;
		int ht = 0;
		int wd = 0;

		if ((float)y / x <= (float)header.mHeight / header.mWidth)
		{
			wd = 256;
			ht = 256 * y / x;
		}
		else
		{
			wd = 192 * x / y;
			ht = wd * y / x;
		}

		if (gHalfRes)
		{
			wd /= 2;
			ht /= 2;
		}

		xofs = (header.mWidth - wd) / 2;
		yofs = (header.mHeight - ht) / 2;


		memset(fr->mRgbPixels, 0, sizeof(unsigned int) * header.mHeight * header.mWidth);

		if (gPointSample)
		{
			for (int i = 0; i < ht; i++)
			{
				for (int j = 0; j < wd; j++)
				{
					int p = data[(((i * y) / ht) * x + ((j * x) / wd))];
					fr->mRgbPixels[(i + yofs) * header.mWidth + j + xofs] = p;
				}
			}
		}
		else
		{
			int cx = x / wd;
			int cy = y / ht;
			for (int i = 0; i < ht; i++)
			{
				for (int j = 0; j < wd; j++)
				{					
					int c = 0;
					int r = 0, g = 0, b = 0;
					for (int k = 0; k < cy; k++)
					{
						for (int l = 0; l < cx; l++, c++)
						{
							unsigned int p = data[(((i * y) / ht + k) * x + ((j * x) / wd + l))];
							r += (p >> 16) & 0xff;
							g += (p >> 8) & 0xff;
							b += (p >> 0) & 0xff;
						}
					}

					r /= c;
					g /= c;
					b /= c;

					int p = (r << 16) | (g << 8) | (b << 0);
					fr->mRgbPixels[(i + yofs) * header.mWidth + j + xofs] = p;
				}
			}
		}
		stbi_image_free((void*)data);
	}
};

void addFrame(char* fn, const FliHeader& header, Thread::Pool &threadpool)
{
	Frame* fr = new Frame;
	fr->mRgbPixels = new unsigned int[header.mHeight * header.mWidth];

	if (gRoot == NULL)
	{
		gRoot = fr;
		gLast = fr;
		return;
	}
	gLast->mNext = fr;
	gLast = fr;

	AddFrameTask* t = new AddFrameTask(fn, fr, header);
	t->mDeleteTask = 1;
	threadpool.addWork(t);
}

void loadframes(const FliHeader& header)
{
	printf("Loading frames..\n");
	auto start = std::chrono::steady_clock::now();
	Thread::Pool threadpool;
	threadpool.init(24);
	for (int i = 0; i < header.mFrames; i++)
	{
		char temp[100];
		sprintf(temp, "ba/ba%04d.png", (i * framestep) + 1);
		//sprintf(temp, "cube/256x192__%04d_cube%04d.png.png", (i * framestep), (i * framestep) + 1);
		//sprintf(temp, "cube/cube%04d.png", (i * framestep) + 1);
		//sprintf(temp, "ba_hvy/frame%04d.png", (i * framestep) + 1);
		printf("Loading %s                    \r", temp);
		addFrame(temp, header, threadpool);
	}
	printf("\nWaiting for threads..\n");
	threadpool.waitUntilDone();
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("Time elapsed: %3.3fs\n\n", elapsed_seconds.count());
}

/*
float dithermatrix8x8[] =
{
	0.0f, 32.0f, 8.0f, 40.0f, 2.0f, 34.0f, 10.0f, 42.0f,
	48.0f, 16.0f, 56.0f, 24.0f, 50.0f, 18.0f, 58.0f, 26.0f,
	12.0f, 44.0f, 4.0f, 36.0f, 14.0f, 46.0f, 6.0f, 38.0f,
	60.0f, 28.0f, 52.0f, 20.0f, 62.0f, 30.0f, 54.0f, 22.0f,
	3.0f, 35.0f, 11.0f, 43.0f, 1.0f, 33.0f, 9.0f, 41.0f,
	51.0f, 19.0f, 59.0f, 27.0f, 49.0f, 17.0f, 57.0f, 25.0f,
	15.0f, 47.0f, 7.0f, 39.0f, 13.0f, 45.0f, 5.0f, 37.0f,
	63.0f, 31.0f, 55.0f, 23.0f, 61.0f, 29.0f, 53.0f, 21.0f
};

The following is the above /63.0f-0.5f)/8.0f
*/

float dithermatrix8x8[] =
{
	-0.062500f,  0.000992f, -0.046627f,  0.016865f, -0.058532f,  0.004960f, -0.042659f,  0.020833f, 
	 0.032738f, -0.030754f,  0.048611f, -0.014881f,  0.036706f, -0.026786f,  0.052579f, -0.010913f,
	-0.038690f,  0.024802f, -0.054563f,  0.008929f, -0.034722f,  0.028770f, -0.050595f,  0.012897f,
	 0.056548f, -0.006944f,  0.040675f, -0.022817f,  0.060516f, -0.002976f,  0.044643f, -0.018849f,
	-0.056548f,  0.006944f, -0.040675f,  0.022817f, -0.060516f,  0.002976f, -0.044643f,  0.018849f,
	 0.038690f, -0.024802f,  0.054563f, -0.008929f,  0.034722f, -0.028770f,  0.050595f, -0.012897f,
	-0.032738f,  0.030754f, -0.048611f,  0.014881f, -0.036706f,  0.026786f, -0.052579f,  0.010913f,
	 0.062500f, -0.000992f,  0.046627f, -0.016865f,  0.058532f, -0.004960f,  0.042659f, -0.020833f
};

class QuantizeTask : public Thread::PoolTask
{
public:
	Frame* mFrame;
	const FliHeader& mHeader;
	unsigned int* colors;
	QuantizeTask(Frame* aFrame, const FliHeader& aHeader, unsigned int *aColors) : mFrame(aFrame), mHeader(aHeader), colors(aColors)
	{}

	virtual ~QuantizeTask()
	{}

	virtual void work()
	{
		int pixels = mHeader.mWidth * mHeader.mHeight;
		mFrame->mIndexPixels = new unsigned char[pixels];

		if (gDither)
		{
			for (int i = 0, c = 0; i < mHeader.mHeight; i++)
			{
				for (int j = 0; j < mHeader.mWidth; j++, c++)
				{
					int p = mFrame->mRgbPixels[c];
					int r = ((p >> 16) & 0xff);
					int g = ((p >> 8) & 0xff);
					int b = ((p >> 0) & 0xff);

					r += (int)floor(dithermatrix8x8[(i % 8) * 8 + (j % 8)] * (float)r);
					g += (int)floor(dithermatrix8x8[(i % 8) * 8 + (j % 8)] * (float)g);
					b += (int)floor(dithermatrix8x8[(i % 8) * 8 + (j % 8)] * (float)b);

					if (r < 0) r = 0;
					if (g < 0) g = 0;
					if (b < 0) b = 0;
					if (r > 0xff) r = 0xff;
					if (g > 0xff) g = 0xff;
					if (b > 0xff) b = 0xff;

					p = (r << 16) | (g << 8) | (b << 0);
					mFrame->mRgbPixels[c] = p;
				}
			}
		}

		for (int i = 0; i < pixels; i++)
		{
			int c = mFrame->mRgbPixels[i];
			// Map to 512 color space
			mFrame->mRgbPixels[i] = (((c >> 5) & 0x7) << 0) | (((c >> 13) & 0x7) << 3) | (((c >> 21) & 0x7) << 6);
			// collect used colors
			colors[mFrame->mRgbPixels[i]] = (c & 0xe0e0e0) >> 5;
		}
	}
};

void quantize(const FliHeader& header)
{
	auto start = std::chrono::steady_clock::now();
	Frame* walker = gRoot;
	unsigned int colors[512];
	memset(colors, 0xff, 512 * sizeof(unsigned int));
	printf("Dithering and reducing..\n");
	Thread::Pool threadpool;
	threadpool.init(24);

	while (walker)
	{
		QuantizeTask* t = new QuantizeTask(walker, header, colors);
		t->mDeleteTask = 1;
		threadpool.addWork(t);
		walker = walker->mNext;
	}

	printf("Waiting for threads..\n");
	threadpool.waitUntilDone();

	int cc = 0;
	for (int i = 0; i < 512; i++)
		if (colors[i] != 0xffffffff) cc++;
	printf("%d unique colors\n", cc + 1);

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("Time elapsed: %3.3fs\n\n", elapsed_seconds.count());
	start = std::chrono::steady_clock::now();


	printf("Quantizing and remapping..\n");

	SQ* q = sq_alloc();	
	sq_addcolormap(q, (unsigned char*)&colors, 512, 4);
	unsigned char* idxmap;
	unsigned char* pal;
	unsigned int intpal[256];
	int idxs;
	int ofs = sq_reduce(q, &idxmap, &pal, &idxs, 256);

	for (int i = 0; i < 256; i++)
	{
		intpal[i] = (pal[i * 3 + 0] << (0 + 5)) | (pal[i * 3 + 1] << (8 + 5)) | (pal[i * 3 + 2] << (16 + 5));
	}

	walker = gRoot;
	while (walker)
	{
		memcpy(walker->mPalette, intpal, sizeof(int) * 256);
		int pixels = header.mWidth * header.mHeight;
		for (int i = 0; i < pixels; i++)
		{
			walker->mIndexPixels[i] = idxmap[walker->mRgbPixels[i]];
/*
			int l = walker->mRgbPixels[i];
			l = (((l & 0xff) + ((l >> 8) & 0xff) + ((l >> 16) & 0xff)) / 3) >> 5;
			walker->mIndexPixels[i] = l;
*/
		}
		delete walker->mRgbPixels;
		walker->mRgbPixels = 0;
		walker = walker->mNext;
	}
	free(idxmap);
	free(pal);
	end = std::chrono::steady_clock::now();
	elapsed_seconds = end - start;
	printf("Time elapsed: %3.3fs\n\n", elapsed_seconds.count());
}



class EncodeTask : public Thread::PoolTask
{
public:
	Frame* mFrame, * mPrev;
	const FliHeader& mHeader;
	EncodeTask(Frame* aFrame, Frame* aPrev, const FliHeader& aHeader) : mFrame(aFrame), mPrev(aPrev), mHeader(aHeader)
	{}

	virtual ~EncodeTask()
	{}

	virtual void work()
	{
		int pixels = mHeader.mWidth * mHeader.mHeight;
		
		int allzero = 1;
		for (int i = 0; allzero && i < pixels; i++)
			allzero = (mFrame->mIndexPixels[i] == 0);
		if (allzero)
		{
			mFrame->mFrameType = BLACKFRAME;
			verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
			return;
		}

		if (mPrev)
		{
			// Check if previous frame is identical to current
			int identical = 1;
			for (int i = 0; identical && i < pixels; i++)
				identical = (mFrame->mIndexPixels[i] == mPrev->mIndexPixels[i]);
			if (identical)
			{
				mFrame->mFrameType = SAMEFRAME;
				verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				return;
			}
		}

		if (gExtendedBlocks)
		{
			int allsingle = 1;
			for (int i = 0; allsingle && i < pixels; i++)
				allsingle = (mFrame->mIndexPixels[i] == mFrame->mIndexPixels[0]);
			if (allsingle)
			{
				mFrame->mFrameType = ONECOLOR;
				mFrame->mFrameData = new unsigned char[1];
				mFrame->mFrameDataSize = 1;
				mFrame->mFrameData[0] = mFrame->mIndexPixels[0];
				verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				return;
			}
		}

		mFrame->mFrameData = new unsigned char[pixels*2];
		mFrame->mFrameDataSize = encodeRLEFrame(mFrame->mFrameData, mFrame->mIndexPixels, mHeader.mWidth, mHeader.mHeight);
		if (mFrame->mFrameDataSize > pixels) printf("overlong rle: %d +%d\n", mFrame->mFrameDataSize, mFrame->mFrameDataSize-pixels);
		mFrame->mFrameType = RLEFRAME;
		verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
		unsigned char* data;
		int len;

		if (gExtendedBlocks)
		{
			data = new unsigned char[pixels * 2];
			len = encodeLinearRLE8Frame(data, mFrame->mIndexPixels, pixels);
			if (len > pixels) printf("overlong Lrle8 %d +%d\n", len, len - pixels);
			if (len < mFrame->mFrameDataSize)
			{
				delete[] mFrame->mFrameData;
				mFrame->mFrameData = data;
				mFrame->mFrameDataSize = len;
				mFrame->mFrameType = LINEARRLE8;
				verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
			}
			else
			{
				delete[] data;
			}

			data = new unsigned char[pixels * 2];
			len = encodeLinearRLE16Frame((unsigned short*)data, (unsigned short*)(mFrame->mIndexPixels), pixels);
			if (len > pixels) printf("overlong Lrle16 %d +%d\n", len, len-pixels);
			if (len < mFrame->mFrameDataSize)
			{
				delete[] mFrame->mFrameData;
				mFrame->mFrameData = data;
				mFrame->mFrameDataSize = len;
				mFrame->mFrameType = LINEARRLE16;
				verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
			}
			else
			{
				delete[] data;
			}
		}

		if (mPrev)
		{
			if (gExtendedBlocks)
			{
				data = new unsigned char[pixels * 2];
				len = encodeLinearDelta8Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
				if (len > pixels) printf("overlong Ldelta8 %d +%d\n", len, len - pixels);

				if (len < mFrame->mFrameDataSize)
				{
					delete[] mFrame->mFrameData;
					mFrame->mFrameData = data;
					mFrame->mFrameDataSize = len;
					mFrame->mFrameType = LINEARDELTA8;
					verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				}
				else
				{
					delete[] data;
				}

				data = new unsigned char[pixels * 2];
				len = encodeLinearDelta16Frame((unsigned short*)data, (unsigned short*)mFrame->mIndexPixels, (unsigned short*)mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
				if (len > pixels) printf("overlong Ldelta16 %d +%d\n", len, len - pixels);

				if (len < mFrame->mFrameDataSize)
				{
					delete[] mFrame->mFrameData;
					mFrame->mFrameData = data;
					mFrame->mFrameDataSize = len;
					mFrame->mFrameType = LINEARDELTA16;
					verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				}
				else
				{
					delete[] data;
				}

				if (gSlowBlocks)
				{
					data = new unsigned char[pixels * 2];
					len = encodeLZ1Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
					if (len > pixels) printf("overlong Lz1 %d +%d\n", len, len - pixels);

					if (len < mFrame->mFrameDataSize)
					{
						delete[] mFrame->mFrameData;
						mFrame->mFrameData = data;
						mFrame->mFrameDataSize = len;
						mFrame->mFrameType = LZ1;
						verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
					}
					else
					{
						delete[] data;
					}
				}
			}

			data = new unsigned char[pixels*2];
			len = encodeDelta8Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth, mHeader.mHeight);
			if (len > pixels) printf("overlong delta8 %d +%d\n", len, len - pixels);

			if (len < mFrame->mFrameDataSize)
			{
				delete[] mFrame->mFrameData;
				mFrame->mFrameData = data;
				mFrame->mFrameDataSize = len;
				mFrame->mFrameType = DELTA8FRAME;
				verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
			}
			else
			{
				delete[] data;
			}

			data = new unsigned char[pixels*2];
			len = encodeDelta16Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth, mHeader.mHeight);
			if (len > pixels) printf("overlong delta16 %d +%d\n", len, len - pixels);

			if (len < mFrame->mFrameDataSize)
			{
				delete[] mFrame->mFrameData;
				mFrame->mFrameData = data;
				mFrame->mFrameDataSize = len;
				mFrame->mFrameType = DELTA16FRAME;
				verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
			}
			else
			{
				delete[] data;
			}
		}

		// TODO: start frame from solid fill - pretty much same as linear-rle..
		// TODO: start frame from nudged source - pretty much same as lz
		// TODO: start frame from earlier frame - not sure if this is worth the effort
		// TODO: non-destructive palette swap? - can't be parallelized
	}
};

void encode(const FliHeader& header)
{
	printf("Encoding..\n");
	auto start = std::chrono::steady_clock::now();
	
	
	Thread::Pool threadpool;
	threadpool.init(24);

	Frame* walker = gRoot;
	Frame* prev = 0;
	while (walker)
	{
		EncodeTask* t = new EncodeTask(walker, prev, header);
		t->mDeleteTask = 1;
#ifdef NDEBUG
		threadpool.addWork(t);
#else
//		threadpool.addWork(t);
		t->work();
#endif
		prev = walker;
		walker = walker->mNext;
	}

	printf("Waiting for threads..\n");
	threadpool.waitUntilDone();
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("Time elapsed: %3.3fs\n\n", elapsed_seconds.count());

}

void output(FliHeader& header, FILE* outfile)
{
	printf("Writing file..\n");
	auto start = std::chrono::steady_clock::now();
	fwrite(&header, 1, sizeof(FliHeader), outfile); // unfinished header

	// TODO header.mOframe1 = ftell(outfile); // start of first frame
	// TODO header.mOframe1 = ftell(outfile); // start of second frame

	// TODO: write out whole flc
	Frame* walker = gRoot;
	int framecounts[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	int framemaxsize[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	int frameminsize[16] = { 0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff };
	int total = 0;
	int frames = 0;
	while (walker)
	{
		switch (walker->mFrameType)
		{
		case SAMEFRAME:printf("s"); break;
		case BLACKFRAME:printf("b"); break;
		case RLEFRAME:printf("r"); break;
		case DELTA8FRAME:printf("d"); break;
		case DELTA16FRAME:printf("D"); break;
		case ONECOLOR:printf("o"); break;
		case LINEARRLE8:printf("l"); break;
		case LINEARRLE16:printf("L"); break;
		case LINEARDELTA8:printf("e"); break;
		case LINEARDELTA16:printf("E"); break;
		case LZ1:printf("1"); break;
		default: printf("?!?\n");
		}
		if (walker->mFrameDataSize > framemaxsize[walker->mFrameType]) framemaxsize[walker->mFrameType] = walker->mFrameDataSize;
		if (walker->mFrameDataSize < frameminsize[walker->mFrameType]) frameminsize[walker->mFrameType] = walker->mFrameDataSize;
		framecounts[walker->mFrameType]++;
		total += walker->mFrameDataSize;
		walker = walker->mNext;
		frames++;
	}
	printf("\nTotal %d bytes (%d kB, %d MB)\n", total, total/1024, total/(1024*1024));
	printf("Compression ratio %3.3f%%\n", 100 * total / (float)(frames * header.mWidth * header.mWidth));
	printf("\nBlock types:\n");
	if (framecounts[1]) printf("sameframe     %5d (%5d -%5d bytes)\n", framecounts[1], frameminsize[1], framemaxsize[1]);
	if (framecounts[2]) printf("blackframe    %5d (%5d -%5d bytes)\n", framecounts[2], frameminsize[2], framemaxsize[2]);
	if (framecounts[3]) printf("rleframe      %5d (%5d -%5d bytes)\n", framecounts[3], frameminsize[3], framemaxsize[3]);
	if (framecounts[4]) printf("delta8frame   %5d (%5d -%5d bytes)\n", framecounts[4], frameminsize[4], framemaxsize[4]);
	if (framecounts[5]) printf("delta16frame  %5d (%5d -%5d bytes)\n", framecounts[5], frameminsize[5], framemaxsize[5]);
	printf("-- extended blocks --\n");
	if (framecounts[6]) printf("onecolor      %5d (%5d -%5d bytes)\n", framecounts[6], frameminsize[6], framemaxsize[6]);
	if (framecounts[7]) printf("linearrle8    %5d (%5d -%5d bytes)\n", framecounts[7], frameminsize[7], framemaxsize[7]);
	if (framecounts[8]) printf("linearrle16   %5d (%5d -%5d bytes)\n", framecounts[8], frameminsize[8], framemaxsize[8]);
	if (framecounts[9]) printf("lineardelta8  %5d (%5d -%5d bytes)\n", framecounts[9], frameminsize[9], framemaxsize[9]);
	if (framecounts[10]) printf("lineardelta16 %5d (%5d -%5d bytes)\n", framecounts[10], frameminsize[10], framemaxsize[10]);
	if (framecounts[11]) printf("lz scheme 1   %5d (%5d -%5d bytes)\n", framecounts[11], frameminsize[11], framemaxsize[11]);
	
	header.mSize = ftell(outfile);

	fseek(outfile, 0, SEEK_SET);
	fwrite(&header, 1, sizeof(FliHeader), outfile); // finished header

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("\nTime elapsed: %3.3fs\n\n", elapsed_seconds.count());
}

int main(int argc, char* argv[])
{
	FILE* outfile;
	FliHeader header;
	auto start = std::chrono::steady_clock::now();

	header.mWidth = 256;
	header.mHeight = 192;
	header.mFrames = frames / framestep;


	loadframes(header);
	quantize(header);
	encode(header);

	outfile = fopen("output.flc", "wb");
	output(header, outfile);
	fclose(outfile);
	
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("\nTotal time elapsed: %3.3fs\n", elapsed_seconds.count());

	return 0;
}
