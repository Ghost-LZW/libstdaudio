// Copyright 2022 Zongwei Lan. All rights reserved.
// Author: Zongwei Lan (lanzongwei541@gmail.com)

#pragma once

#if !defined(AUDIO_PROPOSED_NAMESPACE)
#define AUDIO_PROPOSED_NAMESPACE std::experimental
#endif

#define _LIBSTDAUDIO_NAMESPACE AUDIO_PROPOSED_NAMESPACE

#define _LIBSTDAUDIO_NAMESPACE_BEGIN namespace _LIBSTDAUDIO_NAMESPACE {
#define _LIBSTDAUDIO_NAMESPACE_END }
