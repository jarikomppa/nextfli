// TODO
// - set minimum / maximum run length
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#define SOL_QMEDIAN_IMPLEMENTATION
#include "sol_qmedian.h"

#include "threadpool.h"

#include "nextfli.h"

#include <iostream>
#include "optionparser.h"

#include <windows.h> // findfirst

Frame* gRoot = NULL, * gLast = NULL;
int gHalfRes = 0;
int gPointSample = 0;
int gDither = 0;
int gExtendedBlocks = 0;
int gClassicBlocks = 1;
int gSlowBlocks = 1;
int gVerify = 0;
int gFramedelay = 4;
int gThreads = 0;


int gUseBlack = 1;
int gUseSame = 1;
int gUseSingle = 1;
int gUseRLE = 1;
int gUseDelta8Frame = 1;
int gUseDelta16Frame = 1;

int gUseLRLE8 = 1;
int gUseLRLE16 = 1;
int gUseLDelta8 = 1;
int gUseLDelta16 = 0;
int gUseLZ1 = 0;
int gUseLZ2 = 0;
int gUseLZ1b = 1;
int gUseLZ2b = 0;
int gUseLZ3 = 0;

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
			stbir_resize_uint8((const unsigned char*)data, x, y, x*4,
				(unsigned char*)fr->mRgbPixels + header.mWidth * 4 * yofs + xofs * 4, wd, ht, header.mWidth * 4,
				4);
		}
		stbi_image_free((void*)data);
	}
};

void addFrame(char* fn, const FliHeader& header, Thread::Pool &threadpool)
{
	Frame* fr = new Frame;
	fr->mRgbPixels = new unsigned int[header.mHeight * header.mWidth];
	memset(fr->mRgbPixels, 0, sizeof(int) * header.mHeight * header.mWidth);

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

void loadframes(FliHeader& header, const char *filemask)
{
	printf("Loading frames using filemask \"%s\"\n", filemask);	

	auto start = std::chrono::steady_clock::now();
	Thread::Pool threadpool;
	threadpool.init(gThreads);
	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind;
	hFind = FindFirstFileA(filemask, &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			char temp[512];
			if (strrchr(filemask, '\\') != 0)
			{
				*(char*)(strrchr(filemask, '\\')+1) = 0;
				sprintf(temp, "%s%s", filemask, FindFileData.cFileName);
			}
			else
			if (strrchr(filemask, '/') != 0)
			{
				*(char*)(strrchr(filemask, '/')+1) = 0;
				sprintf(temp, "%s%s", filemask, FindFileData.cFileName);
			}
			else
				sprintf(temp, "%s", FindFileData.cFileName);
			printf("Loading %s                    \r", temp);
			addFrame(temp, header, threadpool);
			header.mFrames++;
		} 
		while (FindNextFileA(hFind, &FindFileData));
		
		FindClose(hFind);
	}
	printf("\nWaiting for threads..\n");
	threadpool.waitUntilDone(100, true);
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
			int r = ((c >> 16) & 0xff);
			int g = ((c >> 8) & 0xff);
			int b = ((c >> 0) & 0xff);
			// rounding
			r = (r + 15) >> 5;
			if (r > 7) r = 7;
			g = (g + 15) >> 5;
			if (g > 7) g = 7;
			b = (b + 15) >> 5;
			if (b > 7) b = 7;

			mFrame->mRgbPixels[i] = (r << 6) | (g << 3) | b;
			// collect used colors
			colors[mFrame->mRgbPixels[i]] = (r << 16) | (g << 8) | (b);
		}
	}
};

