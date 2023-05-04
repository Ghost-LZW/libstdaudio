// Copyright 2023 Lan Zongwei. All rights reserved.
// Author: Zongwei Lan (lanzongwei541@gmail.com)

#pragma once

#include <type_traits>

#include "experimental/__p1386/audio_buffer.h"
#include "experimental/__p1386/audio_device.h"
#include "experimental/__p1386/config.h"

_LIBSTDAUDIO_NAMESPACE_BEGIN

template <typename T>
concept AudioDeviceListCallback = std::is_nothrow_invocable_v<T>;

template <typename T>
concept AudioDeviceCallback = std::is_nothrow_invocable_v<T, audio_device &>;

template <typename T, typename SampleType>
concept AudioIOCallback =
    std::is_nothrow_invocable_v<T, audio_device &,
                                audio_device_io<SampleType> &>;

_LIBSTDAUDIO_NAMESPACE_END
