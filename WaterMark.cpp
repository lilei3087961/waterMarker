#undef LOG_TAG
#define LOG_TAG "WaterMark"
#include <mtkcam/middleware/v1/camshot/WaterMark.h>
#include <utils/Log.h>
#include <math.h> 
//lwl add waterMark begin
#include <mtkcam/middleware/v1/camshot/AlgoGomeWaterMark.h>
#define NAME "GomeWaterMark"
#include <mtkcam/drv/iopipe/SImager/ISImager.h>
#include <feature/common/utils/BufAllocUtil.h>
#include <math.h> 
#define PICTURE_SIZE_WIDTH_2560 2560
#define PICTURE_SIZE_WIDTH_3120 3120
#define PICTURE_SIZE_WIDTH_2448 2448
#define PICTURE_SIZE_WIDTH_1536 1536
#define PICTURE_SIZE_WIDTH_1584 1584
#define PICTURE_SIZE_WIDTH_1920 1920
#define PICTURE_SIZE_WIDTH_1568 1568

#include <mtkcam/middleware/v1/camshot/_params.h>
//lwl add waterMark end

void WaterMark::watermark(NSCam::IImageBuffer *pImgBuf, int yuvType, MUINT32 w, MUINT32 h)
{
    ALOGD("watermark: lwl yuvType=%d,w=%d,h=%d\n", yuvType, w , h);
    //yuvType only for 0x11(nv21) and 0x14(yuy2/yuyv)
    formatType = (yuvType == NSCam::eImgFmt_NV21) ? 1 : (yuvType == NSCam::eImgFmt_YUY2 ? 2 : 3);   
    ALOGD("watermark: formatType=%d\n", formatType);
    if(formatType == 1 || formatType == 2)
    {
        mJpgWidth = w;
        mJpgHeight = h;
        watermark_YUV(pImgBuf);
    }else{
        ALOGE("watermark: yuvType is not support!\n");
    }
    
}

void WaterMark::watermark_YUV(NSCam::IImageBuffer *pImgBuf)
{
	requestBufs();
    if(extractPNG())
    {
        WaterMarkProcess((MUINT8 *)pImgBuf->getBufVA(0), mJpgWidth, mJpgHeight);   
    }
    releaseBufs(); 
}

