enum FRAMETYPE
{
	SAMEFRAME = 1,
	BLACKFRAME = 2,
	RLEFRAME = 3,
	DELTA8FRAME = 4,
	DELTA16FRAME = 5,
	// extended blocks
	ONECOLOR = 6,
	LINEARRLE8 = 7,
	LINEARRLE16 = 8,
	LINEARDELTA8 = 9,
	LINEARDELTA16 = 10,
	LZ1 = 11,
	LZ2 = 12,
	LZ3 = 13
}; 

// FLI header
struct FliHeader
{
	int       mSize; // Should be same as file size.
	unsigned short int mMagic; // 0AF12h
	short int mFrames; // Frames in flic, 4000 max
	short int mWidth; // x size
	short int mHeight;// y size
	short int mDepth; // bits per pixel, always 8 in fli/flc
	short int mFlags; // Bitmapped flags. 0=ring frame, 1=header updated
	short int mSpeed; // Delay between frames in ms (or retraces :))
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
	char      mReserved3[42];

	FliHeader()
	{
		// Some more or less good defaults
		mSize = 0;
		mMagic = 0xAF12;
		mFrames = 0;
		mWidth = 0;
		mHeight = 0;
		mDepth = 8;
		mFlags = 3;
		mSpeed = 80;
		mReserved1 = 0;
		mCreated = 0;
		mCreator = 0x50181ee7;
		mUpdated = 0;
		mUpdater = mCreator;
		mAspectx = 4;
		mAspecty = 3;
		mOframe1 = 0;
		mOframe2 = 0;
		for (int i = 0; i < 38; i++)
			mReserved2[i] = 0;
		for (int i = 0; i < 42; i++)
			mReserved3[i] = 0;
	}
};

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
	int mFrameType;
	unsigned int mPalette[256];
	int mPaletteChanged;
	unsigned char* mFrameData;
	int mFrameDataSize;

	unsigned int* mRgbPixels;
	unsigned char* mIndexPixels;

	Frame* mNext;

	Frame() : mFrameType(0), mFrameData(0), mFrameDataSize(0), mPaletteChanged(0), mRgbPixels(0), mIndexPixels(0), mNext(0)
	{
		for (int i = 0; i < 256; i++)
			mPalette[i] = 0;
	}
};

int encodeDelta16Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int width, int height);
int encodeDelta8Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int width, int height);
int encodeRLEFrame(unsigned char* aRLEframe, unsigned char* aIndexPixels, int width, int height);

int encodeLZ3Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels);
int encodeLZ2Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels);
int encodeLZ1Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels);
int encodeLinearDelta16Frame(unsigned short* data, unsigned short* aFrame, unsigned short* aPrev, int pixels);
int encodeLinearDelta8Frame(unsigned char* data, unsigned char* aFrame, unsigned char* aPrev, int pixels);
int encodeLinearRLE16Frame(unsigned short* data, unsigned short* src, int pixels);
int encodeLinearRLE8Frame(unsigned char* data, unsigned char* src, int pixels);

int verify_frame(Frame* aFrame, Frame* aPrev, int aWidth, int aHeight);
