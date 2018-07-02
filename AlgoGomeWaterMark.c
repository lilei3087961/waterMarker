#undef LOG_TAG
#define LOG_TAG "AlgoGomeWaterMark"

#include <mtkcam/middleware/v1/camshot/AlgoGomeWaterMark.h>

#ifdef SUPPORT_LIBPNG
#include "png.h"
#endif // SUPPORT_LIBPNG

#include <utils/Log.h>

#define ALPHA_RATE(value, alpha) ((value) * (alpha) >> 8)

#ifdef SUPPORT_LIBPNG
unsigned int component(png_const_bytep row, png_uint_32 x, unsigned int c, unsigned int bit_depth, unsigned int channels) {
    png_uint_32 bit_offset_hi = bit_depth * ((x >> 6) * channels);
    png_uint_32 bit_offset_lo = bit_depth * ((x & 0x3f) * channels + c);

    row = (png_const_bytep)(((PNG_CONST png_byte(*)[8])row) + bit_offset_hi);
    row += bit_offset_lo >> 3;
    bit_offset_lo &= 0x07;

    switch (bit_depth) {
    case 1:
        return (row[0] >> (7 - bit_offset_lo)) & 0x01;
    case 2:
        return (row[0] >> (6 - bit_offset_lo)) & 0x03;
    case 4:
        return (row[0] >> (4 - bit_offset_lo)) & 0x0f;
    case 8:
        return row[0];
    case 16:
        return (row[0] << 8) + row[1];
    default:
        fprintf(stderr, "pixel_component: invalid bit depth %u\n", bit_depth);
        return row[0];
    }
}

void readPixel(png_const_bytep row, png_uint_32 x, png_uint_32 bit_depth, png_uint_32 color_type, BitmapPixel *pPixel) {
    unsigned int channels = PNG_COLOR_TYPE_RGB_ALPHA ? 4 : 3;
    pPixel->r = component(row, x, 0, bit_depth, channels);
    pPixel->g = component(row, x, 1, bit_depth, channels);
    pPixel->b = component(row, x, 2, bit_depth, channels);
    pPixel->a = color_type == PNG_COLOR_TYPE_RGB_ALPHA ? component(row, x, 3, bit_depth, channels) : 0xFF;
}

