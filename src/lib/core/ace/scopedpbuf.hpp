#pragma once

#include "lwip/pbuf.h"

/**
 * RAII wrapper for LwIP pbufs. It will automatically free the pbuf when it goes out of scope. 
 * It also provides some helper functions to access the pbuf data.
 */
class ScopedPbuf {
public:
    explicit ScopedPbuf(pbuf* p)
        : p_(p)
    {}

    ScopedPbuf(const ScopedPbuf&) = delete;
    ScopedPbuf& operator=(const ScopedPbuf&) = delete;
    ScopedPbuf(ScopedPbuf&&) = delete;
    ScopedPbuf& operator=(ScopedPbuf&&) = delete;

    ~ScopedPbuf() {
        if (p_) {
            pbuf_free(p_);
            p_ = nullptr;
        }
    }

    pbuf* get() const { return p_; }
    pbuf* operator->() const { return p_; }
    operator pbuf*() const { return p_; }

private:
    pbuf* p_ = nullptr;
};
