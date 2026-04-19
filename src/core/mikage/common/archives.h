#pragma once

#include "serialization/boost_all_serialization.h"

using iarchive = MikageSerialization::input_archive;
using oarchive = MikageSerialization::output_archive;

#define SERIALIZE_IMPL(A)                                                                          \
    template void A::serialize<iarchive>(iarchive&, const unsigned int);                          \
    template void A::serialize<oarchive>(oarchive&, const unsigned int);

#define SERIALIZE_EXPORT_IMPL(A)

#define DEBUG_SERIALIZATION_POINT                                                                  \
    do {                                                                                           \
    } while (0)
