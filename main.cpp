// TODO
// - different graphics modes support
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

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h> // findfirst
#else
#include <glob.h>
#define _strdup strdup
#endif

#include "gif.h"

Frame* gRoot = NULL, * gLast = NULL;
int gHalfRes = 0;
int gPointSample = 0;
int gDither = 0;
int gExtendedBlocks = 0;
int gClassicBlocks = 1;
int gSlowBlocks = 1;
int gAggressive = 1;
int gVerify = 0;
int gFramedelay = 4;
int gThreads = 0;
int gGlobalMinSpan = 0;
int gSpanGoal = 100000;
int gMaxFrameSize = 8192;
int gLossy = 0;
int gLossyKeyframes = 0;
int gPaletteWidth = 256;
int gChunkyX = 1;
int gChunkyY = 1;
int gCreateLoopFrame = 0;
int gGraphicsMode = 0;

// trivial blocks
int gUseBlack = 1;
int gUseSame = 1;
int gUseSingle = 1;

// classic blocks
int gUseRLE = 1;
int gUseDelta8Frame = 1;
int gUseDelta16Frame = 1;

// retired
int gUseLRLE8 = 0;
int gUseLRLE16 = 0;
int gUseLDelta8 = 0;
int gUseLDelta16 = 0;
int gUseLZ1 = 0;
int gUseLZ2 = 0;
int gUseLZ2b = 0;
int gUseLZ3 = 0;
int gUseLZ3B = 0;
int gUseLZ3D = 0;
int gUseLZ3E = 0;

// flx blocks
int gUseLZ1b = 1;
int gUseLZ4 = 1; 
int gUseLZ5 = 1;
int gUseLZ6 = 1; 
int gUseLZ3C = 1;

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
		int height = header.mHeight;
		if (gGraphicsMode == 1 || gGraphicsMode == 2)
		{
			// 320x and 640x modes have x and y swapped
			unsigned int* temp = new unsigned int[x * y];
			memcpy(temp, data, sizeof(unsigned int) * x * y);
			for (int i = 0; i < y; i++)
				for (int j = 0; j < x; j++)
				{
					data[j * y + i] = temp[i * x + j];
				}
			delete[] temp;
			int t = x;
			x = y;
			y = t;
		}

		int xofs = 0;
		int yofs = 0;
		int ht = 0;
		int wd = 0;

		if (gGraphicsMode == 2)
			height /= 2;
		if ((float)y / x <= (float)height / header.mWidth)
		{
			wd = header.mWidth;
			ht = wd * y / x;
		}
		else
		{
			wd = height * x / y;
			ht = wd * y / x;
		}

		if (gGraphicsMode == 2)
		{
			ht *= 2; // 640x mode has weird aspect ratio
			height *= 2;
		}

		if (gHalfRes)
		{
			wd /= 2;
			ht /= 2;
		}

		xofs = (header.mWidth - wd) / 2;
		yofs = (height - ht) / 2;


		memset(fr->mRgbPixels, 0, sizeof(unsigned int) * height * header.mWidth);

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
			stbir_resize_uint8((const unsigned char*)data, x, y, x * 4,
				(unsigned char*)fr->mRgbPixels + header.mWidth * 4 * yofs + xofs * 4, wd, ht, header.mWidth * 4,
				4);
		}
		stbi_image_free((void*)data);
	}
};

void addLoopFrame(FliHeader &header)
{
	printf("Adding loop frame..\n");
	Frame* fr = new Frame;
	fr->mIndexPixels = new unsigned char[header.mHeight * header.mWidth];
	memcpy(fr->mIndexPixels, gRoot->mIndexPixels, header.mHeight * header.mWidth);
	fr->mPaletteChanged = gRoot->mPaletteChanged;
	memcpy(fr->mPalette, gRoot->mPalette, sizeof(int) * 256);
	fr->mSubframes = gRoot->mSubframes;
	gLast->mNext = fr;
	gLast = fr;
	header.mFrames++;	
}

void addFrame(char* fn, const FliHeader& header, Thread::Pool& threadpool)
{
	Frame* fr = new Frame;
	fr->mRgbPixels = new unsigned int[header.mHeight * header.mWidth];
	memset(fr->mRgbPixels, 0, sizeof(int) * header.mHeight * header.mWidth);
	if (gGraphicsMode == 1 || gGraphicsMode == 2)
		fr->mSubframes = 2;

	if (gRoot == NULL)
	{
		gRoot = fr;
		gLast = fr;
	}
	else
	{
		gLast->mNext = fr;
		gLast = fr;
	}

	AddFrameTask* t = new AddFrameTask(fn, fr, header);
	t->mDeleteTask = 1;
	threadpool.addWork(t);
}