void quantize(const FliHeader& header)
{
	auto start = std::chrono::steady_clock::now();
	unsigned int *framecolors;
	framecolors = new unsigned int[512 * header.mFrames];
	memset(framecolors, 0xff, 512 * sizeof(unsigned int) * header.mFrames);
	printf("Dithering and reducing..\n");
	Thread::Pool threadpool;
	threadpool.init(gThreads);

	Frame* walker = gRoot;
	unsigned int* cw = framecolors;
	while (walker)
	{
		QuantizeTask* t = new QuantizeTask(walker, header, cw);
		t->mDeleteTask = 1;
		threadpool.addWork(t);
		walker = walker->mNext;
		cw += 512;
	}

	printf("Waiting for threads..\n");
	threadpool.waitUntilDone(100, true);

	int colors[512];
	memset(colors, 0xff, 512 * sizeof(unsigned int));
	for (int i = 0; i < header.mFrames; i++)
		for (int j = 0; j < 512; j++)
			if (framecolors[i * 512 + j] != 0xffffffff)
				colors[j] = framecolors[i * 512 + j];
	delete[] framecolors;

	int cc = 0;
	for (int i = 0; i < 512; i++)
		if (colors[i] != 0xffffffff)
			cc++;
		else
			colors[i] = 0;
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
		if (allzero && gUseBlack)
		{
			mFrame->mFrameType = BLACKFRAME;
			mFrame->mFrameData = 0;
			mFrame->mFrameDataSize = 0;
			if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
			return;
		}

		if (mPrev && gUseSame)
		{
			// Check if previous frame is identical to current
			int identical = 1;
			for (int i = 0; identical && i < pixels; i++)
				identical = (mFrame->mIndexPixels[i] == mPrev->mIndexPixels[i]);
			if (identical)
			{
				mFrame->mFrameType = SAMEFRAME;
				mFrame->mFrameData = 0;
				mFrame->mFrameDataSize = 0;
				if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				return;
			}
		}
		
		if (gExtendedBlocks && gUseSingle)
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
				if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				return;
			}
		}

		unsigned char* data = 0;
		int len = 0;
		mFrame->mFrameData = 0;
		mFrame->mFrameDataSize = 0xffffff;

		if (gClassicBlocks)
		{
			mFrame->mFrameData = new unsigned char[pixels * 2];
			mFrame->mFrameDataSize = mHeader.mWidth * mHeader.mHeight;
			memcpy(mFrame->mFrameData, mFrame->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
			mFrame->mFrameType = FLI_COPY;
			if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);

			if (gUseRLE)
			{
				data = new unsigned char[pixels * 2];
				len = encodeRLEFrame(data, mFrame->mIndexPixels, mHeader.mWidth, mHeader.mHeight);
				if (gVerify) if (len > pixels) printf("overlong rle: %d +%d\n", len, len - pixels);
				if (len < mFrame->mFrameDataSize)
				{
					delete[] mFrame->mFrameData;
					mFrame->mFrameData = data;
					mFrame->mFrameDataSize = len;
					mFrame->mFrameType = RLEFRAME;
					if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				}
				else
				{
					delete[] data;
				}
			}
		}

		if (gExtendedBlocks)
		{
			if (gUseLRLE8)
			{
				data = new unsigned char[pixels * 2];
				len = encodeLinearRLE8Frame(data, mFrame->mIndexPixels, pixels);
				if (gVerify) if (len > pixels) printf("overlong Lrle8 %d +%d\n", len, len - pixels);
				if (len < mFrame->mFrameDataSize)
				{
					delete[] mFrame->mFrameData;
					mFrame->mFrameData = data;
					mFrame->mFrameDataSize = len;
					mFrame->mFrameType = LINEARRLE8;
					if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				}
				else
				{
					delete[] data;
				}
			}

			if (gUseLRLE16)
			{
				data = new unsigned char[pixels * 2];
				len = encodeLinearRLE16Frame(data, (mFrame->mIndexPixels), pixels);
				if (gVerify) if (len > pixels) printf("overlong Lrle16 %d +%d\n", len, len - pixels);
				if (len < mFrame->mFrameDataSize)
				{
					delete[] mFrame->mFrameData;
					mFrame->mFrameData = data;
					mFrame->mFrameDataSize = len;
					mFrame->mFrameType = LINEARRLE16;
					if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
				}
				else
				{
					delete[] data;
				}
			}
		}

		if (mPrev)
		{
			if (gExtendedBlocks)
			{
				if (gUseLDelta8)
				{
					data = new unsigned char[pixels * 2];
					len = encodeLinearDelta8Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
					if (gVerify) if (len > pixels) printf("overlong Ldelta8 %d +%d\n", len, len - pixels);

					if (len < mFrame->mFrameDataSize)
					{
						delete[] mFrame->mFrameData;
						mFrame->mFrameData = data;
						mFrame->mFrameDataSize = len;
						mFrame->mFrameType = LINEARDELTA8;
						if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
					}
					else
					{
						delete[] data;
					}
				}

				if (gUseLDelta16)
				{
					data = new unsigned char[pixels * 2];
					len = encodeLinearDelta16Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
					if (gVerify) if (len > pixels) printf("overlong Ldelta16 %d +%d\n", len, len - pixels);

					if (len < mFrame->mFrameDataSize)
					{
						delete[] mFrame->mFrameData;
						mFrame->mFrameData = data;
						mFrame->mFrameDataSize = len;
						mFrame->mFrameType = LINEARDELTA16;
						if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
					}
					else
					{
						delete[] data;
					}
				}

				if (gSlowBlocks)
				{
					if (gUseLZ1)
					{
						data = new unsigned char[pixels * 2];
						len = encodeLZ1Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
						if (gVerify) if (len > pixels) printf("overlong Lz1 %d +%d\n", len, len - pixels);

						if (len < mFrame->mFrameDataSize)
						{
							delete[] mFrame->mFrameData;
							mFrame->mFrameData = data;
							mFrame->mFrameDataSize = len;
							mFrame->mFrameType = LZ1;
							if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
						}
						else
						{
							delete[] data;
						}
					}

					if (gUseLZ1b)
					{
						data = new unsigned char[pixels * 2];
						len = encodeLZ1bFrame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
						if (gVerify) if (len > pixels) printf("overlong Lz1b %d +%d\n", len, len - pixels);

						if (len < mFrame->mFrameDataSize)
						{
							delete[] mFrame->mFrameData;
							mFrame->mFrameData = data;
							mFrame->mFrameDataSize = len;
							mFrame->mFrameType = LZ1B;
							if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
						}
						else
						{
							delete[] data;
						}
					}

					if (gUseLZ2)
					{
						data = new unsigned char[pixels * 2];
						len = encodeLZ2Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
						if (gVerify) if (len > pixels) printf("overlong Lz2 %d +%d\n", len, len - pixels);

						if (len < mFrame->mFrameDataSize)
						{
							delete[] mFrame->mFrameData;
							mFrame->mFrameData = data;
							mFrame->mFrameDataSize = len;
							mFrame->mFrameType = LZ2;
							if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
						}
						else
						{
							delete[] data;
						}
					}

					if (gUseLZ2b)
					{
						data = new unsigned char[pixels * 2];
						len = encodeLZ2bFrame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
						if (gVerify) if (len > pixels) printf("overlong Lz2b %d +%d\n", len, len - pixels);

						if (len < mFrame->mFrameDataSize)
						{
							delete[] mFrame->mFrameData;
							mFrame->mFrameData = data;
							mFrame->mFrameDataSize = len;
							mFrame->mFrameType = LZ2B;
							if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
						}
						else
						{
							delete[] data;
						}
					}
				}

				if (gUseLZ3)
				{
					data = new unsigned char[pixels * 2];
					len = encodeLZ3Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
					if (gVerify) if (len > pixels) printf("overlong Lz3 %d +%d\n", len, len - pixels);

					if (len < mFrame->mFrameDataSize)
					{
						delete[] mFrame->mFrameData;
						mFrame->mFrameData = data;
						mFrame->mFrameDataSize = len;
						mFrame->mFrameType = LZ3;
						if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
					}
					else
					{
						delete[] data;
					}
				}
			}
		
			if (gClassicBlocks)
			{
				if (gUseDelta8Frame)
				{
					data = new unsigned char[pixels * 2];
					len = encodeDelta8Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth, mHeader.mHeight);
					if (gVerify) if (len > pixels) printf("overlong delta8 %d +%d\n", len, len - pixels);

					if (len < mFrame->mFrameDataSize)
					{
						delete[] mFrame->mFrameData;
						mFrame->mFrameData = data;
						mFrame->mFrameDataSize = len;
						mFrame->mFrameType = DELTA8FRAME;
						if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
					}
					else
					{
						delete[] data;
					}
				}

				if (gUseDelta16Frame)
				{
					data = new unsigned char[pixels * 2];
					len = encodeDelta16Frame(data, mFrame->mIndexPixels, mPrev->mIndexPixels, mHeader.mWidth, mHeader.mHeight);
					if (gVerify) if (len > pixels) printf("overlong delta16 %d +%d\n", len, len - pixels);

					if (len < mFrame->mFrameDataSize)
					{
						delete[] mFrame->mFrameData;
						mFrame->mFrameData = data;
						mFrame->mFrameDataSize = len;
						mFrame->mFrameType = DELTA16FRAME;
						if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight);
					}
					else
					{
						delete[] data;
					}
				}
			}
		}
	}
};

