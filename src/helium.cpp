#include <uv.h>

#include <iostream>
#include <memory>
#include <cassert>
#include <cstring>
#include "jpeg2faceid_transfer.hpp"


using std::unique_ptr;
using std::make_unique;


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

    fprintf(stderr, "recv size:%d\n", nread);

    ::uv_write_t *req = new uv_write_t();
    ::uv_buf_t uv_sbuf = ::uv_buf_init(nullptr, 0
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
    // 初始化单例
    helium::afd_fsdk_engine::get_instance();
    helium::afr_fsdk_engine::get_instance();

    return helium::helium_main(argc, argv);
}