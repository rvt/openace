#pragma once

#include "models.hpp"
#include "spinlockguard.hpp"

namespace GATAS
{
    /**
     * Current ownship samples, with independent atomic snapshot reads and writes.
     * Initialize once during startup before sharing this object with modules.
     * Both values use the supplied lock; no hardware spinlock is allocated here.
     * Separate loads do not form a single atomic snapshot of both samples.
     */
    class OwnshipState
    {
    public:
        static OwnshipState &shared()
        {
            static OwnshipState state;
            return state;
        }

        SynchronizedValue<OwnshipPositionInfo> location;
        SynchronizedValue<BarometricPressure> barometricPressure;

        void init(spin_lock_t *lock)
        {
            location.init(lock);
            barometricPressure.init(lock);
        }
    };
}