void encode(const FliHeader& header)
{
	printf("Encoding..\n");
	auto start = std::chrono::steady_clock::now();
	
	
	Thread::Pool threadpool;
	threadpool.init(gThreads);

	Frame* walker = gRoot;
	Frame* prev = 0;
	int fr = 0;
	while (walker)
	{
		EncodeTask* t = new EncodeTask(walker, prev, header);
		t->mDeleteTask = 1;
#ifdef NDEBUG
		threadpool.addWork(t);
#else
		//threadpool.addWork(t);
		t->work();
#endif
		prev = walker;
		walker = walker->mNext;
		fr++;
		printf("%d%% tasks started \r", fr * 100 / header.mFrames);
	}

	printf("Waiting for threads..\n");
	threadpool.waitUntilDone(100, true);
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("Time elapsed: %3.3fs\n\n", elapsed_seconds.count());

}

void writechunk(FILE* outfile, Frame* frame, int tag, int frameno)
{
#pragma pack(push, 1)

	struct framehdr
	{
		unsigned int size;
		unsigned short tag;
		unsigned short chunks;
		unsigned short reserved[4];
	} hdr;	

	struct chunkhdr
	{
		unsigned int size;
		unsigned short tag;
	} chdr;
#pragma pack(pop)

	memset(&hdr, 0, sizeof(hdr));
	hdr.size = sizeof(hdr) + sizeof(chdr) + frame->mFrameDataSize;
	if (frameno == 0)
		hdr.size += sizeof(chdr) + 2 + 1 + 1 + 3 * 256;
	hdr.tag = 0xF1FA;
	hdr.chunks = 1 + (frameno == 0 ? 1 : 0);
	if (frame->mFrameType == SAMEFRAME)
	{
		hdr.chunks = 0;
		hdr.size = sizeof(hdr);
	}
	fwrite(&hdr, 1, sizeof(hdr), outfile);

	if (frameno == 0)
	{
		// output palette chunk
		chdr.size = sizeof(chdr) + 2 + 1 + 1 + 3 * 256;
		chdr.tag = 4; // 256 level palette
		fwrite(&chdr, 1, sizeof(chdr), outfile);
		fputc(1, outfile); // one packet
		fputc(0, outfile); // ^16bit value
		fputc(0, outfile); // skip 0
		fputc(0, outfile); // copy 256
		for (int i = 0; i < 256; i++)
		{
			fwrite(&frame->mPalette[i], 1, 3, outfile); // TODO: verify this is r,g,b
		}
	}
	chdr.size = sizeof(chdr) + frame->mFrameDataSize;
	chdr.tag = tag;
	fwrite(&chdr, 1, sizeof(chdr), outfile);
	if (frame->mFrameDataSize)
		fwrite(frame->mFrameData, 1, frame->mFrameDataSize, outfile);
}

