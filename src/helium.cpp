#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <memory>
#include <cstring>
#include <thread>

#include "readerwriterqueue.h"
#include "jpeg2faceid_transfer.h"


using std::unique_ptr;
using std::make_unique;
using std::thread;

// 单生产者，单消费者无锁队列
// A fast **single-producer, single-consumer** lock-free queue for C++
using moodycamel::BlockingReaderWriterQueue;

// 读缓冲大小
static int const SZ_RBUF = 16 * 1024 * 1024;

// fd队列
static BlockingReaderWriterQueue<int> fd_queue(4);

namespace helium {
    void worker_proc() {
        int elmt;
        uint32_t expLen;
        uint8_t *buf = nullptr;
        unique_ptr<uint8_t[], void(*)(uint8_t*)> buf_ptr(
                new uint8_t[SZ_RBUF], [](uint8_t* p){delete[](p);}
        );

        buf = buf_ptr.get();
        fprintf(stderr, "start working...\n");
        while (true) {
            fd_queue.wait_dequeue(elmt);

            if (-1 == elmt) {
                break;
            }

            // 获取应收长度
            if (::recv(elmt, &expLen, sizeof(expLen), 0) < sizeof(expLen)) {
                static_cast<void>(::close(elmt));
                continue;
            }
            expLen = ::ntohl(expLen);

            // 接收内容
            uint32_t nrecv = 0; // 已收长度
            while (nrecv < expLen) {
                auto n = ::recv(elmt, buf+nrecv, SZ_RBUF-nrecv, 0);
                if (n < 1) {
                    static_cast<void>(::close(elmt));
                    break;
                }
                nrecv += static_cast<uint32_t >(n);
            }
            if (nrecv < expLen) {
                // 未能接收完整
                continue;
            }

#if 0
            FILE *f = fopen("last.jpg", "wb");
            fwrite(buf, nrecv, 1, f);
            fclose(f);
#endif
            // 3字节文件类型
            char const *img_file = nullptr;
            if (0 == ::strncmp("jpg", reinterpret_cast<char const *>(buf), 3)) {
                img_file = FFMPEG_JPG;
            } else if (0 == ::strncmp("png", reinterpret_cast<char const *>(buf), 3)) {
                img_file = FFMPEG_PNG;
            } else {}

            // 4字节宽高
            union {char c[2];uint16_t v;} width, height;
            width.c[0] = buf[4]; width.c[1] = buf[3];
            height.c[0] = buf[6]; height.c[1] = buf[5];
            fprintf(stderr, "recved widht:%d, height:%d\n", width.v, height.v);

            // 获取人脸特征文件
            const static int IMAGE_BUF_OFFSET = 7;
            jpeg2faceid_transfer jft(buf+IMAGE_BUF_OFFSET, nrecv-IMAGE_BUF_OFFSET);
            if (! jft.init()) {
                static_cast<void>(::close(elmt));
                continue;
            }

            auto faceid = jft.genFaceId(img_file, width.v, height.v);
            if (faceid.len == 0) {
                // 未找到人脸
                static_cast<void>(::close(elmt));
                continue;
            }

            // 发送文件
            ::write(elmt, faceid.data, faceid.len);
            static_cast<void>(::shutdown(elmt, SHUT_WR));
            static_cast<void>(::close(elmt));
        }
    }
    int helium_main(int argc, char *argv[]);
}


int helium::helium_main(int argc, char *argv[]) {
    int lsn_fd, noptval;
    struct sockaddr_in serv_addr = {
            AF_INET, ::htons(8008), {::inet_addr("127.0.0.1")}, {0},
    };
    // auto ncpu = thread::hardware_concurrency();

    if ((lsn_fd = ::socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return EXIT_FAILURE;
    }
    ::setsockopt(lsn_fd, SOL_SOCKET, SO_REUSEADDR,
                 static_cast<const void *>(&noptval), sizeof(int));
    if (::bind(lsn_fd,
               reinterpret_cast<struct sockaddr *>(&serv_addr),
               sizeof(serv_addr)) == -1) {
        static_cast<void>(::close(lsn_fd));
        return EXIT_FAILURE;
    }
    if (::listen(lsn_fd, SOMAXCONN) == -1) {
        static_cast<void>(::close(lsn_fd));
        return EXIT_FAILURE;
    }

    thread worker(helium::worker_proc); // 工作者线程

    while (true) {
        int cli_fd = -1;
        struct sockaddr cli;
        socklen_t cli_len = 0; // 这里必须初始化，否则无法建立连接

        cli_fd = ::accept(lsn_fd, &cli, &cli_len);
        if (cli_fd == -1) {
            continue;
        }

        fd_queue.enqueue(cli_fd);
    }
}


int main(int argc, char *argv[]) {
    int rslt;

    // 初始化单例
    helium::afd_fsdk_engine::get_instance();
    helium::afr_fsdk_engine::get_instance();

    rslt = helium::helium_main(argc, argv);
    fprintf(stdout, "server stoped\n");

    return rslt;
}