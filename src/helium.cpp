#include <uv.h>

#include <iostream>
#include <memory>

using std::unique_ptr;
using std::make_unique;


class uv_loop {
private:
    ::uv_loop_t loop;

public:
    uv_loop(void) {
        static_cast<void>(::uv_loop_init(&loop));
    }
    uv_loop(const uv_loop &) = delete;
    uv_loop& operator=(const uv_loop &) = delete;
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


int main(int argc, char *argv[]) {
    unique_ptr<uv_loop> loop = make_unique<uv_loop>();

    loop->run(UV_RUN_DEFAULT);

    return 0;
}