void loadframes(FliHeader& header, const char* filemask)
{
	printf("Loading frames using filemask \"%s\"\n", filemask);

	auto start = std::chrono::steady_clock::now();
	Thread::Pool threadpool;
	threadpool.init(gThreads);

#if defined(_WIN32) || defined(_WIN64)
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
				*(char*)(strrchr(filemask, '\\') + 1) = 0;
				sprintf(temp, "%s%s", filemask, FindFileData.cFileName);
			}
			else
				if (strrchr(filemask, '/') != 0)
				{
					*(char*)(strrchr(filemask, '/') + 1) = 0;
					sprintf(temp, "%s%s", filemask, FindFileData.cFileName);
				}
				else
					sprintf(temp, "%s", FindFileData.cFileName);
			printf("Loading %s                    \r", temp);
			addFrame(temp, header, threadpool);
			header.mFrames++;
		} while (FindNextFileA(hFind, &FindFileData));

		FindClose(hFind);
	}
#else
	glob_t glob_result;
	glob(filemask, GLOB_TILDE, NULL, &glob_result);
	for (unsigned int i = 0; i < glob_result.gl_pathc; i++)
	{
		printf("Loading %s                    \r", glob_result.gl_pathv[i]);
		addFrame(glob_result.gl_pathv[i], header, threadpool);
		header.mFrames++;
	}
#endif

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
	QuantizeTask(Frame* aFrame, const FliHeader& aHeader, unsigned int* aColors) : mFrame(aFrame), mHeader(aHeader), colors(aColors)
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

void applyLossy(Frame *prev, Frame *frame, const int pixels)
{
	for (int i = 0; i < pixels; i++)
	{
		int pc = prev->mRgbPixels[i];
		int cc = frame->mRgbPixels[i];
		int delta = 0;
		delta += abs(((pc >> 0) & 7) - ((cc >> 0) & 7));
		delta += abs(((pc >> 3) & 7) - ((cc >> 3) & 7));
		delta += abs(((pc >> 6) & 7) - ((cc >> 6) & 7));
		if (delta <= gLossy)
			frame->mRgbPixels[i] = pc;
	}
}

void applyChunky(Frame* frame, const int width, const int height)
{
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; j++)
		{
			int cx = ((j / gChunkyX) * gChunkyX);
			int cy = ((i / gChunkyY) * gChunkyY);
			frame->mRgbPixels[i * width + j] =
				frame->mRgbPixels[cy * width + cx];
		}
}