BitmapData* PNG_ARGB8888(FILE *pngFile) {
    BitmapData *pData;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep row = NULL;
    png_uint_32 width, height, ystart, xstart, ystep, xstep, px, ppx, py, count;
    png_int_32 bit_depth, color_type, interlace_method, compression_method, filter_method, passes, pass;

    if (!pngFile)
        return NULL;

    if (!(png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
        ALOGE("PNG_ARGB8888: out of memory allocating png_struct!\n");
        return NULL;
    }

    if (!(info_ptr = png_create_info_struct(png_ptr))
            || setjmp(png_jmpbuf(png_ptr))) {
        ALOGE("PNG_ARGB8888: out of memory allocating png_info!\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    png_init_io(png_ptr, pngFile);
    png_read_info(png_ptr, info_ptr);
    row = (png_bytep)png_malloc(png_ptr, png_get_rowbytes(png_ptr, info_ptr));

    if (!png_get_IHDR(png_ptr, info_ptr, &width, &height,
                      &bit_depth, &color_type, &interlace_method,
                      &compression_method, &filter_method) || !(color_type & PNG_COLOR_TYPE_RGB)) {
        ALOGE("PNG_ARGB8888: png_get_IHDR failed!\n");
        row = NULL;
        png_free(png_ptr, row);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    switch (interlace_method) {
    case PNG_INTERLACE_NONE:
        passes = 1;
        break;
    case PNG_INTERLACE_ADAM7:
        passes = PNG_INTERLACE_ADAM7_PASSES;
        break;
    default:
        ALOGE("PNG_ARGB8888: png unknown interlace!\n");
        row = NULL;
        png_free(png_ptr, row);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    pData = (BitmapData*)malloc(sizeof(BitmapData));
    if (!pData) {
        ALOGE("PNG_ARGB8888: allocate memory for bitmap data failed!\n");
        row = NULL;
        png_free(png_ptr, row);
        png_destroy_info_struct(png_ptr, &info_ptr);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    png_start_read_image(png_ptr);
    for (pass = 0; pass < passes; ++pass) {
        if (interlace_method == PNG_INTERLACE_ADAM7) {
            if (PNG_PASS_COLS(width, pass) == 0)
                continue;

            xstart = PNG_PASS_START_COL(pass);
            ystart = PNG_PASS_START_ROW(pass);
            xstep = PNG_PASS_COL_OFFSET(pass);
            ystep = PNG_PASS_ROW_OFFSET(pass);
        } else {
            ystart = xstart = 0;
            ystep = xstep = 1;
        }
        ALOGE("(%d, %d, %d, %d, %d, %d, %d)\n", xstart, ystart, xstep, ystep, width, height, sizeof(BitmapPixel));

        pData->infoHeader.width = width / xstep;
        pData->infoHeader.height = height / ystep;
        pData->fileHeader.size = sizeof(BitmapPixel) * pData->infoHeader.width * pData->infoHeader.height;
        pData->pPixels = (BitmapPixel*)malloc(pData->fileHeader.size);
        if (!pData->pPixels) {
            ALOGE("PNG_ARGB8888: allocate memory for bitmap pixels failed!\n");
            break;
        }

        count = 0;
        for (py = ystart; py < ystart + height; py += ystep) {
            png_read_row(png_ptr, row, NULL);
            for (px = xstart, ppx = 0; px < xstart + width; px += xstep, ppx++) {
                readPixel(row, ppx, bit_depth, color_type, pData->pPixels + count);
                count++;
            }
        }
        break;
    }

    row = NULL;
    png_free(png_ptr, row);
    png_destroy_info_struct(png_ptr, &info_ptr);
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    return pData;
}
#endif // SUPPORT_LIBPNG

void freeBitmap(BitmapData* pData) {
    if (!pData) return;
    free(pData->pPixels);
    free(pData);
}

void extractAlphas(BitmapData* pData, unsigned char* pAlphas, unsigned int widthPitch, unsigned int heightPitch) {
    unsigned int i, j;
    for (i = 0; i < pData->infoHeader.height; i++) {
        for (j = 0; j < pData->infoHeader.width; j++) {
            pAlphas[i * widthPitch + j] = pData->pPixels[i * widthPitch + j].a;
        }
    }
}

void* extractAlphaThread(void* args) {
    int widthPitch, heightPitch;
    ExtractAlphaData* p = (ExtractAlphaData*)args;
    if (!p) {
        return NULL;
    }

    widthPitch = ALIGN(p->pYuv->width, p->pYuv->widthAlign);
    heightPitch = ALIGN(p->pYuv->height, p->pYuv->heightAlign);
    extractAlphas(p->pData, p->pYuv->pBuf + p->pYuv->alphaOffset, widthPitch, heightPitch);
    return p;
}

void* watermark_NV21(void* args) {
    ALOGD("watermark: watermark_NV21  +++++++++++++ ");
    WaterMarkContext* p = (WaterMarkContext*)args;
    int srcPitchWidth = ALIGN(p->pSrc->width, p->pSrc->widthAlign);
    int srcPitchHeight = ALIGN(p->pSrc->height, p->pSrc->heightAlign);
    int dstPitchWidth = ALIGN(p->pDst->width, p->pDst->widthAlign);
    int dstPitchHeight = ALIGN(p->pDst->height, p->pDst->heightAlign);
    int i, j;
    unsigned char *alpha1, *alpha2, alpha;
    for(i = p->heightOffset; i < p->height; i += 2) {
        for (j = 0; j < p->width; j++) {
            alpha1 = p->pAlpha + i * srcPitchWidth + j;
            alpha2 = alpha1 + srcPitchWidth;
            alpha = (*alpha1 + *alpha2) >> 1;

            if (*alpha1) {
                p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidth + p->pDst->pX + j] // Y1
                    = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i) * srcPitchWidth + p->pSrc->pX + j], *alpha1)
                    + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidth + p->pDst->pX + j], ~*alpha1 & 0xFF);
            }
            if (*alpha2) {
                p->pDst->pBuf[(p->pDst->pY + i + 1) * dstPitchWidth + p->pDst->pX + j] // Y2
                    = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i + 1) * srcPitchWidth + p->pSrc->pX + j], *alpha2)
                    + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i + 1) * dstPitchWidth + p->pDst->pX + j], ~*alpha2 & 0xFF);
            }
#if 0
            if (alpha) {
                p->pDst->pBuf[(((p->pDst->pY + i) >> 1) + dstPitchHeight) * dstPitchWidth + p->pDst->pX + j] // UV
                    = ALPHA_RATE(p->pSrc->pBuf[(((p->pSrc->pY + i) >> 1) + srcPitchHeight) * srcPitchWidth + p->pSrc->pX + j], alpha)
                    + ALPHA_RATE(p->pDst->pBuf[(((p->pDst->pY + i) >> 1) + dstPitchHeight) * dstPitchWidth + p->pDst->pX + j], ~alpha & 0xFF);
            }
#endif
        }
    }
    ALOGD("watermark: watermark_NV21  -------------");
    return p;
}