void output_flc(FliHeader& header, FILE* outfile)
{
	printf("Writing file..\n");
	auto start = std::chrono::steady_clock::now();
	fwrite(&header, 1, sizeof(FliHeader), outfile); // unfinished header
	Frame* walker = gRoot;
	int total = 0;
	int frames = 0;
	while (walker)
	{
		if (frames == 0)
			header.mOframe1 = ftell(outfile); // start of first frame
		if (frames == 1)
			header.mOframe2 = ftell(outfile); // start of second frame

		int chunktype = 0;
		switch (walker->mFrameType)
		{
		case SAMEFRAME:chunktype = 0; break;
		case BLACKFRAME:chunktype = 13; break;
		case RLEFRAME:chunktype = 15; break;
		case DELTA8FRAME: chunktype = 12; break;
		case DELTA16FRAME: chunktype = 7; break;
		case FLI_COPY: chunktype = 16; break;
			// extended blocks:
		case ONECOLOR: chunktype = 101; break;
		case LINEARRLE8: chunktype = 102; break;
		case LINEARRLE16: chunktype = 103; break;
		case LINEARDELTA8: chunktype = 104; break;
		case LINEARDELTA16: chunktype = 105; break;
		case LZ1: chunktype = 106; break;
		case LZ2: chunktype = 107; break;
		case LZ3: chunktype = 108; break;
		case LZ1B: chunktype = 109; break;
		case LZ2B: chunktype = 101; break;
		default: printf("?!?\n");
		}
		writechunk(outfile, walker, chunktype, frames);
		total += walker->mFrameDataSize;
		walker = walker->mNext;
		frames++;
	}
	printf("\nTotal %d bytes of payload (%d kB, %d MB), %d bytes overhead\n", total, total/1024, total/(1024*1024), ftell(outfile)-total);
	printf("Compression ratio %3.3f%%, %d bytes per frame on average\n", 100.0 * total / (double)(frames * header.mWidth * header.mHeight), total / frames);

	header.mSize = ftell(outfile);
	header.mFrames = frames;

	fseek(outfile, 0, SEEK_SET);
	fwrite(&header, 1, sizeof(FliHeader), outfile); // finished header

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("\nTime elapsed: %3.3fs\n\n", elapsed_seconds.count());
}


