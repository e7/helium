#include <uv.h>

#include <iostream>
#include <memory>
#include <cassert>
#include <cstring>
#include <arcsoft_fsdk_face_recognition.h>

#include "arcsoft_fsdk_face_recognition.h"
#include "merror.h"

using std::unique_ptr;
using std::make_unique;


class afr_fsdk_engine {
public:
    afr_fsdk_engine& operator=(const afr_fsdk_engine&) = delete;
    static afr_fsdk_engine *get_instance() {
        static afr_fsdk_engine inst;

        if (! inst.initialized) {
            AFR_FSDK_InitialEngine(
                    "CyQcgkFu1zhQXaoqS5guq6piAkKrmG2eS2xnKLUaN3m8",
                    "FrDSjJDyiCc6tBcug1uUzY5rUCs8AMck1XLXeh5umRfG",
                    inst.work_mem_ptr.get(), WORKBUF_SIZE, &inst.engine
            );
            inst.initialized = true;
        }
        return &inst;
    }
    ~afr_fsdk_engine() {
        ::AFR_FSDK_UninitialEngine(&engine);
    }

    MHandle *get_engine() {
        return &engine;
    }

private:
    afr_fsdk_engine() {}
    afr_fsdk_engine(const afr_fsdk_engine&) {}

    static int const WORKBUF_SIZE = 40*1024*1024;

    std::unique_ptr<MByte[]> work_mem_ptr = std::unique_ptr<MByte[]>(new MByte[WORKBUF_SIZE]);

    bool initialized = false;
    MHandle engine;
};


namespace helium {
    class uv_loop;

    void on_alloc_buffer(::uv_handle_t *handler, size_t suggested_size, ::uv_buf_t *buf);
    void on_read(::uv_stream_t *cli, ssize_t nread, const ::uv_buf_t *buf);
    void on_write(uv_write_t* req, int status);
    void on_new_connection(::uv_stream_t *server, int status);
    int helium_main(int argc, char *argv[]);
}

class helium::uv_loop {
private:
    ::uv_loop_t loop;

public:
    uv_loop(void) {
        static_cast<void>(::uv_loop_init(&loop));
    }

    uv_loop(const uv_loop &) = delete;

    uv_loop &operator=(const uv_loop &) = delete;

    ~uv_loop(void) {
        static_cast<void>(::uv_loop_close(&loop));
    }

    /**
     * 获取裸的loop变量，不应被外部释放
     */
    ::uv_loop_t *get_naked_loop(void) {
        return &loop;
    }

    int run(uv_run_mode mode) {
        return ::uv_run(&loop, mode);
    }
};


unique_ptr<helium::uv_loop> loop;
// 读缓冲大小
static int const SZ_RBUF = 16 * 1024 * 1024;


void helium::on_alloc_buffer(::uv_handle_t *handler, size_t suggested_size, ::uv_buf_t *buf) {
    buf->base = new char[SZ_RBUF];
    buf->len = SZ_RBUF;
}


void helium::on_read(::uv_stream_t *cli, ssize_t nread, const ::uv_buf_t *uv_rbuf) {
    std::unique_ptr<char[]> buf_ptr(uv_rbuf->base);

    if (nread < 0) {
        // UV_EOF or UV_ECONNRESET

        // 关闭连接
        ::uv_close(reinterpret_cast<::uv_handle_t *>(cli), nullptr);
        delete(cli);
        return;
    }

    if (nread == uv_rbuf->len) {
        // too large entity
        return;
    }

    // -------------------- 执行业务 -----------------------------
    ::ASVLOFFSCREEN img;
    img.u32PixelArrayFormat = ASVL_PAF_I420;
    img.i32Width = 640;
    img.i32Height = 480;
    img.ppu8Plane[0] = reinterpret_cast<MUInt8 *>(uv_rbuf->base);
    img.pi32Pitch[0] = img.i32Width;
    img.pi32Pitch[1] = img.i32Width / 2;
    img.pi32Pitch[2] = img.i32Width / 2;
    img.ppu8Plane[1] = img.ppu8Plane[0] + img.pi32Pitch[0] * img.i32Height;
    img.ppu8Plane[2] = img.ppu8Plane[1] + img.pi32Pitch[1] * img.i32Height / 2;

    ::AFR_FSDK_FACEMODEL face_model = {0};
    do {
        ::AFR_FSDK_FACEINPUT face_rslt;
        face_rslt.lOrient = AFR_FSDK_FOC_0;
        face_rslt.rcFace.left = 282;
        face_rslt.rcFace.top = 58;
        face_rslt.rcFace.right = 422;
        face_rslt.rcFace.bottom = 198;
        auto ret = AFR_FSDK_ExtractFRFeature(
                afr_fsdk_engine::get_instance()->get_engine(), &img, &face_rslt, &face_model
        );
        if (0 != ret) {
            fprintf(stderr, "extract fr feature failed\n");
            return;
        }
    } while (0);

    ::uv_write_t *req = new uv_write_t();
    ::uv_buf_t uv_sbuf = ::uv_buf_init(
            reinterpret_cast<char *>(face_model.pbFeature), face_model.lFeatureSize
    );
    ::uv_write(req, cli, &uv_sbuf, 1, helium::on_write);
}


void helium::on_write(uv_write_t* req, int status) {
    std::unique_ptr<uv_write_t> req_ptr(req);

    if (status) {
        return;
    }

    return;
}


void helium::on_new_connection(::uv_stream_t *server, int status) {
    ::uv_tcp_t *_cli = nullptr;
    ::uv_stream_t **cli = reinterpret_cast<::uv_stream_t **>(&_cli);

    if (status < 0) {
        return;
    }

    _cli = new ::uv_tcp_t();
    ::uv_tcp_init(loop->get_naked_loop(), _cli);

    if (::uv_accept(server, *cli) != 0) {
        delete(cli);
        return;
    }

    ::uv_read_start(*cli, &helium::on_alloc_buffer, &helium::on_read);
}


int helium::helium_main(int argc, char *argv[]) {
    int err = 0;
    ::sockaddr_in6 addr = {};
    ::uv_tcp_t server = {};
    loop = make_unique<uv_loop>();

    ::uv_tcp_init(loop->get_naked_loop(), &server);
    ::uv_ip6_addr("::", 8008, &addr);
    ::uv_tcp_bind(&server, reinterpret_cast<const ::sockaddr *>(&addr), 0);

    err = ::uv_listen(reinterpret_cast<::uv_stream_t *>(&server), 1, helium::on_new_connection);
    if (err != 0) {
        // listen failed
        fprintf(stderr, "%s\n", ::uv_strerror(err));
        return EXIT_FAILURE;
    }

    return loop->run(UV_RUN_DEFAULT);
}


int main(int argc, char *argv[]) {
    afr_fsdk_engine::get_instance(); // 初始化

    return helium::helium_main(argc, argv);
}