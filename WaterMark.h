#ifndef WATERMARK_H_
#define WATERMARK_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/middleware/v1/camshot/AlgoGomeWaterMark.h>//lwl add

#define WATER_MARK_PATH_PREFIX "/system/media/%s.png"
//////typedef unsigned char CHAR;

class WaterMark
{
public:
	void watermark(NSCam::IImageBuffer *pImgBuf, int yuvType, MUINT32 w, MUINT32 h);
	void watermark_YUV(NSCam::IImageBuffer *pImgBuf) ;
            
    MBOOL extractPNG();
    MVOID extractAlpha(ExtractAlphaData* data);
    MBOOL WaterMarkProcess(MUINT8* Srcbuf, MUINT32 w, MUINT32 h);
    MBOOL releaseBufs();
    MBOOL requestBufs();
    NSCam::IImageBuffer* allocMem(MUINT32 fmt, MUINT32 w, MUINT32 h);
    MVOID   deallocMem(NSCam::IImageBuffer *pBuf);
    MBOOL   ImgProcess(NSCam::IImageBuffer* Srcbufinfo, MUINT32 srcWidth, MUINT32 srcHeight, NSCam::EImageFormat srctype, NSCam::IImageBuffer* Desbufinfo, MUINT32 desWidth, MUINT32 desHeight, NSCam::EImageFormat destype, MUINT32 transform = 0)const;  
            
    MUINT32                                             mJpgWidth;
    MUINT32                                             mJpgHeight;
    MUINT32                                             mThumWidth;
    MUINT32                                             mThumHeight;
    MUINT32                                             formatType;
    YuvDataBlock*                                       pSrc;
    YuvDataBlock*                                       pDst;
    
    NSCam::IImageBuffer*                                mpRGBSource;
    NSCam::IImageBuffer*                                mpSource;
    NSCam::IImageBuffer*                                mpThumRGBSource;
    NSCam::IImageBuffer*                                mpThumSource;
};
#endif  // WATERMARK_H_