void output_flx(FliHeader& header, FILE* outfile)
{
	printf("Writing file..\n");
	auto start = std::chrono::steady_clock::now();

#pragma pack(push, 1)
	struct flxheader
	{
		unsigned int tag;
		unsigned short frames;
		unsigned short speed;
		unsigned char pal[2 * 256];
	} hdr;
#pragma pack(pop)
	memset(&hdr, 0, sizeof(hdr));
	hdr.tag = 'LFXN'; // reverses to 'NXFL'
	hdr.tag = header.mFrames;
	hdr.speed = header.mSpeed;
	for (int i = 0; i < 256; i++)
	{
		int c;
		c = ((gRoot->mPalette[i] >> 16) & 0xe0) << 1;
		c |= ((gRoot->mPalette[i] >> 8) & 0xe0) >> 2;
		c |= ((gRoot->mPalette[0] >> 0) & 0xe0) >> 5;

		hdr.pal[i * 2 + 0] = c >> 1;
		hdr.pal[i * 2 + 1] = c & 1;
	}

	fwrite(&hdr, 1, sizeof(hdr), outfile);
	Frame* walker = gRoot;
	int framecounts[24] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	int framemaxsize[24] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	int frameminsize[24] = { 
		0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,
		0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,
		0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff };
	int total = 0;
	int frames = 0;
	while (walker)
	{
		if (frames == 0)
			header.mOframe1 = ftell(outfile); // start of first frame
		if (frames == 1)
			header.mOframe2 = ftell(outfile); // start of second frame

		int chunktype = 0;
		switch (walker->mFrameType)
		{
		case SAMEFRAME:chunktype = 0;  printf("s"); break;
		case BLACKFRAME:chunktype = 13;  printf("b"); break;
		case RLEFRAME:chunktype = 15; printf("r"); break;
		case DELTA8FRAME: chunktype = 12; printf("d"); break;
		case DELTA16FRAME: chunktype = 7;  printf("D"); break;
		case FLI_COPY: chunktype = 16; printf("c"); break;
			// extended blocks:
		case ONECOLOR: chunktype = 101;  printf("o"); break;
		case LINEARRLE8: chunktype = 102; printf("l"); break;
		case LINEARRLE16: chunktype = 103; printf("L"); break;
		case LINEARDELTA8: chunktype = 104; printf("e"); break;
		case LINEARDELTA16: chunktype = 105; printf("E"); break;
		case LZ1: chunktype = 106; printf("1"); break;
		case LZ2: chunktype = 107; printf("2"); break;
		case LZ3: chunktype = 108; printf("3"); break;
		case LZ1B: chunktype = 109; printf("4"); break;
		case LZ2B: chunktype = 110; printf("5"); break;
		default: printf("?!?\n");
		}		
		fputc(walker->mFrameType, outfile);
		unsigned short ds = walker->mFrameDataSize;
		fwrite(&ds, 1, 2, outfile);
		if (ds)
		{
			fwrite(walker->mFrameData, 1, walker->mFrameDataSize, outfile);
		}
		
		if (walker->mFrameDataSize > framemaxsize[walker->mFrameType]) framemaxsize[walker->mFrameType] = walker->mFrameDataSize;
		if (walker->mFrameDataSize < frameminsize[walker->mFrameType]) frameminsize[walker->mFrameType] = walker->mFrameDataSize;
		framecounts[walker->mFrameType]++;
		total += walker->mFrameDataSize;
		walker = walker->mNext;
		frames++;
	}
	printf("\nTotal %d bytes of payload (%d kB, %d MB), %d bytes overhead\n", total, total / 1024, total / (1024 * 1024), ftell(outfile) - total);
	printf("Compression ratio %3.3f%%, %d bytes per frame on average\n", 100.0 * total / (double)(frames * header.mWidth * header.mHeight), total / frames);
	printf("\nBlock types:\n");
	if (framecounts[1]) printf("s sameframe     %5d (%5d -%5d bytes)\n", framecounts[1], frameminsize[1], framemaxsize[1]);
	if (framecounts[2]) printf("b blackframe    %5d (%5d -%5d bytes)\n", framecounts[2], frameminsize[2], framemaxsize[2]);
	if (framecounts[3]) printf("r rleframe      %5d (%5d -%5d bytes)\n", framecounts[3], frameminsize[3], framemaxsize[3]);
	if (framecounts[4]) printf("d delta8frame   %5d (%5d -%5d bytes)\n", framecounts[4], frameminsize[4], framemaxsize[4]);
	if (framecounts[5]) printf("D delta16frame  %5d (%5d -%5d bytes)\n", framecounts[5], frameminsize[5], framemaxsize[5]);
	if (framecounts[6]) printf("c copy          %5d (%5d -%5d bytes)\n", framecounts[6], frameminsize[6], framemaxsize[6]);
	printf("-- extended blocks --\n");
	if (framecounts[7]) printf("o onecolor      %5d (%5d -%5d bytes)\n", framecounts[7], frameminsize[7], framemaxsize[7]);
	if (framecounts[8]) printf("l linearrle8    %5d (%5d -%5d bytes)\n", framecounts[8], frameminsize[8], framemaxsize[8]);
	if (framecounts[9]) printf("L linearrle16   %5d (%5d -%5d bytes)\n", framecounts[9], frameminsize[9], framemaxsize[9]);
	if (framecounts[10]) printf("e lineardelta8  %5d (%5d -%5d bytes)\n", framecounts[10], frameminsize[10], framemaxsize[10]);
	if (framecounts[11]) printf("E lineardelta16 %5d (%5d -%5d bytes)\n", framecounts[11], frameminsize[11], framemaxsize[11]);
	if (framecounts[12]) printf("1 lz scheme 1   %5d (%5d -%5d bytes)\n", framecounts[12], frameminsize[12], framemaxsize[12]);
	if (framecounts[13]) printf("4 lz scheme 1b  %5d (%5d -%5d bytes)\n", framecounts[13], frameminsize[13], framemaxsize[13]);
	if (framecounts[14]) printf("2 lz scheme 2   %5d (%5d -%5d bytes)\n", framecounts[14], frameminsize[14], framemaxsize[14]);
	if (framecounts[15]) printf("5 lz scheme 2b  %5d (%5d -%5d bytes)\n", framecounts[15], frameminsize[15], framemaxsize[15]);
	if (framecounts[16]) printf("3 lz scheme 3   %5d (%5d -%5d bytes)\n", framecounts[16], frameminsize[16], framemaxsize[16]);

	header.mSize = ftell(outfile);
	header.mFrames = frames;

	fseek(outfile, 0, SEEK_SET);
	fwrite(&header, 1, sizeof(FliHeader), outfile); // finished header

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("\nTime elapsed: %3.3fs\n\n", elapsed_seconds.count());
}

