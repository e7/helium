//
// Created by lijia on 18-5-8.
//

#ifndef HELIUM_JPEG2FACEID_TRANSFER_HPP
#define HELIUM_JPEG2FACEID_TRANSFER_HPP

#include <string>
#include <cstdio>
#include <memory>
#include <turbojpeg.h>
#include <arcsoft_fsdk_face_detection.h>
#include <arcsoft_fsdk_face_recognition.h>
#include <merror.h>

#define ARC_SOFT_APPID              "CyQcgkFu1zhQXaoqS5guq6piAkKrmG2eS2xnKLUaN3m8"
#define ARC_SOFT_DETECTION_KEY      "FrDSjJDyiCc6tBcug1uUzY5MpbpUjcbpkZzvj2FAm14R"
#define ARC_SOFT_RECOGNITION_KEY    "FrDSjJDyiCc6tBcug1uUzY5rUCs8AMck1XLXeh5umRfG"
#define ARC_SOFT_WORKBUF_SIZE       (40*1024*1024)
#define ARC_SOFT_MAX_FACE_NUM       50

using std::string;
using std::unique_ptr;


namespace helium {
    class afd_fsdk_engine final {
    public:
        afd_fsdk_engine& operator=(afd_fsdk_engine const&) = delete;

        // 获取实例
        static afd_fsdk_engine *get_instance() {
            static afd_fsdk_engine inst;

            if (! inst.initialized) {
                if (MOK != ::AFD_FSDK_InitialFaceEngine(
                        const_cast<char *>(inst.appid), const_cast<char *>(inst.sdk_key),
                        inst.work_mem_ptr.get(), ARC_SOFT_WORKBUF_SIZE, &inst.engine,
                        AFD_FSDK_OPF_0_HIGHER_EXT, 16, ARC_SOFT_MAX_FACE_NUM
                )) {::abort();}
                inst.initialized = true;
            }

            return &inst;
        }

        ~afd_fsdk_engine() {
            ::AFD_FSDK_UninitialFaceEngine(engine);
        }

        // 获取句柄
        MHandle get_handle() {
            return engine;
        }

    private:
        afd_fsdk_engine() {}
        afd_fsdk_engine(afd_fsdk_engine const&) = delete;

        std::unique_ptr<MByte[]> work_mem_ptr = std::unique_ptr<MByte[]>(
                new MByte[ARC_SOFT_WORKBUF_SIZE]
        );

        bool initialized = false;
        char const *appid = ARC_SOFT_APPID;
        char const *sdk_key = ARC_SOFT_DETECTION_KEY;
        MHandle engine;
    };


    class afr_fsdk_engine final {
    public:
        afr_fsdk_engine& operator=(afr_fsdk_engine const&) = delete;

        // 获取实例
        static afr_fsdk_engine *get_instance() {
            static afr_fsdk_engine inst;

            if (! inst.initialized) {
                if(MOK != ::AFR_FSDK_InitialEngine(
                        const_cast<char *>(inst.appid), const_cast<char *>(inst.sdk_key),
                        inst.work_mem, ARC_SOFT_WORKBUF_SIZE, &inst.engine
                )) {::abort();}
                inst.initialized = true;
            }

            return &inst;
        }
        ~afr_fsdk_engine() {
            ::AFR_FSDK_UninitialEngine(&engine);
        }

        // 获取句柄
        MHandle get_engine() {
            return engine;
        }

    private:
        afr_fsdk_engine() {}
        afr_fsdk_engine(afr_fsdk_engine const&) = delete;

//        std::unique_ptr<MByte[]> work_mem_ptr = std::unique_ptr<MByte[]>(
//                new MByte[ARC_SOFT_WORKBUF_SIZE]
//        );
        MByte *work_mem = new MByte[ARC_SOFT_WORKBUF_SIZE];

        bool initialized = false;
        char const *appid = ARC_SOFT_APPID;
        char const *sdk_key = ARC_SOFT_RECOGNITION_KEY;
        MHandle engine;
    };

    class jpeg2faceid_transfer final {
    public:
        jpeg2faceid_transfer(uint8_t const *jpeg_buf, ssize_t buf_len)
            :buf(jpeg_buf), len(buf_len) {}
        jpeg2faceid_transfer(jpeg2faceid_transfer const&) = delete;
        ~jpeg2faceid_transfer();

        bool init();
        unique_ptr<uint8_t[]> genFaceId();

    private:
        // jpeg转yuv
        int tjpeg2yuv(uint8_t **yuv_buf, size_t *yuv_size,
                      int *yuv_type, int *width, int *height);
        // 人脸检测
        LPAFD_FSDK_FACERES face_detection(uint8_t *yuv_buf, int width, int height);

    private:
        ::tjhandle handle = nullptr;
        uint8_t const *buf = nullptr;
        size_t len = 0;
    };
}

#endif //HELIUM_JPEG2FACEID_TRANSFER_HPP