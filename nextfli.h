
enum FLXBLOCKS
{
	FLX_NEXT = 0,
	FLX_SAME = 3,
	FLX_BLACK = 6,
	FLX_ONE = 9,
	FLX_LZ1B = 12,
	FLX_LZ4 = 15,
	FLX_LZ5 = 18,
	FLX_LZ6 = 21,
	FLX_LZ3C = 24,
	FLX_SUBFRAME = 27,
};

enum FRAMETYPE
{
	SAMEFRAME = 1,
	BLACKFRAME = 2,
	RLEFRAME = 3,
	DELTA8FRAME = 4,
	DELTA16FRAME = 5,
	FLI_COPY = 6,
	// extended blocks
	ONECOLOR = 7,
	LINEARRLE8 = 8,
	LINEARRLE16 = 9,
	LINEARDELTA8 = 10,
	LINEARDELTA16 = 11,
	LZ1 = 12,
	LZ1B = 13,
	LZ2 = 14,
	LZ2B = 15,
	LZ3 = 16,
	LZ4 = 17,
	LZ5 = 18,
	LZ6 = 19,
	LZ3B = 20,
	LZ3C = 21,
	LZ3D = 22,
	LZ3E = 23,
	FRAMETYPE_MAX
}; 

// FLI header
#pragma pack(push, 1)
struct FliHeader
{
	int       mSize; // Should be same as file size.
	unsigned short int mMagic; // 0AF12h
	short int mFrames; // Frames in flic, 4000 max
	short int mWidth; // x size
	short int mHeight;// y size
	short int mDepth; // bits per pixel, always 8 in fli/flc
	short int mFlags; // Bitmapped flags. 0=ring frame, 1=header updated
    int       mSpeed; // Delay between frames in ms (or retraces :))
	short int mReserved1;
	int       mCreated; // MS-dos date of creation
	int       mCreator; // SerNo of animator, 0464c4942h for FlicLib
	int       mUpdated; // MS-dos date of last modify
	int       mUpdater;
	short int mAspectx; // x-axis aspect ratio (320x200: 6)
	short int mAspecty; // y-axis aspect ratio (320x200: 5)
	char      mReserved2[38];
	int       mOframe1; // offset to frame 1 
	int       mOframe2; // offset to frame 2 - for looping, jump here
	char      mReserved3[40];

	FliHeader()
	{
		// Some more or less good defaults
		mSize = 0;
		mMagic = 0xAF12; // "standard FLC"
		mFrames = 0;
		mWidth = 0;
		mHeight = 0;
		mDepth = 8;
		mFlags = 3;
		mSpeed = 80; // 1000/80ms = 12.5Hz, or 1/4 of 50hz
		mReserved1 = 0;
		mCreated = 0;
		mCreator = 0xdecafbad;
		mUpdated = 0;
		mUpdater = mCreator;
		mAspectx = 4;
		mAspecty = 3;
		mOframe1 = 0;
		mOframe2 = 0;
		for (int i = 0; i < 38; i++)
			mReserved2[i] = 0;
		for (int i = 0; i < 40; i++)
			mReserved3[i] = 0;
	}
};
#pragma pack(pop)

struct FliFrameHeader
{
	int       mFramesize;
	unsigned short int mMagic;
	short int mChunks;
	unsigned char mReserved[8];

	FliFrameHeader()
	{
		mFramesize = 0;
		mMagic = 0;
		mChunks = 0;
		for (int i = 0; i < 8; i++)
			mReserved[i] = 0;
	}
};

struct FliChunkHeader
{
	int       mSize;
	short int mType;

	FliChunkHeader()
	{
		mSize = 0;
		mType = 0;
	}
};

struct Frame
{
	int mSubframes = 1;
	int mFrameType[2] = {};
	unsigned int mPalette[256] = {};
	int mPaletteChanged = 0;
	unsigned char* mFrameData[2] = {};
	int mFrameDataSize[2] = {};

	unsigned int* mRgbPixels = 0;
	unsigned char* mIndexPixels = 0;

	Frame* mNext = 0;

	unsigned char mChecksum1[2] = {}, mChecksum2[2] = {};
	unsigned int mSpans[2] = {};
};


int encodeDelta16Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int width, int height);
int encodeDelta8Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int width, int height);
int encodeRLEFrame(unsigned char* aRLEframe, unsigned char* aIndexPixels, int width, int height);

int encodeLZ1bFrame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int srcpixels, int pixels, int minspan, int& spans);
int encodeLZ4Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aSourceWindow, int srcpixels, int pixels, int minspan, int& spans);
int encodeLZ5Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int srcpixels, int pixels, int minspan, int& spans);
int encodeLZ6Frame(unsigned char* data, unsigned char* aSourceWindow, unsigned char* aFrame, unsigned char* aPrev, int srcpixels, int pixels, int minspan, int& spans);
int encodeLZ3CFrame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int srcpixels, int pixels, int minspan, int &spans);

int verify_frame(Frame* aFrame, Frame* aPrev, int aWidth, int aHeight, int aSubframe);

int runlength(unsigned char* data, int max);
int runlength16(unsigned char* cdata, int max);
int skiplength(unsigned char* data, unsigned char* prev, int max);
int skiplength16(unsigned char* cdata, unsigned char* cprev, int max);
int bestLZRun0(unsigned char* aFrame, unsigned char* aPrev, int ofs, int pixels, int& runofs, int max);
int bestLZRun3(unsigned char* aFrame, unsigned char* aPrev, int ofs, int srcpixels, int dstpixels, signed char& runofs, int max);

void verifyfile(const char* fn, const char * logfilename);
