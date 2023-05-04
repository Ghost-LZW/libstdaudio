// libstdaudio
// Copyright (c) 2018 - Timur Doumler
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at
// http://boost.org/LICENSE_1_0.txt)

#pragma once

#include <array>
#include <cassert>
#include <chrono>
#include <optional>
#include <type_traits>
#include <variant>

#if __cpp_lib_mdspan >= 202207L
#include <mdspan>
#else
#include <experimental/mdspan>
#endif

#include "experimental/__p1386/config.h"

_LIBSTDAUDIO_NAMESPACE_BEGIN

struct contiguous_interleaved_t {};
inline constexpr contiguous_interleaved_t contiguous_interleaved;

struct contiguous_deinterleaved_t {};
inline constexpr contiguous_deinterleaved_t contiguous_deinterleaved;

struct ptr_to_ptr_deinterleaved_t {};
inline constexpr ptr_to_ptr_deinterleaved_t ptr_to_ptr_deinterleaved;

template <typename SampleType> class audio_buffer {
public:
  using sample_type = SampleType;
  using index_type = size_t;
  using contiguous_view_type =
      mdspan<sample_type, dextents<index_type, 2>, layout_stride>;
  using distant_view_type = std::vector<std::span<sample_type>>;

  audio_buffer(sample_type *data, index_type num_frames,
               index_type num_channels, contiguous_interleaved_t)
      : _num_frames(num_frames), _num_channels(num_channels),
        _is_contiguous(true) {
    _data_view = mdspan(
        data, typename contiguous_view_type::mapping_type{
                  dextents<index_type, 2>{num_channels, num_frames},
                  std::array<index_type, 2>{1, num_channels}}); // layout_left
  }

  audio_buffer(sample_type *data, index_type num_frames,
               index_type num_channels, contiguous_deinterleaved_t)
      : _num_frames(num_frames), _num_channels(num_channels),
        _is_contiguous(true) {
    _data_view = mdspan(
        data, typename contiguous_view_type::mapping_type{
                  dextents<index_type, 2>{num_channels, num_frames},
                  std::array<index_type, 2>{num_frames, 1}}); // layout_right
  }

  audio_buffer(sample_type **data, index_type num_frames,
               index_type num_channels, ptr_to_ptr_deinterleaved_t)
      : _num_frames(num_frames), _num_channels(num_channels),
        _is_contiguous(false) {
    distant_view_type view;
    view.reserve(_num_channels);
    for (int i = 0; i < _num_channels; i++) {
      view.emplace_back(data[i], num_frames);
    }
    _data_view = view;
  }

  sample_type *data() const noexcept {
    return _is_contiguous ? get<contiguous_view_type>(_data_view).data_handle()
                          : nullptr;
  }

  bool is_contiguous() const noexcept { return _is_contiguous; }

  bool frames_are_contiguous() const noexcept {
    return is_contiguous()
               ? get<contiguous_view_type>(_data_view).stride(0) == 1
               : false;
  }

  bool channels_are_contiguous() const noexcept {
    return is_contiguous()
               ? get<contiguous_view_type>(_data_view).stride(1) == 1
               : false;
  }

  index_type size_frames() const noexcept { return _num_frames; }

  index_type size_channels() const noexcept { return _num_channels; }

  index_type size_samples() const noexcept {
    return _num_channels * _num_frames;
  }

  // TODO: enable this only if AUDIO_USE_PAREN_OPERATOR defined.
  sample_type &operator()(index_type channel, index_type frame) noexcept {
    return const_cast<sample_type &>(
        as_const(*this).operator()(channel, frame));
  }

  const sample_type &operator()(index_type channel,
                                index_type frame) const noexcept {
    return std::visit(
        [&channel, &frame](auto &&v) -> const sample_type & {
          if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                       contiguous_view_type>) {
#if __cpp_multidimensional_subscript >= 202110L
            return v[channel, frame];
#else
            return v(channel, frame);
#endif
          } else {
            return v[channel][frame];
          }
        },
        _data_view);
  }

#if __cpp_multidimensional_subscript >= 202110L
  sample_type &operator[](index_type channel, index_type frame) noexcept {
    return const_cast<sample_type &>(
        as_const(*this).operator()(channel, frame));
  }

  const sample_type &operator[](index_type channel,
                                index_type frame) const noexcept {
    return std::visit(
        [&channel, &frame](auto &&v) -> const sample_type & {
          if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                       contiguous_view_type>) {
            return v[channel, frame];
          } else {
            return v[channel][frame];
          }
        },
        _data_view);
  }
#endif

private:
  bool _is_contiguous = false;
  index_type _num_frames = 0;
  index_type _num_channels = 0;

  std::variant<contiguous_view_type, distant_view_type> _data_view;
};

// TODO: this is currently macOS specific!
using audio_clock_t = chrono::steady_clock;

template <typename SampleType> struct audio_device_io {
  std::optional<audio_buffer<SampleType>> input_buffer;
  std::optional<chrono::time_point<audio_clock_t>> input_time;
  std::optional<audio_buffer<SampleType>> output_buffer;
  std::optional<chrono::time_point<audio_clock_t>> output_time;
};

_LIBSTDAUDIO_NAMESPACE_END