enum optionIndex { UNKNOWN, HELP, FLC, STD, EXT, NOSLOW, HALFRES, DITHER, FASTSCALE, VERIFY, THREADS, FRAMEDELAY };
const option::Descriptor usage[] =
{
	{ UNKNOWN,		0, "", "",	option::Arg::None,				 "USAGE: nextfli outputfilename outputfilemask [options]\n\nOptions:"},
	{ HELP,			0, "h", "help", option::Arg::None,			 " -h --help\t Print usage and exit"},
	{ FLC,			0, "f", "flc", option::Arg::None,			 " -f --flc\t Output standard FLC format (default: use flx)"},
	{ STD,			0, "s", "std", option::Arg::None,			 " -s --std\t Use standard blocks (default: flc yes, flx no)"},
	{ EXT,			0, "e", "ext", option::Arg::None,			 " -e --ext\t Use extended blocks (default: flc no, fli yes)"},
	{ NOSLOW,		0, "n", "noslow", option::Arg::None,		 " -n --noslow\t Don't use slow-to-encode blocks (default: yes)"},
	{ HALFRES,      0, "l", "halfres", option::Arg::None,        " -l --halfres\t Reduce output resolution to half x, half y (default: don't)"},
	{ DITHER,       0, "d", "dither", option::Arg::None,         " -d --dither\t Use ordered dither (default: don't)"},
	{ FASTSCALE,    0, "p", "fastscale", option::Arg::None,      " -p --fastscale\t Use fast image scaling (default: don't)"},
	{ VERIFY,       0, "v", "verify", option::Arg::None,         " -v --verify\tVerify encoding (default: don't)"},
	{ THREADS,      0, "t", "threads", option::Arg::Optional,    " -t --threads\tNumber of threads to use (default: num of logical cores)"},
	{ FRAMEDELAY,   0, "r", "framedelay", option::Arg::Optional, " -r --framedelay\tFrame delay 1=50hz, 2=25hz, 3=16.7hz 4=12.5hz (default: 4)"},
	{ UNKNOWN,      0, "", "", option::Arg::None,				 "Example:\n  nextfli test.flc frames*.png -f -d"},
	{ 0,0,0,0,0,0 }
};