//lwl watermark begin
/*******************************************************************************
*
********************************************************************************/
MBOOL
WaterMark::
extractPNG()
{
    ALOGD("[extractPNG] - E.");
    String8  mWaterMarkName;
    switch(mJpgWidth)
    {
        case PICTURE_SIZE_WIDTH_2560:
            mWaterMarkName = "WaterMark_203";
        break;
        case PICTURE_SIZE_WIDTH_3120:
            mWaterMarkName = "WaterMark_162";
        break;
        case PICTURE_SIZE_WIDTH_2448:
            ///mWaterMarkName = "WaterMark_229";
			mWaterMarkName = "WaterMark_162";
        break;
        case PICTURE_SIZE_WIDTH_1536:
            mWaterMarkName = "WaterMark_153";
        break;
        case PICTURE_SIZE_WIDTH_1584:
        case PICTURE_SIZE_WIDTH_1920:
        case PICTURE_SIZE_WIDTH_1568:
            mWaterMarkName = "WaterMark_119";
        break;
        default:
            mWaterMarkName = "WaterMark_153";
        break;
    }
    if(mWaterMarkName.isEmpty() || 0 == strcmp(mWaterMarkName, "none"))
        return MFALSE;
    char path[256];
    pSrc = (YuvDataBlock*)malloc(sizeof(YuvDataBlock));
    memset(pSrc, 0, sizeof(YuvDataBlock));

    sprintf(path, WATER_MARK_PATH_PREFIX, (char const*) mWaterMarkName);
    ALOGD("path = %s \n",path);
    FILE* f = fopen(path, "rb");
    if (!f) {
        ALOGD("watermark: open png file %s failed!\n", path);
        return MFALSE;
    }

#ifdef SUPPORT_LIBPNG
    BitmapData* pData = PNG_ARGB8888(f);
#else // TODO: provide other method to read png if libpng not found
    BitmapData* pData = NULL;
#endif
    if (!pData) {
        ALOGD("watermark: read png file %s failed!\n", path);
        fclose(f);
        return MFALSE;
    }
    fclose(f);
    NSCam::IImageBuffer* mpARGBSource = allocMem(NSCam::eImgFmt_RGBA8888, pData->infoHeader.width, pData->infoHeader.height);
    NSCam::IImageBuffer* mpYUYVSource = allocMem(NSCam::eImgFmt_YUY2, pData->infoHeader.width, pData->infoHeader.height);
    NSCam::IImageBuffer* mpNV21Source = allocMem(NSCam::eImgFmt_NV21, pData->infoHeader.width, pData->infoHeader.height);

    unsigned int pixelSize = pData->infoHeader.width * pData->infoHeader.height;
    pSrc->width = pData->infoHeader.width;
    pSrc->height = pData->infoHeader.height;
    pSrc->widthAlign = 1;  // !
    pSrc->heightAlign = 1; // !
    pSrc->pX = 0;
    pSrc->pY = 0;
    {
        // formatType = 1 for NV21, 2 for yuyv
        ALOGD("waterMark : formatType = %d \n",formatType);
        pSrc->formatType = formatType;
        pSrc->alphaOffset = pixelSize * sizeof(BitmapPixel);
        if (pSrc->pBuf) {
            free(pSrc->pBuf);
        };
        pSrc->pBuf = (unsigned char*)malloc((pSrc->alphaOffset + pixelSize) * sizeof(unsigned char));
        if (!pSrc->pBuf)
        {
            ALOGE("watermark: allocate memory for watermark failed! VERY VERY VERY CRITICAL!\n");
        } else {
            ExtractAlphaData exData;
            exData.pData = pData;
            exData.pYuv = pSrc;
            extractAlpha(&exData);

            memcpy((void*)mpARGBSource->getBufVA(0), (void*)pData->pPixels, pixelSize * 4);
            ImgProcess(mpARGBSource, pData->infoHeader.width, pData->infoHeader.height, NSCam::eImgFmt_RGBA8888, mpYUYVSource, pData->infoHeader.width, pData->infoHeader.height, NSCam::eImgFmt_YUY2, 0);
            if(formatType == 1)//nv21
            {
                ImgProcess(mpYUYVSource, pData->infoHeader.width, pData->infoHeader.height, NSCam::eImgFmt_YUY2, mpNV21Source, pData->infoHeader.width, pData->infoHeader.height, NSCam::eImgFmt_NV21, 0);
                memcpy((void *)pSrc->pBuf, (void *)mpNV21Source->getBufVA(0), pixelSize*3/2);
            }else if(formatType == 2){//yuy2/yuyv
                memcpy((void *)pSrc->pBuf, (void *)mpYUYVSource->getBufVA(0), pixelSize * 2);
            }else{
                ALOGE("watermark: the preview format is not support!\n");
                return MFALSE;
            }
        }
    }
    freeBitmap(pData);
    deallocMem(mpARGBSource);
    deallocMem(mpYUYVSource);
    deallocMem(mpNV21Source);
    ALOGD("[extractPNG] - X.");
    return MTRUE;
}

/*******************************************************************************
*
********************************************************************************/
MBOOL
WaterMark::
WaterMarkProcess(MUINT8* Srcbuf, MUINT32 w, MUINT32 h)
{
    ALOGD("VendorProcess :in\n");

    if (!pSrc || !pSrc->pBuf || !pDst) {
        ALOGD("watermark: init buffer failed!\n");
        return MFALSE;
    }
    pDst->pBuf = Srcbuf;
    pDst->width = w;
    pDst->height = h;
    pDst->widthAlign = 1;
    pDst->heightAlign = 1;
    pDst->pX = round(pSrc->width/5);
    pDst->pY = pDst->height - 2*pSrc->height;

    ALOGD("watermark --->: %d, %d\n", w, h);
    ::watermark(pSrc, pDst, pSrc->width, pSrc->height, pSrc->pBuf + pSrc->alphaOffset);
    ALOGD("watermark <--- \n");

    return MTRUE;
}