void quantize(FliHeader& header)
{
	auto start = std::chrono::steady_clock::now();
	unsigned int* framecolors;
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

	if (gChunkyX > 1 || gChunkyY > 1)
	{
		printf("Applying chunky filter..\n");
		Frame* walker = gRoot;
		while (walker)
		{
			applyChunky(walker, header.mWidth, header.mHeight);
			walker = walker->mNext;
		}
	}

	if (gLossy > 0)
	{
		printf("Applying lossy filter..\n");
		walker = gRoot->mNext;
		Frame* prev = gRoot;
		int i = 0;
		while (walker)
		{
			if (gLossyKeyframes == 0 || (i % gLossyKeyframes) != 0)
				applyLossy(prev, walker, header.mWidth * header.mHeight);
			prev = walker;
			walker = walker->mNext;
			i++;
		}
	}

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
	printf("%d unique colors\n", cc);

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
	int ofs = sq_reduce(q, &idxmap, &pal, &idxs, gPaletteWidth);

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

		if (gGraphicsMode == 2)
		{
			// Combine pixels in 16 color mode
			for (int i = 0; i < header.mHeight / 2; i++)
				for (int j = 0; j < header.mWidth; j++)
					walker->mIndexPixels[i * header.mWidth + j] =
					((walker->mIndexPixels[(i * 2 + 0) * header.mWidth + j] & 0xf) << 4) |
					((walker->mIndexPixels[(i * 2 + 1) * header.mWidth + j] & 0xf));
		}

		delete walker->mRgbPixels;
		walker->mRgbPixels = 0;
		walker = walker->mNext;
	
	 }

	if (gGraphicsMode == 2)
	{
		// From here on out, graphics modes 1 and 2 are identical
		header.mHeight /= 2;
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

	void calcChecksums()
	{
		int pixels = mHeader.mWidth * mHeader.mHeight / mFrame->mSubframes;
		for (int subframe = 0; subframe < mFrame->mSubframes; subframe++)
		{
			mFrame->mChecksum1[subframe] = 0;
			mFrame->mChecksum2[subframe] = 0;
			for (int i = 0; i < pixels; i++)
			{
				mFrame->mChecksum1[subframe] ^= mFrame->mIndexPixels[i + subframe * pixels];
				mFrame->mChecksum2[subframe] += mFrame->mChecksum1[subframe];
			}
		}
	}

	virtual void work()
	{
		int pixels = mHeader.mWidth * mHeader.mHeight / mFrame->mSubframes;

		calcChecksums();

		int minspan = gGlobalMinSpan;
		int totalspans = 0;
		int totalsize = 0;

		do
		{
			for (int subframe = 0; subframe < mFrame->mSubframes; subframe++)
			{
				unsigned char* data = 0;
				int len = 0;
				mFrame->mFrameData[subframe] = 0;
				mFrame->mFrameDataSize[subframe] = 0xffffff;

				int allzero = 1;
				for (int i = 0; allzero && i < pixels; i++)
					allzero = (mFrame->mIndexPixels[i + subframe * pixels] == 0);

				if (allzero && gUseBlack)
				{
					mFrame->mFrameType[subframe] = BLACKFRAME;
					mFrame->mFrameData[subframe] = 0;
					mFrame->mFrameDataSize[subframe] = 0;
					mFrame->mSpans[subframe] = 0;
					if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
				}

				if (mPrev && gUseSame && mFrame->mFrameDataSize[subframe] > 0)
				{
					// Check if previous frame is identical to current
					int identical = 1;
					for (int i = 0; identical && i < pixels; i++)
						identical = (mFrame->mIndexPixels[i + subframe * pixels] == mPrev->mIndexPixels[i + subframe * pixels]);
					if (identical)
					{
						mFrame->mFrameType[subframe] = SAMEFRAME;
						mFrame->mFrameData[subframe] = 0;
						mFrame->mFrameDataSize[subframe] = 0;
						mFrame->mSpans[subframe] = 0;
						if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
					}
				}

				if (gExtendedBlocks && gUseSingle && mFrame->mFrameDataSize[subframe] > 0)
				{
					int allsingle = 1;
					for (int i = 0; allsingle && i < pixels; i++)
						allsingle = (mFrame->mIndexPixels[i] == mFrame->mIndexPixels[0]);
					if (allsingle)
					{
						mFrame->mFrameType[subframe] = ONECOLOR;
						mFrame->mFrameData[subframe] = new unsigned char[1];
						mFrame->mFrameDataSize[subframe] = 1;
						mFrame->mFrameData[subframe][0] = mFrame->mIndexPixels[0];
						mFrame->mSpans[subframe] = 0;
						if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
					}
				}


				if (gClassicBlocks && mFrame->mFrameDataSize[subframe] > 1)
				{
					mFrame->mFrameData[subframe] = new unsigned char[pixels * 2];
					mFrame->mFrameDataSize[subframe] = mHeader.mWidth * mHeader.mHeight;
					memcpy(mFrame->mFrameData, mFrame->mIndexPixels, mHeader.mWidth * mHeader.mHeight);
					mFrame->mFrameType[subframe] = FLI_COPY;
					if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);

					if (gUseRLE)
					{
						data = new unsigned char[pixels * 2];
						len = encodeRLEFrame(data, mFrame->mIndexPixels, mHeader.mWidth, mHeader.mHeight);
						mFrame->mSpans[subframe] = 0;
						if (gVerify) if (len > pixels) printf("overlong rle: %d +%d\n", len, len - pixels);
						if (len < mFrame->mFrameDataSize[subframe])
						{
							delete[] mFrame->mFrameData[subframe];
							mFrame->mFrameData[subframe] = data;
							mFrame->mFrameDataSize[subframe] = len;
							mFrame->mFrameType[subframe] = RLEFRAME;
							if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
						}
						else
						{
							delete[] data;
						}
					}
				}

				if (gExtendedBlocks && mFrame->mFrameDataSize[subframe] > 1)
				{
					if (gSlowBlocks)
					{
						if (gUseLZ4 && (gAggressive || !mPrev)) // LZ block that doesn't need previous frames
						{
							data = new unsigned char[pixels * 2];
							int spans = 0;
							len = encodeLZ4Frame(data, mFrame->mIndexPixels + subframe * pixels, mFrame->mIndexPixels + subframe * 16384, mFrame->mSubframes > 1 ? 65536 : pixels, pixels, minspan, spans);
							if (gVerify) if (len > pixels) printf("overlong Lz4 %d +%d\n", len, len - pixels);

							if (len < mFrame->mFrameDataSize[subframe])
							{
								delete[] mFrame->mFrameData[subframe];
								mFrame->mFrameData[subframe] = data;
								mFrame->mFrameDataSize[subframe] = len;
								mFrame->mFrameType[subframe] = LZ4;
								mFrame->mSpans[subframe] = spans;
								if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
							}
							else
							{
								delete[] data;
							}
						}
					}
				}

				if (mPrev && mFrame->mFrameDataSize[subframe] > 1)
				{
					if (gExtendedBlocks)
					{
						if (gSlowBlocks)
						{
							if (gUseLZ1b && gAggressive)
							{
								data = new unsigned char[pixels * 2];
								int spans = 0;
								len = encodeLZ1bFrame(data, mFrame->mIndexPixels + pixels * subframe, mPrev->mIndexPixels + 16384 * subframe, mFrame->mSubframes > 1 ? 65536 : pixels, pixels, minspan, spans);
								if (gVerify) if (len > pixels) printf("overlong Lz1b %d +%d\n", len, len - pixels);

								if (len < mFrame->mFrameDataSize[subframe])
								{
									delete[] mFrame->mFrameData[subframe];
									mFrame->mFrameData[subframe] = data;
									mFrame->mFrameDataSize[subframe] = len;
									mFrame->mFrameType[subframe] = LZ1B;
									mFrame->mSpans[subframe] = spans;
									if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
								}
								else
								{
									delete[] data;
								}
							}

							if (gUseLZ5)
							{
								data = new unsigned char[pixels * 2];
								int spans = 0;
								len = encodeLZ5Frame(data, mFrame->mIndexPixels + pixels * subframe, mPrev->mIndexPixels + 16384 * subframe, mFrame->mSubframes > 1 ? 65536 : pixels, pixels, minspan, spans);
								if (gVerify) if (len > pixels) printf("overlong Lz5 %d +%d\n", len, len - pixels);

								if (len < mFrame->mFrameDataSize[subframe])
								{
									delete[] mFrame->mFrameData[subframe];
									mFrame->mFrameData[subframe] = data;
									mFrame->mFrameDataSize[subframe] = len;
									mFrame->mFrameType[subframe] = LZ5;
									mFrame->mSpans[subframe] = spans;
									if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
								}
								else
								{
									delete[] data;
								}
							}

							if (gUseLZ6 && gAggressive)
							{
								data = new unsigned char[pixels * 2];
								int spans = 0;
								len = encodeLZ6Frame(
									data, 
									mFrame->mIndexPixels + pixels * subframe, 
									mFrame->mIndexPixels + 16384 * subframe, 
									mPrev->mIndexPixels + 16384 * subframe, 
									mFrame->mSubframes > 1 ? 65536 : pixels, 
									pixels, minspan, spans);
								if (gVerify) if (len > pixels) printf("overlong Lz6 %d +%d\n", len, len - pixels);

								if (len < mFrame->mFrameDataSize[subframe])
								{
									delete[] mFrame->mFrameData[subframe];
									mFrame->mFrameData[subframe] = data;
									mFrame->mFrameDataSize[subframe] = len;
									mFrame->mFrameType[subframe] = LZ6;
									mFrame->mSpans[subframe] = spans;
									if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
								}
								else
								{
									delete[] data;
								}
							}

							if (gUseLZ3C && gAggressive)
							{
								data = new unsigned char[pixels * 4];
								int spans = 0;
								len = encodeLZ3CFrame(data, mFrame->mIndexPixels + pixels * subframe, mPrev->mIndexPixels + 16384 * subframe, mFrame->mSubframes > 1 ? 65536 : pixels, pixels, minspan, spans);
								if (gVerify) if (len > pixels) printf("overlong Lz3c %d +%d\n", len, len - pixels);

								if (len < mFrame->mFrameDataSize[subframe])
								{
									delete[] mFrame->mFrameData[subframe];
									mFrame->mFrameData[subframe] = data;
									mFrame->mFrameDataSize[subframe] = len;
									mFrame->mFrameType[subframe] = LZ3C;
									mFrame->mSpans[subframe] = spans;
									if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
								}
								else
								{
									delete[] data;
								}
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

							if (len < mFrame->mFrameDataSize[subframe])
							{
								delete[] mFrame->mFrameData[subframe];
								mFrame->mFrameData[subframe] = data;
								mFrame->mFrameDataSize[subframe] = len;
								mFrame->mFrameType[subframe] = DELTA8FRAME;
								mFrame->mSpans[subframe] = 0;
								if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
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

							if (len < mFrame->mFrameDataSize[subframe])
							{
								delete[] mFrame->mFrameData[subframe];
								mFrame->mFrameData[subframe] = data;
								mFrame->mFrameDataSize[subframe] = len;
								mFrame->mFrameType[subframe] = DELTA16FRAME;
								mFrame->mSpans[subframe] = 0;
								if (gVerify) verify_frame(mFrame, mPrev, mHeader.mWidth, mHeader.mHeight, subframe);
							}
							else
							{
								delete[] data;
							}
						}
					}
				}
			}
			minspan++;
			totalspans = 0;
			totalsize = 0;
			for (int i = 0; i < mFrame->mSubframes; i++)
			{
				totalspans += mFrame->mFrameDataSize[i];
				totalsize += mFrame->mSpans[i];
			}
		}
		while (totalspans > gSpanGoal && totalsize < gMaxFrameSize);
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
#if (!defined(_WIN32) && !defined(_WIN64)) || defined(NDEBUG)
		threadpool.addWork(t);
#else
		//threadpool.addWork(t);
		t->work();
#endif
		prev = walker;
		walker = walker->mNext;
		fr++;
		printf("\r%d%% tasks started ", fr * 100 / header.mFrames);
	}
	printf("\r");

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
	hdr.size = sizeof(hdr) + sizeof(chdr) + frame->mFrameDataSize[0];
	if (frameno == 0)
		hdr.size += sizeof(chdr) + 2 + 1 + 1 + 3 * 256;
	hdr.tag = 0xF1FA;
	hdr.chunks = 1 + (frameno == 0 ? 1 : 0);
	if (frame->mFrameType[0] == SAMEFRAME)
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
			fwrite(&frame->mPalette[i], 1, 3, outfile);
		}
	}
	chdr.size = sizeof(chdr) + frame->mFrameDataSize[0];
	chdr.tag = tag;
	fwrite(&chdr, 1, sizeof(chdr), outfile);
	if (frame->mFrameDataSize[0])
		fwrite(frame->mFrameData[0], 1, frame->mFrameDataSize[0], outfile);
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
		switch (walker->mFrameType[0])
		{
		case SAMEFRAME:    chunktype = 0;  printf("s"); break;
		case BLACKFRAME:   chunktype = 13; printf("b"); break;
		case RLEFRAME:     chunktype = 15; printf("r"); break;
		case DELTA8FRAME:  chunktype = 12; printf("d"); break;
		case DELTA16FRAME: chunktype = 7;  printf("D"); break;
		case FLI_COPY:     chunktype = 16; printf("c"); break;
		default: printf("?!?\n");
		}
		writechunk(outfile, walker, chunktype, frames);
		total += walker->mFrameDataSize[0];
		walker = walker->mNext;
		frames++;
	}
	printf("\n\nTotal %d bytes of payload (%d kB, %d MB), %d bytes overhead\n", total, total / 1024, total / (1024 * 1024), ftell(outfile) - total);
	printf("Compression ratio %3.3f%%, %d bytes per frame on average\n", 100.0 * total / (double)(frames * header.mWidth * header.mHeight), total / frames);

	header.mSize = ftell(outfile);
	header.mFrames = frames;

	fseek(outfile, 0, SEEK_SET);
	fwrite(&header, 1, sizeof(FliHeader), outfile); // finished header

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("\nTime elapsed: %3.3fs\n\n", elapsed_seconds.count());
}