int main(int parc, char* pars[])
{
	option::Stats stats(usage, parc - 1, pars + 1);
	assert(stats.buffer_max < 16 && stats.options_max < 16);
	option::Option options[16], buffer[16];
	option::Parser parse(true, usage, parc - 1, pars + 1, options, buffer);

	if (options[UNKNOWN])
	{
		for (option::Option* opt = options[UNKNOWN]; opt; opt = opt->next())
			printf("Unknown option: %s\n", opt->name);
		printf("Run without parameters for help.\n");
		return 0;
	}

	if (parse.error() || parc < 3 || options[HELP] || parse.nonOptionsCount() != 2)
	{
		option::printUsage(std::cout, usage);
		return 0;
	}

	if (options[FLC])
	{
		gClassicBlocks = 1;
		gExtendedBlocks = 0;
	}
	else
	{
		gClassicBlocks = 0;
		gExtendedBlocks = 1;
		gSlowBlocks = 1;
	}

	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	gThreads = sysinfo.dwNumberOfProcessors;

	if (options[STD]) gClassicBlocks = 1;
	if (options[EXT]) gExtendedBlocks = 1;
	if (options[NOSLOW]) gSlowBlocks = 0;
	if (options[HALFRES]) gHalfRes = 1;
	if (options[DITHER]) gDither = 1;
	if (options[FASTSCALE]) gPointSample = 1;
	if (options[VERIFY]) gVerify = 1;
	if (options[THREADS])
	{
		if (options[THREADS].arg != 0 && options[THREADS].arg[0])
			gThreads = atoi(options[THREADS].arg);
	}
	if (options[FRAMEDELAY])
	{
		if (options[FRAMEDELAY].arg != 0 && options[FRAMEDELAY].arg[0])
			gFramedelay = atoi(options[FRAMEDELAY].arg);
	}

	if (gFramedelay <= 0)
		gFramedelay = 1;

	if (gThreads < 0)
		gThreads = 0;

	FILE* outfile;
	FliHeader header;
	auto start = std::chrono::steady_clock::now();

	header.mWidth = 256;
	header.mHeight = 192;
	header.mFrames = 0;

	printf("Running with %d threads\n", gThreads);
	printf("Encoding with %d frame delay (%3.3fhz)\n", gFramedelay, 50.0f / gFramedelay);
	header.mSpeed = (1000 / 50) * gFramedelay;

	loadframes(header, parse.nonOption(1));
	if (header.mFrames == 0)
	{
		printf("No frames loaded.\n");
		return 0;
	}
	quantize(header);
	encode(header);
	printf("Opening %s for writing..\n", parse.nonOption(0));
	outfile = fopen(parse.nonOption(0), "wb");
	if (!outfile)
	{
		printf("file open failed\n");
		return 0;
	}
	if (options[FLC])
	{
		output_flc(header, outfile);
	}
	else
	{
		output_flx(header, outfile);
	}
	fclose(outfile);

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("\nTotal time elapsed: %3.3fs\n", elapsed_seconds.count());

	return 0;
}
