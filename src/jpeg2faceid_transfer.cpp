//
// Created by lijia on 18-5-8.
//

#include <cstring>
#include <arcsoft_fsdk_face_detection.h>
#include <arcsoft_fsdk_face_recognition.h>
#include "jpeg2faceid_transfer.h"

using helium::intu_array;


bool helium::jpeg2faceid_transfer::init() {
    ASSERT(!inited);
    handle = ::tjInitDecompress();
    return (nullptr != handle);
}


helium::jpeg2faceid_transfer::~jpeg2faceid_transfer() {
    ::tjDestroy(handle);
}


int helium::jpeg2faceid_transfer::tjpeg2yuv(
        uint8_t **yuv_buf, size_t *yuv_size,
        int *yuv_type, int *width, int *height
) {
    int subsample, colorspace;
    int flags = 0;
    int padding = 1; // 1或4均可，但不能是0
    int ret = 0;

    if (-1 == ::tjDecompressHeader3(
            handle, buf, len, width, height, &subsample, &colorspace)) {
        return -1;
    }
    *yuv_type = subsample;
    *yuv_size = ::tjBufSizeYUV2(*width, padding, *height, subsample);
    if (-1 == *yuv_size) {
        return -1;
    }
    *yuv_buf = new (std::nothrow) uint8_t[*yuv_size];

    if (nullptr == *yuv_buf) {
        fprintf(stderr, "[FATAL] !! out of memory, require size %lu !!\n", *yuv_size);
        ::abort();
    }

    return ::tjDecompressToYUV2(
            handle, buf, len, *yuv_buf, *width, padding, *height, flags
    );
}


LPAFD_FSDK_FACERES helium::jpeg2faceid_transfer::face_detection(
        uint8_t *yuv_buf,int width, int height
) {
    LPAFD_FSDK_FACERES rslt = nullptr;
    ASVLOFFSCREEN input_img = {0};
    auto hdetection = afd_fsdk_engine::get_instance()->get_handle();

    input_img.u32PixelArrayFormat = ASVL_PAF_I420;
    input_img.i32Width = width;
    input_img.i32Height = height;
    input_img.ppu8Plane[0] = yuv_buf;
    do {
        input_img.pi32Pitch[0] = input_img.i32Width;
        input_img.pi32Pitch[1] = input_img.i32Width/2;
        input_img.pi32Pitch[2] = input_img.i32Width/2;
        input_img.ppu8Plane[1] = input_img.ppu8Plane[0] + input_img.pi32Pitch[0] * input_img.i32Height;
        input_img.ppu8Plane[2] = input_img.ppu8Plane[1] + input_img.pi32Pitch[1] * input_img.i32Height/2;
    } while (0);

    auto r = ::AFD_FSDK_StillImageFaceDetection(hdetection, &input_img, &rslt);
    if (MOK != r) {
        fprintf(stderr, "failed to detection face:0x%lx\n", r);
        return nullptr;
    }

    return rslt;
}


intu_array&& helium::jpeg2faceid_transfer::genFaceId() {
    uint8_t *yuv_buf;
    size_t yuv_size;
    int yuv_type, width, height;
    auto hrecognition = afr_fsdk_engine::get_instance()->get_engine();

    // jpeg转换为yuv
    if (-1 == tjpeg2yuv(&yuv_buf, &yuv_size, &yuv_type, &width, &height)) {
        fprintf(stderr, "translate jpeg to yuv failed:%s\n", ::tjGetErrorStr());
        return std::move(intu_array());
    }
    auto free_yuv_buf = std::make_unique<uint8_t[]> (*yuv_buf);

    // 人脸检测
    auto facers = face_detection(yuv_buf, width, height);
    if (!facers) {
        fprintf(stderr, "no face found\n");
        return std::move(intu_array());
    }
    fprintf(stderr, "face found height=%d, width=%d\n", height, width);

    // 人脸特征生成
    ASVLOFFSCREEN input_img = {0};
    input_img.u32PixelArrayFormat = ASVL_PAF_I420;
    input_img.i32Width = width;
    input_img.i32Height = height;
    input_img.ppu8Plane[0] = yuv_buf;
    input_img.pi32Pitch[0] = input_img.i32Width;
    input_img.pi32Pitch[1] = input_img.i32Width/2;
    input_img.pi32Pitch[2] = input_img.i32Width/2;
    input_img.ppu8Plane[1] = input_img.ppu8Plane[0] + input_img.pi32Pitch[0] * input_img.i32Height;
    input_img.ppu8Plane[2] = input_img.ppu8Plane[1] + input_img.pi32Pitch[1] * input_img.i32Height/2;

    AFR_FSDK_FACEMODEL facemodel = {0};
    AFR_FSDK_FACEINPUT faceinput = {
            // 只识别第一张
            {
                    facers->rcFace[0].left, facers->rcFace[0].top,
                    facers->rcFace[0].right, facers->rcFace[0].bottom
            },
            AFR_FSDK_FOC_0,
    };
    auto r = ::AFR_FSDK_ExtractFRFeature(hrecognition, &input_img, &faceinput, &facemodel);
    if (MOK != r) {
        fprintf(stderr, "generate feature failed:%lx\n", r);
        return std::move(intu_array());
    }

    intu_array facebuf(new (std::nothrow) uint8_t[facemodel.lFeatureSize], facemodel.lFeatureSize);
    ::memcpy(facebuf.data, facemodel.pbFeature, facemodel.lFeatureSize);

    return std::move(facebuf);
}