void output_gif(FliHeader& header, const char* fn)
{
	printf("Writing file..\n");
	auto start = std::chrono::steady_clock::now();
	GifWriter gw;
	GifBegin(&gw, fn, header.mWidth, header.mHeight, 2 * gFramedelay);
	Frame* walker = gRoot;
	while (walker)
	{
		int frame[256 * 640];
		for (int i = 0; i < header.mWidth * header.mHeight; i++)
			frame[i] = walker->mPalette[walker->mIndexPixels[i]];
		GifWriteFrame(&gw, (const unsigned char*)frame, header.mWidth, header.mHeight, 2 * gFramedelay);
		walker = walker->mNext;
	}
	GifEnd(&gw);
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
		unsigned short config;
		unsigned short drawoffset;
		unsigned short loopoffset;

		unsigned char pal[2 * 256];
	} hdr;
#pragma pack(pop)
	memset(&hdr, 0, sizeof(hdr));
	hdr.tag = 0x21584c46; // 'FLX!'
	hdr.frames = header.mFrames;
	hdr.speed = gFramedelay;
	hdr.config = gGraphicsMode; // graphics mode
	hdr.config |= gCreateLoopFrame ? 0x8000 : 0;
	hdr.drawoffset = 0; // offset to start drawing from
	hdr.loopoffset = 0; // offset to 2nd frame (or 1st if there's no loop frame)
	for (int i = 0; i < 256; i++)
	{
		int c;
		c = ((gRoot->mPalette[i] >> 0) & 0xe0) << 1;
		c |= ((gRoot->mPalette[i] >> 8) & 0xe0) >> 2;
		c |= ((gRoot->mPalette[i] >> 16) & 0xe0) >> 5;

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
	int totalspans = 0;

	while (walker)
	{
		if (frames == gCreateLoopFrame) // 0 or 1
			hdr.loopoffset = (unsigned short)ftell(outfile);
		for (int subframe = 0; subframe < walker->mSubframes; subframe++)
		{
			int chunktype = 0;
			switch (walker->mFrameType[subframe])
			{
			case SAMEFRAME: chunktype = FLX_SAME;  printf("h"); break;
			case BLACKFRAME:chunktype = FLX_BLACK; printf("s"); break;
			case ONECOLOR:  chunktype = FLX_ONE;   printf("n"); break;
			case LZ1B:      chunktype = FLX_LZ1B;  printf("o"); break;
			case LZ4:       chunktype = FLX_LZ4;   printf("a"); break;
			case LZ5:       chunktype = FLX_LZ5;   printf("e"); break;
			case LZ6:       chunktype = FLX_LZ6;   printf("t"); break;
			case LZ3C:      chunktype = FLX_LZ3C;  printf("i"); break;
			default: printf("?!?\n");
			}
			fputc(chunktype, outfile);
			unsigned short ds = walker->mFrameDataSize[subframe];
			fwrite(&ds, 1, 2, outfile);
			if (ds)
			{
				fwrite(walker->mFrameData[subframe], 1, walker->mFrameDataSize[subframe], outfile);
			}
			fputc(walker->mChecksum1[subframe], outfile);
			fputc(walker->mChecksum2[subframe], outfile);

			if (walker->mFrameDataSize[subframe] > framemaxsize[walker->mFrameType[subframe]]) framemaxsize[walker->mFrameType[subframe]] = walker->mFrameDataSize[subframe];
			if (walker->mFrameDataSize[subframe] < frameminsize[walker->mFrameType[subframe]]) frameminsize[walker->mFrameType[subframe]] = walker->mFrameDataSize[subframe];

			framecounts[walker->mFrameType[subframe]]++;
			total += walker->mFrameDataSize[subframe];
			totalspans += walker->mSpans[subframe];
			if (subframe != walker->mSubframes-1)
				fputc(FLX_SUBFRAME, outfile);
		}
		walker = walker->mNext;
		frames++;

		fputc(FLX_NEXT, outfile);
	}
	printf("\nTotal %d bytes of payload (%d kB, %d MB), %d bytes overhead\n", total, total / 1024, total / (1024 * 1024), ftell(outfile) - total);
	printf("Compression ratio %3.3f%%, %d spans and %d bytes per frame on average\n", 100.0 * total / (double)(frames * header.mWidth * header.mHeight), totalspans / frames, total / frames);
	float mspf = ((totalspans / (float)frames) / 10.0f);
	float recommended_fps = 1000.0f / mspf;
	int retraces = (int)floor(mspf / 20) + 1;
	printf("Recommended frame rate based on spans: %3.3f (about %d retraces)\n", recommended_fps, retraces);
	printf("\nBlock types:\n");
	if (framecounts[1]) printf("h sameframe     %5d (%5d -%5d bytes)\n", framecounts[1], frameminsize[1], framemaxsize[1]);
	if (framecounts[2]) printf("s blackframe    %5d (%5d -%5d bytes)\n", framecounts[2], frameminsize[2], framemaxsize[2]);
	if (framecounts[7]) printf("n onecolor      %5d (%5d -%5d bytes)\n", framecounts[7], frameminsize[7], framemaxsize[7]);
	if (framecounts[13]) printf("o lz scheme 1b  %5d (%5d -%5d bytes)\n", framecounts[13], frameminsize[13], framemaxsize[13]);
	if (framecounts[17]) printf("a lz scheme 4   %5d (%5d -%5d bytes)\n", framecounts[17], frameminsize[17], framemaxsize[17]);
	if (framecounts[18]) printf("e lz scheme 5   %5d (%5d -%5d bytes)\n", framecounts[18], frameminsize[18], framemaxsize[18]);
	if (framecounts[19]) printf("t lz scheme 6   %5d (%5d -%5d bytes)\n", framecounts[19], frameminsize[19], framemaxsize[19]);
	if (framecounts[21]) printf("i lz scheme 3c  %5d (%5d -%5d bytes)\n", framecounts[21], frameminsize[21], framemaxsize[21]);

	hdr.frames = frames;

	fseek(outfile, 0, SEEK_SET);
	fwrite(&hdr, 1, sizeof(hdr), outfile); // finished header

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("\nTime elapsed: %3.3fs\n\n", elapsed_seconds.count());
}

