// Copyright 2023 Lan Zongwei. All rights reserved.
// Author: Zongwei Lan (lanzongwei541@gmail.com)

#pragma once

#include "experimental/__p1386/audio_buffer.h"
#include "experimental/__p1386/audio_device.h"
#include "experimental/__p1386/config.h"

#include "experimental/__p1386/concepts.h"

_LIBSTDAUDIO_NAMESPACE_BEGIN

enum class audio_device_list_event {
  device_list_changed,
  default_input_device_changed,
  default_output_device_changed,
};

template <AudioDeviceListCallback F>
void set_audio_device_list_callback(audio_device_list_event, F &&cb);

_LIBSTDAUDIO_NAMESPACE_END