void* watermark_YUYV(void* args) {  
    ALOGE("watermark: watermark_YUYV\n");
    WaterMarkContext* p = (WaterMarkContext*)args;
    int srcPitchWidth = ALIGN(p->pSrc->width, p->pSrc->widthAlign), srcPitchWidthSize = srcPitchWidth << 1;
    int srcPitchHeight = ALIGN(p->pSrc->height, p->pSrc->heightAlign);
    int dstPitchWidth = ALIGN(p->pDst->width, p->pDst->widthAlign), dstPitchWidthSize = dstPitchWidth << 1;
    int dstPitchHeight = ALIGN(p->pDst->height, p->pDst->heightAlign);
    int i, j;
    unsigned char *alpha1, *alpha2, alpha;
    for (i = p->heightOffset; i < p->height; i++) {
        for (j = 0; j < (p->width << 1); j += 4) {
            alpha1 = p->pAlpha + (p->pSrc->pY + i) * srcPitchWidth + p->pSrc->pX + (j >> 1);
            alpha2 = alpha1 + 1;
            alpha = (*alpha1 + *alpha2) >> 1;

            if (*alpha1) {
                p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidthSize + (p->pDst->pX << 1) + j] // Y1
                    = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i) * srcPitchWidthSize + (p->pSrc->pX << 1) + j], *alpha1)
                    + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidthSize + (p->pDst->pX << 1) + j], ~*alpha1 & 0xFF);
            }
            if (*alpha2) {
                p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidthSize + (p->pDst->pX << 1) + j + 2] // Y2
                    = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i) * srcPitchWidthSize + (p->pSrc->pX << 1) + j + 2], *alpha2)
                    + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidthSize + (p->pDst->pX << 1) + j + 2], ~*alpha2 & 0xFF);
            }
            if (alpha) {
                p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidthSize + (p->pDst->pX << 1) + j + 1] // U
                    = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i) * srcPitchWidthSize + (p->pSrc->pX << 1) + j + 1], alpha)
                    + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidthSize + (p->pDst->pX << 1) + j + 1], ~alpha & 0xFF);
                p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidthSize + (p->pDst->pX << 1) + j + 3] // V
                    = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i) * srcPitchWidthSize + (p->pSrc->pX << 1) + j + 3], alpha)
                    + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i) * dstPitchWidthSize + (p->pDst->pX << 1) + j + 3], ~alpha & 0xFF);
            }
        }
    }
    return p;
}

void* watermark_YUVA(void* args) {
    WaterMarkContext* p = (WaterMarkContext*)args;
    int srcPitchWidth = ALIGN(p->pSrc->width, p->pSrc->widthAlign);
    int srcWidthSize = srcPitchWidth << 2;
    int srcPitchHeight = ALIGN(p->pSrc->height, p->pSrc->heightAlign);
    int dstPitchWidth = ALIGN(p->pDst->width, p->pDst->widthAlign);
    int dstWidthSize = dstPitchWidth << 2;
    int dstPitchHeight = ALIGN(p->pDst->height, p->pDst->heightAlign);
    int i, j;
    unsigned char alpha;
    for (i = p->heightOffset; i < p->height; i++) {
        for (j = 0; j < p->width; j++) {
            alpha = *(p->pAlpha + (p->pSrc->pY + i) * srcPitchWidth + p->pSrc->pX + j);
            if (!alpha) {
                continue;
            }
            p->pDst->pBuf[(p->pDst->pY + i) * dstWidthSize + ((p->pDst->pX + j) << 2)] // Y
                = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i) * srcWidthSize + ((p->pSrc->pX + j) << 2)], alpha)
                + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i) * dstWidthSize + ((p->pDst->pX + j) << 2)], ~alpha & 0xFF);
            p->pDst->pBuf[(p->pDst->pY + i) * dstWidthSize + ((p->pDst->pX + j) << 2) + 1] // U
                = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i) * srcWidthSize + ((p->pSrc->pX + j) << 2) + 1], alpha)
                + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i) * dstWidthSize + ((p->pDst->pX + j) << 2) + 1], ~alpha & 0xFF);
            p->pDst->pBuf[(p->pDst->pY + i) * dstWidthSize + ((p->pDst->pX + j) << 2) + 2] // V
                = ALPHA_RATE(p->pSrc->pBuf[(p->pSrc->pY + i) * srcWidthSize + ((p->pSrc->pX + j) << 2) + 2], alpha)
                + ALPHA_RATE(p->pDst->pBuf[(p->pDst->pY + i) * dstWidthSize + ((p->pDst->pX + j) << 2) + 2], ~alpha & 0xFF);
        }
    }
    return p;
}

void watermark(YuvDataBlock* pSrc, YuvDataBlock* pDst, int width, int height, unsigned char* pAlpha) {
    WaterMarkContext context;
    ALOGD("SrcYUV: %dx%d\n", pSrc->width, pSrc->height);
    ALOGD("DstYUV: %dx%d\n", pDst->width, pDst->height);
    context.pSrc = pSrc;
    context.pDst = pDst;
    context.pAlpha = pAlpha;
    context.width = width;
    context.height = height;
    context.heightOffset = pSrc->pY;
    ALOGD("watermark: context size(%d, %d), formatType=%d\n", width, height, pSrc->formatType);
    if (pSrc->formatType == 1) { // NV21
        watermark_NV21(&context);
    } else if (pSrc->formatType == 2) { // YUYV
        watermark_YUYV(&context);
    } else if (pSrc->formatType == 3) { // YUVA
        //watermark_YUVA(&context);
    } // TODO: support other format types.
}