enum optionIndex { UNKNOWN, HELP, FLC, FLX, HALFRES, DITHER, FASTSCALE, VERIFY, THREADS, FRAMEDELAY, GIF, INFO, QUICK, MINSPAN, LOSSY, KEYFRAMES, PALWIDTH, CHUNKYX, CHUNKYY, ANALYZE, SPANGOAL, MAXFRAMESIZE, LOOPFRAME, GRAPHICSMODE };
const option::Descriptor usage[] =
{
	{ UNKNOWN,		0, "", "",	option::Arg::None,				  "USAGE: nextfli outputfilename inputfilemask [options]\n\nOptions:"},
	{ HELP,			0, "h", "help", option::Arg::None,			  " -h --help\t Print usage and exit"},
	{ FLC,			0, "f", "flc", option::Arg::None,			  " -f --flc\t Output standard FLC format (default: use flx)"},
	{ FLX,			0, "x", "flx", option::Arg::None,			  " -x --flx\t Output nextfli FLX format (default: use flx)"},
	{ GIF,			0, "g", "gif", option::Arg::None,			  " -g --gif\t Output GIF format (default: use flx)"},
	{ QUICK,		0, "q", "quick", option::Arg::None,			  " -q --quick\t Compress faster, bigger files (default: don't)"},
	{ HALFRES,      0, "l", "halfres", option::Arg::None,         " -l --halfres\t Reduce output resolution to half x, half y (default: don't)"},
	{ DITHER,       0, "d", "dither", option::Arg::None,          " -d --dither\t Use ordered dither (default: don't)"},
	{ FASTSCALE,    0, "p", "fastscale", option::Arg::None,       " -p --fastscale\t Use fast image scaling (default: don't)"},
	{ VERIFY,       0, "v", "verify", option::Arg::None,          " -v --verify\tVerify encoding (default: don't)"},
	{ THREADS,      0, "t", "threads", option::Arg::Optional,     " -t --threads\tNumber of threads to use (default: num of logical cores)"},
	{ FRAMEDELAY,   0, "r", "framedelay", option::Arg::Optional,  " -r --framedelay\tFrame delay 1=50hz, 2=25hz, 3=16.7hz 4=12.5hz (default: 4)"},
	{ INFO,         0, "i", "info", option::Arg::Optional,        " -i --info\t Output frame info log to file (default: don't)"},
	{ MINSPAN,      0, "m", "minspan", option::Arg::Optional,     " -m --minspan\t Set minimum span. Trade file size for faster decode. (default: 0)"},
	{ LOSSY,        0, "L", "lossy", option::Arg::Optional,       " -L --lossy\t Don't update pixels with manhattan rgb distance x (default: 0)"},
	{ KEYFRAMES,    0, "K", "keyframes", option::Arg::Optional,   " -K --keyframes\t Don't apply lossy filter every n frames (default: inf)"},
	{ PALWIDTH,     0, "c", "colors", option::Arg::Optional,      " -c --colors\t Number of colors in palette. (default: 256)"},
	{ CHUNKYX,      0, "X", "chunkyx", option::Arg::Optional,     " -X --chunkyx\t Pixels per pixel horizontally (default: 1)"},
	{ CHUNKYY,      0, "Y", "chunkyy", option::Arg::Optional,     " -Y --chunkyy\t Pixels per pixel vertically (default: 1)"},
	{ ANALYZE,      0, "a", "analyze", option::Arg::None,         " -a --analyze\t Analyze existing file. Does not generate animation."},
	{ SPANGOAL,     0, "G", "spangoal", option::Arg::Optional,    " -G --spangoal\t Set span goal. Grows file size. Heavy. (default: 100000)"},
	{ MAXFRAMESIZE, 0, "S", "maxframesize", option::Arg::Optional," -S --maxframesize\t Maximum frame size. Use with -G. (default:8192)"},
	{ LOOPFRAME,    0, "o", "genloopframe", option::Arg::None,    " -o --genloopframe\t Generate loop frame. (default: loop to 1st frame)"},
	{ GRAPHICSMODE, 0, "e", "graphicsmode", option::Arg::Optional," -e --mode\t Graphics mode 0=256x192, 1=320x256, 2=640x256, 3=128x96"},
	{ UNKNOWN,      0, "", "", option::Arg::None,				  "Example:\n  nextfli test.flx \"frames*.png\" -r3 -d -iframelog.txt"},
	{ 0,0,0,0,0,0 }
};


