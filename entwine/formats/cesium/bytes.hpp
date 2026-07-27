/******************************************************************************
* Copyright (c) 2026, Third Space Interactive
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

namespace entwine
{
namespace cesium
{

// Both tile formats are little-endian binary containers built in memory.

inline void append(std::vector<char>& to, const uint32_t v)
{
    const char* p(reinterpret_cast<const char*>(&v));
    to.insert(to.end(), p, p + sizeof(v));
}

template <typename T>
inline void append(std::vector<char>& to, const std::vector<T>& from)
{
    to.insert(
        to.end(),
        reinterpret_cast<const char*>(from.data()),
        reinterpret_cast<const char*>(from.data() + from.size()));
}

} // namespace cesium
} // namespace entwine
