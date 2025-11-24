#pragma once

#include "lwip/pbuf.h"

class PbufGuard {
public:
    explicit PbufGuard(pbuf* p)
        : p_(p)
    {}

    // No copy
    PbufGuard(const PbufGuard&) = delete;
    PbufGuard& operator=(const PbufGuard&) = delete;

    // No move
    PbufGuard(PbufGuard&&) = delete;
    PbufGuard& operator=(PbufGuard&&) = delete;

    ~PbufGuard() {
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
