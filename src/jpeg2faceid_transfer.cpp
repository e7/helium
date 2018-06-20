//
// Created by lijia on 18-5-8.
//

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <arcsoft_fsdk_face_detection.h>
#include <arcsoft_fsdk_face_recognition.h>
#include <sys/stat.h>
#include "jpeg2faceid_transfer.h"

using helium::intu_array;

#define FFMPEG_IMG      "/tmp/ffmpeg.img"
#define FFMPEG_YUV      "/tmp/ffmpeg.yuv"


bool helium::jpeg2faceid_transfer::init() {
    ASSERT(!inited);
    handle = ::tjInitDecompress();
    return (nullptr != handle);
}


helium::jpeg2faceid_transfer::~jpeg2faceid_transfer() {
    ::tjDestroy(handle);
}


int helium::jpeg2faceid_transfer::image2yuv(
        uint8_t **yuv_buf, size_t *yuv_size,
        int *yuv_type, int *width, int *height
) {
    auto write_full = [](int fd, uint8_t const *buf, size_t len) -> ssize_t {
        int sent = 0;
        while (sent < len) {
            auto n = ::write(fd, buf+sent, len-sent);
            if (-1 == n) {
                fprintf(stderr, "[ERROR] write failed:%d\n", errno);
                return -1;
            }
            sent += n;
        }

        return sent;
    };

    auto read_full = [](int fd, uint8_t *buf, size_t len) -> int {
        int read = 0;

        while (true) {
            auto n = ::read(fd, buf+read, len-read);
            if (-1 == n) {
                fprintf(stderr, "[ERROR] read failed:%d\n", errno);
                return -1;
            }
            if (0 == n) {
                return read;
            }
            read += n;
        }
    };

    // 解压图片，解析出分辨率
//    int subsample, colorspace;
//    if (-1 == ::tjDecompressHeader3(
//            handle, buf, len, width, height, &subsample, &colorspace)) {
//        fprintf(stderr, "decompress image failed:%s\n", ::tjGetErrorStr());
//        return -1;
//    }
    *width = 750;
    *height = 500;

    // 保存image
    int write_fd = ::open(FFMPEG_IMG, O_WRONLY | O_CLOEXEC);
    if (-1 == write_fd) {
        fprintf(stderr, "[ERROR] open file %s failed\n", FFMPEG_IMG);
        return -1;
    }
    if (-1 == write_full(write_fd, this->buf, this->len)) {
        static_cast<void>(::close(write_fd));
        return -1;
    }
    fprintf(stderr, "[INFO] write image file size:%lu\n", this->len);
    static_cast<void>(::close(write_fd));

    // 调用ffmpeg
    auto pid = fork();
    if (-1 == pid) {
        fprintf(stderr, "fork() failed:%d\n", errno);
        return -1;
    }

    if (0 == pid) {
        // child process
        if (execl("/usr/bin/ffmpeg",
                  "ffmpeg", "-y", "-i", FFMPEG_IMG, "-s", "750x500",
                  "-pix_fmt", "yuv420p", FFMPEG_YUV, nullptr)) {
            ::fprintf(stderr, "[ERROR] failed to exec ffmpeg:%d\n", errno);
            ::exit(1);
        }
    }
    ::waitpid(pid, NULL, 0);

    // 读取yuv
    struct stat st;
    int yuv_fd = ::open(FFMPEG_YUV, O_RDONLY);
    if (-1 == yuv_fd) { return -1; }
    if (-1 == ::fstat(yuv_fd, &st)) { return -1; }
    *yuv_buf = new uint8_t[st.st_size];
    *yuv_size = static_cast<size_t>(st.st_size);

    auto nread = read_full(yuv_fd, *yuv_buf, *yuv_size);
    static_cast<void>(::close(yuv_fd));
    return (nread > 0) ? 0 : -1;
}


int helium::jpeg2faceid_transfer::tjpeg2yuv(
        uint8_t **yuv_buf, size_t *yuv_size,
        int *yuv_type, int *width, int *height
) {
    int subsample, colorspace;
    int flags = 0;
    int padding = 1; // 1或4均可，但不能是0

    if (-1 == ::tjDecompressHeader3(
            handle, buf, len, width, height, &subsample, &colorspace)) {
        return -1;
    }

    fprintf(stderr, "[debug] image file size:%lu, %dx%d\n", len, *width, *height);

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

    fprintf(stderr, "image resolution width:%d, height:%d\n", width, height);
    auto r = ::AFD_FSDK_StillImageFaceDetection(hdetection, &input_img, &rslt);
    if (MOK == r) {
        fprintf(stderr, "face detected, (%d,%d,%d,%d)\n",
                rslt->rcFace[0].left, rslt->rcFace[0].top,
                rslt->rcFace[0].right, rslt->rcFace[0].bottom);
        return rslt;
    } else {
        fprintf(stderr, "failed to detection face:0x%lx\n", r);
        return nullptr;
    }
}


intu_array&& helium::jpeg2faceid_transfer::genFaceId() {
    uint8_t *yuv_buf;
    size_t yuv_size;
    int yuv_type, width, height;
    auto hrecognition = afr_fsdk_engine::get_instance()->get_engine();

    // jpeg转换为yuv
    if (-1 == image2yuv(&yuv_buf, &yuv_size, &yuv_type, &width, &height)) {
        return std::move(intu_array());
    }
    unique_ptr<uint8_t[]> free_yuv_buf(yuv_buf);
    fprintf(stderr, "yuv size:%lu\n", yuv_size);

#if 0
    FILE *f = fopen("last.yuv", "wb");
    fwrite(yuv_buf, yuv_size, 1, f);
    fclose(f);
#endif

    // 人脸检测
    auto facers = face_detection(yuv_buf, width, height);
    if (!facers) {
        fprintf(stderr, "no face found\n");
        return std::move(intu_array());
    }

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