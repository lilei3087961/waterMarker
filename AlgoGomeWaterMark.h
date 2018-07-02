#ifndef GOME_ALGO_GOME_WATERMARK_H_
#define GOME_ALGO_GOME_WATERMARK_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#undef SUPPORT_LIBPNG
#define SUPPORT_LIBPNG

#define ALIGN(x, mask) (((x) + (mask) - 1) & ~ ((mask) - 1))
#define WATER_MARK_PATH_PREFIX "/system/media/%s.png"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagBitmapFileHeader {
    unsigned int size;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int offBits;
} BitmapFileHeader;

typedef struct tagBitmapInfoHeader {
    unsigned int size;
    unsigned int width;
    unsigned int height;
    unsigned short planes;
    unsigned short bitCount;
    unsigned int compression;
    unsigned int sizeImage;
    unsigned int xPerMeter;
    unsigned int yPerMeter;
    unsigned int clrUsed;
    unsigned int clrImportant;
} BitmapInfoHeader;

typedef struct tagBitmapPixel {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} BitmapPixel;

typedef struct tagBitmapData {
    BitmapFileHeader fileHeader;
    BitmapInfoHeader infoHeader;
    BitmapPixel* pPixels;
} BitmapData;

typedef struct tagYuvDataBlock {
    unsigned int width;
    unsigned int height;
    unsigned int widthAlign;
    unsigned int heightAlign;
    unsigned int pX;
    unsigned int pY;
    unsigned int formatType;
    unsigned char* pBuf;
    unsigned int alphaOffset;
} YuvDataBlock;

typedef struct tagExtractAlphaData {
    BitmapData* pData;
    YuvDataBlock* pYuv;
} ExtractAlphaData;

typedef struct tagWaterMarkContext {
    YuvDataBlock* pSrc;
    YuvDataBlock* pDst;
    int width;
    int heightOffset;
    int height;
    unsigned char* pAlpha;
} WaterMarkContext;

#ifdef SUPPORT_LIBPNG
BitmapData* PNG_ARGB8888(FILE *pngFile);
#endif

void freeBitmap(BitmapData* pData);
void extractAlphas(BitmapData* pData, unsigned char* pAlphas, unsigned int widthPitch, unsigned int heightPitch);
void watermark(YuvDataBlock* pSrc, YuvDataBlock* pDst, int width, int height, unsigned char* pAlpha);

#ifdef __cplusplus
}
#endif
#endif  // GOME_ALGO_GOME_WATERMARK_H_
