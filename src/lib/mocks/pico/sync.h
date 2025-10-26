#pragma once

struct spin_lock_t {

};

inline spin_lock_t* spin_lock_instance(int num) {
    static spin_lock_t t;
    return &t;
}

inline int spin_lock_claim_unused(int num) {
    return 1;
}

inline int spin_lock_blocking(spin_lock_t* spinlock) {
    (void)spinlock;
    return 1;
}

inline void spin_unlock(spin_lock_t* spinlock, int num) {
    (void)spinlock;
    (void)num;
}