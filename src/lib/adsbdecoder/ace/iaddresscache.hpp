#pragma once

#include "ace/coreutils.hpp"
#include "ace/constants.hpp"
#include "ace/models.hpp"

#include "etl/flat_set.h"

class IAddressCache
{

    virtual bool insert(uint32_t address, uint32_t usTime) = 0;

};