/*******************************************************************************
*
********************************************************************************/
MVOID
WaterMark::
extractAlpha(ExtractAlphaData* data)
{
    ALOGD("[extractAlpha] - E.");
    MINT widthPitch, heightPitch;
    widthPitch = ALIGN(data->pYuv->width, data->pYuv->widthAlign);
    heightPitch = ALIGN(data->pYuv->height, data->pYuv->heightAlign);
    extractAlphas(data->pData, data->pYuv->pBuf + data->pYuv->alphaOffset, widthPitch, heightPitch);
    ALOGD("[extractAlpha] - X.");
}//

MBOOL
WaterMark::
requestBufs()
{
    ALOGD("[requestBufs] - E.");
    MBOOL ret = MTRUE; 
    pDst = (YuvDataBlock*)malloc(sizeof(YuvDataBlock));
    memset(pDst, 0, sizeof(YuvDataBlock));

    ret = MTRUE;
    ALOGD("[requestBufs] - X.");
    return  ret;
}

MBOOL
WaterMark::
releaseBufs()
{
    ALOGD("[releaseBufs] - E.");
    if (pSrc) {
        free(pSrc->pBuf);
        free(pSrc);
        pSrc = NULL;
    }
    free(pDst);
    pDst = NULL;
    ALOGD("[releaseBufs] - X.");
    return MTRUE;
}


MBOOL
WaterMark::
ImgProcess(NSCam::IImageBuffer* Srcbufinfo, MUINT32 srcWidth, MUINT32 srcHeight, NSCam::EImageFormat srctype, NSCam::IImageBuffer* Desbufinfo, MUINT32 desWidth, MUINT32 desHeight, NSCam::EImageFormat destype, MUINT32 transform)const
{
    //MY_LOGD("[Resize33] srcAdr 0x%x srcWidth %d srcHeight %d desAdr 0x%x desWidth %d desHeight %d ",(MUINT32)Srcbufinfo.virtAddr,srcWidth,srcHeight,(MUINT32)Desbufinfo.virtAddr,desWidth,desHeight);
    //MY_LOGD("FilterShot Imgprocess: srcfmt=%x , dstfmt=%x\n", srctype, destype);
    MBOOL ret = MTRUE;

    ret &= Srcbufinfo->syncCache(eCACHECTRL_FLUSH);

    NSCam::NSIoPipe::NSSImager::ISImager *mpISImager = NSCam::NSIoPipe::NSSImager::ISImager::createInstance(Srcbufinfo);
    if (mpISImager == NULL)
    {
        ALOGD("Null ISImager Obj \n");
        return MFALSE;
    }

    //
    ret &= mpISImager->setTargetImgBuffer(Desbufinfo);
    //
    ret &= mpISImager->setTransform(transform);
    //
    //mpISImager->setFlip(0);
    //
    //mpISImager->setResize(desWidth, desHeight);
    //
    ret &= mpISImager->setEncodeParam(1, 90);
    //
    //mpISImager->setROI(Rect(0, 0, srcWidth, srcHeight));
    //
    ret &= mpISImager->execute();

    ALOGD("[Resize] Out");
    return  MTRUE;
}


NSCam::IImageBuffer*
WaterMark::
allocMem(MUINT32 fmt, MUINT32 w, MUINT32 h)
{
    return BufAllocUtil::getInstance().allocMem(NAME, fmt, w, h);
}


MVOID
WaterMark::
deallocMem(NSCam::IImageBuffer *pBuf)
{
    BufAllocUtil::getInstance().deAllocMem(NAME, pBuf);
}
//lwl watermark end