int main(int parc, char* pars[])
{
	auto start = std::chrono::steady_clock::now();
	option::Stats stats(usage, parc - 1, pars + 1);
	assert(stats.buffer_max < 32 && stats.options_max < 32);
	option::Option options[32], buffer[32];
	option::Parser parse(true, usage, parc - 1, pars + 1, options, buffer);

	if (options[UNKNOWN])
	{
		for (option::Option* opt = options[UNKNOWN]; opt; opt = opt->next())
			printf("Unknown option: %s\n", opt->name);
		printf("Run without parameters for help.\n");
		return 0;
	}

	if (parse.error() || !options[ANALYZE])
	{

		if (parse.error() || parc < 3 || options[HELP] || parse.nonOptionsCount() != 2)
		{
			option::printUsage(std::cout, usage);
			return 0;
		}
		if (options[GIF])
		{
			gClassicBlocks = 0;
			gExtendedBlocks = 0;
		}
		else
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

		gThreads = std::thread::hardware_concurrency();

		if (options[QUICK]) gAggressive = 0;
		if (options[HALFRES]) gHalfRes = 1;
		if (options[DITHER]) gDither = 1;
		if (options[FASTSCALE]) gPointSample = 1;
		if (options[VERIFY]) gVerify = 1;
		if (options[LOOPFRAME]) gCreateLoopFrame = 1;
		if (options[THREADS])
		{
			if (options[THREADS].arg != 0 && options[THREADS].arg[0])
				gThreads = atoi(options[THREADS].arg);
			else
			{
				printf("Invalid threads. Example: -t7\n");
				return 0;
			}
		}
		if (options[MINSPAN])
		{
			if (options[MINSPAN].arg != 0 && options[MINSPAN].arg[0])
				gGlobalMinSpan = atoi(options[MINSPAN].arg);
			else
			{
				printf("Invalid min span. Example: -m20\n");
				return 0;
			}
		}
		if (options[FRAMEDELAY])
		{
			if (options[FRAMEDELAY].arg != 0 && options[FRAMEDELAY].arg[0])
				gFramedelay = atoi(options[FRAMEDELAY].arg);
			else
			{
				printf("Invalid framedelay. Example: -r3\n");
				return 0;
			}
		}
		if (options[LOSSY])
		{
			if (options[LOSSY].arg != 0 && options[LOSSY].arg[0])
				gLossy = atoi(options[LOSSY].arg);
			else
			{
				printf("Invalid lossy. Example: -L3\n");
				return 0;
			}
		}
		if (options[KEYFRAMES])
		{
			if (options[KEYFRAMES].arg != 0 && options[KEYFRAMES].arg[0])
				gLossyKeyframes = atoi(options[KEYFRAMES].arg);
			else
			{
				printf("Invalid keyframes. Example: -K25\n");
				return 0;
			}
		}
		if (options[INFO])
		{
			if (options[INFO].arg == 0 || !options[INFO].arg[0])
			{
				printf("Invalid infolog. Example: -iinfolog.txt\n");
				return 0;
			}
		}
		if (options[PALWIDTH])
		{
			if (options[PALWIDTH].arg != 0 && options[PALWIDTH].arg[0])
				gPaletteWidth = atoi(options[PALWIDTH].arg);
			else
			{
				printf("Invalid number of colors. Example: -c16\n");
				return 0;
			}
		}
		if (options[CHUNKYX])
		{
			if (options[CHUNKYX].arg != 0 && options[CHUNKYX].arg[0])
				gChunkyX = atoi(options[CHUNKYX].arg);
			else
			{
				printf("Invalid chunky x. Example: -X3\n");
				return 0;
			}
		}
		if (options[CHUNKYY])
		{
			if (options[CHUNKYY].arg != 0 && options[CHUNKYY].arg[0])
				gChunkyX = atoi(options[CHUNKYY].arg);
			else
			{
				printf("Invalid chunky y. Example: -Y3\n");
				return 0;
			}
		}
		if (options[SPANGOAL])
		{
			if (options[SPANGOAL].arg != 0 && options[SPANGOAL].arg[0])
				gSpanGoal = atoi(options[SPANGOAL].arg);
			else
			{
				printf("Invalid span goal. Example: -G600\n");
				return 0;
			}
		}
		if (options[MAXFRAMESIZE])
		{
			if (options[MAXFRAMESIZE].arg != 0 && options[MAXFRAMESIZE].arg[0])
				gMaxFrameSize = atoi(options[MAXFRAMESIZE].arg);
			else
			{
				printf("Invalid max frame size. Example: -S32768\n");
				return 0;
			}
		}
		if (options[GRAPHICSMODE])
		{
			if (options[GRAPHICSMODE].arg != 0 && options[GRAPHICSMODE].arg[0])
				gGraphicsMode = atoi(options[GRAPHICSMODE].arg);
			else
			{
				printf("Invalid graphics mode. Example: -e1\n");
				return 0;
			}
		}
		if (gGraphicsMode > 3 || gGraphicsMode < 0)
		{
			printf("Graphics mode out of range (%d). Valid values are 0,1,2 and 3.\n", gGraphicsMode);
			return 0;
		}

		if (gFramedelay <= 0)
			gFramedelay = 1;

		if (gThreads < 0)
			gThreads = 0;

		FILE* outfile;
		FliHeader header;

		switch (gGraphicsMode)
		{
		case 0:
			header.mWidth = 256;
			header.mHeight = 192;
			break;
		case 1:
			header.mWidth = 256;
			header.mHeight = 320;
			break;
		case 2:
			header.mWidth = 256;
			header.mHeight = 640;
			break;
		case 3:
			header.mWidth = 128;
			header.mHeight = 96;
			break;
		}

		header.mFrames = 0;

		if (gGraphicsMode == 2 && gPaletteWidth > 16) gPaletteWidth = 16;


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
		if (gCreateLoopFrame)
		{
			addLoopFrame(header);
		}
		encode(header);
		printf("Opening %s for writing..\n", parse.nonOption(0));
		outfile = fopen(parse.nonOption(0), "wb");
		if (!outfile)
		{
			printf("file open failed\n");
			return 0;
		}
		if (options[GIF])
		{
			fclose(outfile);
			outfile = 0;
			output_gif(header, parse.nonOption(0));
		}
		else
			if (options[FLC])
			{
				output_flc(header, outfile);
			}
			else
			{
				output_flx(header, outfile);
			}
		if (outfile) fclose(outfile);
	}

	if (options[VERIFY] || options[INFO])
	{
		verifyfile(parse.nonOption(0), options[INFO] ? options[INFO].arg : 0);
	}

	if (options[INFO])
	{
		printf("Frame info log written to %s\n", options[INFO].arg);
	}

	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	printf("\nTotal time elapsed: %3.3fs\n", elapsed_seconds.count());

	return 0;
}
