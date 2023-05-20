// libstdaudio
// Copyright (c) 2018 - Timur Doumler
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at
// http://boost.org/LICENSE_1_0.txt)

#pragma once

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <forward_list>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include "SDL3/SDL_audio.h"

// Since std::move_only_function not suppored by clang
#include "SDL_events.h"
#include "SDL_hints.h"
#include "SDL_init.h"
#include "SDL_stdinc.h"
#include "experimental/audio_backend/FunctionExtras.h"

#include "experimental/__p1386/audio_buffer.h"
#include "experimental/__p1386/audio_device.h"
#include "experimental/__p1386/audio_event.h"
#include "experimental/__p1386/concepts.h"

_LIBSTDAUDIO_NAMESPACE_BEGIN

namespace detail {
template <class...> struct function_type;
template <class R, class... A> struct function_type<R (*)(A...)> {
  using type = R(A...);
};
} // namespace detail

class audio_device {
public:
  using device_id_t = SDL_AudioDeviceID;
  using sample_rate_t = int;
  using buffer_size_t = uint16_t;

  audio_device() = delete;
  audio_device(audio_device &) = delete;

  audio_device(audio_device &&) = default;

  ~audio_device() { stop(); }

  string_view name() const noexcept { return name_; }

  device_id_t device_id() const noexcept { return id_; }

  bool is_input() const noexcept { return iscapture_; }

  bool is_output() const noexcept { return !iscapture_; }

  int get_num_input_channels() const noexcept {
    return iscapture_ ? spec_.channels : 0;
  }

  int get_num_output_channels() const noexcept {
    return iscapture_ ? 0 : spec_.channels;
  }

  sample_rate_t get_sample_rate() const noexcept { return spec_.freq; }

  bool set_sample_rate(sample_rate_t freq) noexcept {
    if (is_running())
      return false;
    spec_.freq = freq;
    return true;
  }

  buffer_size_t get_buffer_size_frames() const noexcept {
    return spec_.samples;
  }

  bool set_buffer_size_frames(buffer_size_t buffer_size) noexcept {
    if (is_running())
      return false;
    spec_.samples = buffer_size;
    return true;
  }

  template <typename SampleType>
  static constexpr bool supports_sample_type() noexcept {
    return (std::is_floating_point_v<SampleType> ||
            std::is_integral_v<SampleType>)&&sizeof(SampleType) <= 32;
  }

  SDL_AudioFormat get_sample_type() const noexcept { return spec_.format; }

  template <typename SampleType> bool set_sample_type() noexcept {
    static_assert(supports_sample_type<SampleType>());
    if (is_running()) {
      return false;
    }
    spec_.format = GetTypeFormat<SampleType>();
    return true;
  }

  constexpr bool can_connect() const noexcept { return true; }

  // return if device is in play/pause statu
  bool is_running() const noexcept {
    return id_ != 0 &&
           (SDL_GetAudioDeviceStatus(id_) ==
                SDL_AudioStatus::SDL_AUDIO_PLAYING ||
            SDL_GetAudioDeviceStatus(id_) == SDL_AudioStatus::SDL_AUDIO_PAUSED);
  }

  template <typename SampleType>
  void connect(AudioIOCallback<SampleType> auto &&io_callback) {
    if (is_running()) {
      throw std::runtime_error("can't connect running device");
    }
    if (!supports_sample_type<SampleType>()) {
      throw std::runtime_error("sample type not supported");
    }
    set_sample_type<SampleType>();
    device_callback_ = device_callback;
    user_callback_ = [this,
                      cb = llvm::unique_function<void(
                          audio_device &, audio_device_io<SampleType> &)>(
                          std::move(io_callback)),
                      channel_num = spec_.channels](void *userdata,
                                                    Uint8 *stream,
                                                    int len) mutable noexcept {
      audio_device_io<SampleType> io = CreateDeviceIOFromBytes<SampleType>(
          stream, len, channel_num, iscapture_);
      cb(*this, io);
    };
  }

  constexpr bool can_process() const noexcept { return true; }

  template <typename SampleType>
  void process(AudioIOCallback<SampleType> auto &&io_callback) {
    if (!is_running()) {
      throw std::runtime_error("device is not running");
      return;
    }
    if (GetTypeFormat<SampleType>() != spec_.format) {
      throw std::runtime_error(
          "device and callback's sample type is different");
    }

    std::vector<uint8_t> process_buffer;

    if (iscapture_) {
      std::vector<uint8_t> capture_process_buffer(
          std::min(spec_.size, SDL_GetQueuedAudioSize(id_)));

      auto size = SDL_DequeueAudio(id_, capture_process_buffer.data(),
                                   capture_process_buffer.size());

      capture_process_buffer.resize(size);

      process_buffer.swap(capture_process_buffer);
    } else {
      process_buffer.resize(spec_.size);
    }

    audio_device_io<SampleType> io = CreateDeviceIOFromBytes<SampleType>(
        process_buffer.data(), process_buffer.size(), spec_.channels,
        iscapture_);

    io_callback(*this, io);

    if (!iscapture_) {
      if (SDL_QueueAudio(id_, process_buffer.data(), process_buffer.size()) !=
          0) {
        throw std::runtime_error("audio:: output Error :"s + SDL_GetError());
      }
    }
  }

  void wait() const {
    if (!is_running() || device_callback_) {
      return;
    }
    if (iscapture_) {
      auto size = SDL_GetQueuedAudioSize(id_);
      auto need_size = spec_.size;
      if (size < need_size) {
        std::this_thread::sleep_for(
            1000.0 *
            ((need_size - size) /
             ((spec_.format & ((1 << 8) - 1)) * get_num_input_channels())) /
            get_sample_rate() * 1ms);
      }
    }
  }

  bool has_unprocessed_io() const noexcept {
    if (device_callback_ || id_ == 0) {
      return false;
    }
    return iscapture_ ? SDL_GetQueuedAudioSize(id_) != 0 : false;
  }

  // return true if device statu turn play/stop/pause => play
  bool start() {
    if (!is_running() ||
        SDL_GetAudioDeviceStatus(id_) != SDL_AudioStatus::SDL_AUDIO_PAUSED) {
      if (id_ == 0 ||
          SDL_GetAudioDeviceStatus(id_) != SDL_AudioStatus::SDL_AUDIO_PAUSED) {
        spec_.userdata = this;
        spec_.callback = device_callback_;

        SDL_AudioSpec obtained;
        id_ = SDL_OpenAudioDevice(name_.c_str(), iscapture_, &spec_, &obtained,
                                  0);
        if (id_ == 0) {
          throw std::runtime_error("audio:: open device Error :"s +
                                   SDL_GetError());
        }
        spec_ = obtained;
      }

      if (SDL_PlayAudioDevice(id_) != 0) {
        throw std::runtime_error("audio:: play device Error :"s +
                                 SDL_GetError());
      }
      return true;
    }
    return false;
  }

  // return true if device statu turn pause/play => pause
  bool pause() {
    if (!is_running()) {
      return false;
    }
    if (SDL_GetAudioDeviceStatus(id_) != SDL_AudioStatus::SDL_AUDIO_PAUSED) {
      return true;
    }
    if (SDL_PauseAudioDevice(id_) != 0) {
      throw std::runtime_error("audio:: pause device Error :"s +
                               SDL_GetError());
    }
    return true;
  }

  // return true if device statu turn stop/play/pause => stop
  bool stop() {
    if (!is_running()) {
      return true;
    }
    pause();
    device_callback_ = nullptr;
    SDL_CloseAudioDevice(id_);
    return true;
  }

private:
  friend class audio_device_list;

  audio_device(device_id_t id, bool iscapture)
      : id_(id), iscapture_(iscapture) {
    SDL_GetAudioDeviceSpec(id, iscapture, &spec_);
    name_ = SDL_GetAudioDeviceName(id_, iscapture_);
    id_ = 0;
  }

  audio_device(std::string &&name, SDL_AudioSpec &&spec, bool iscapture)
      : iscapture_(iscapture), id_(0), name_(std::move(name)),
        spec_(std::move(spec)) {}

  static void device_callback(void *void_ptr_to_this_device, uint8_t *stream,
                              int len) {
    audio_device &this_device =
        *reinterpret_cast<audio_device *>(void_ptr_to_this_device);

    this_device.user_callback_(nullptr, stream, len);
  }

  template <typename SampleType>
  static auto CreateDeviceIOFromBytes(uint8_t *stream, int len, int channel_num,
                                      bool iscapture) {
    audio_device_io<SampleType> io;
    audio_buffer<SampleType> buffer(reinterpret_cast<SampleType *>(stream),
                                    (len / sizeof(SampleType)) / channel_num,
                                    channel_num, contiguous_interleaved);
    auto timestamp = audio_clock_t::now();
    if (iscapture) {
      io.input_buffer = std::move(buffer);
      io.input_time = timestamp;
    } else {
      io.output_buffer = std::move(buffer);
      io.output_time = timestamp;
    }
    return io;
  }

  template <typename SampleType>
  static constexpr SDL_AudioFormat GetTypeFormat() noexcept {
    return (std::is_signed_v<SampleType> << 15) |
           (std::is_floating_point_v<SampleType> << 8) |
           (sizeof(SampleType) * 8);
  }

  template <typename SampleType> auto buffer_size_in_byte() const noexcept {
    return sizeof(SampleType) * get_sample_rate() * get_buffer_size_frames();
  }

private:
  bool iscapture_;
  SDL_AudioCallback device_callback_ = nullptr;
  device_id_t id_{0};
  std::string name_;

  llvm::unique_function<detail::function_type<SDL_AudioCallback>::type>
      user_callback_;

  SDL_AudioSpec spec_;
};

class audio_device_list : public std::forward_list<audio_device> {
public:
  audio_device_list() {
    std::call_once(flag, []() {
      if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::abort();
      }
      std::atexit([]() {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_Quit();
      });
    });
  }

  audio_device default_input_device() { return default_device(true); }

  audio_device default_output_device() { return default_device(false); }

  void fill_with_input_device() { fill_with_audio_device(true); }

  void fill_with_output_device() { fill_with_audio_device(false); }

private:
  void fill_with_audio_device(bool iscapture) {
    clear();
    for (int i = SDL_GetNumAudioDevices(iscapture) - 1; i >= 0; i--) {
      push_front(audio_device(i, iscapture));
    }
  }

  audio_device default_device(bool iscapture) {
    char *name;
    SDL_AudioSpec spec;
    SDL_GetDefaultAudioInfo(&name, &spec, iscapture);
    std::string name_tmp(name);
    SDL_free(name);
    return audio_device(std::move(name_tmp), std::move(spec), iscapture);
  }

private:
  inline static auto flag = [] -> std::once_flag {
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    return {};
  }();
};

optional<audio_device> get_default_audio_input_device() {
  audio_device_list list;
  return list.default_input_device();
}

optional<audio_device> get_default_audio_output_device() {
  audio_device_list list;
  return list.default_output_device();
}

audio_device_list get_audio_input_device_list() {
  audio_device_list list;
  list.fill_with_input_device();
  return list;
}

audio_device_list get_audio_output_device_list() {
  audio_device_list list;
  list.fill_with_output_device();
  return list;
}

void set_audio_device_list_callback(audio_device_list_event event,
                                    AudioDeviceListCallback auto &&cb) {
  struct EventCallback {
    llvm::unique_function<void()> device_change;
    llvm::unique_function<void()> default_input_device_change;
    llvm::unique_function<void()> default_output_device_change;
  } static kEventCallback;
  static std::shared_mutex kCallbackMutex;
  std::unique_lock lock(kCallbackMutex);
  switch (event) {
  case audio_device_list_event::device_list_changed:
    kEventCallback.device_change = std::move(cb);
    break;
  case audio_device_list_event::default_input_device_changed:
    kEventCallback.default_input_device_change = std::move(cb);
    break;
  case audio_device_list_event::default_output_device_changed:
    kEventCallback.default_output_device_change = std::move(cb);
    break;
  default:
    assert("Unsupported Event" && false);
  }
  lock.unlock();

  static std::once_flag kEventCallFlag;
  std::call_once(kEventCallFlag, [] {
    SDL_SetEventFilter(
        [](void *userdata, SDL_Event *event) {
          switch (event->type) {
          case SDL_EventType::SDL_EVENT_AUDIO_DEVICE_ADDED:
          case SDL_EventType::SDL_EVENT_AUDIO_DEVICE_REMOVED: {
            std::shared_lock _(kCallbackMutex);
            if (kEventCallback.device_change) {
              kEventCallback.device_change();
            }
            if (kEventCallback.default_input_device_change &&
                event->adevice.iscapture &&
                SDL_GetAudioDeviceName(event->adevice.which, true) ==
                    get_default_audio_input_device()->name()) {
              kEventCallback.default_input_device_change();
            }
            if (kEventCallback.default_output_device_change &&
                !event->adevice.iscapture &&
                SDL_GetAudioDeviceName(event->adevice.which, false) ==
                    get_default_audio_output_device()->name()) {
              kEventCallback.default_output_device_change();
            }
          }
          default:
            return 1;
          }
        },
        nullptr);
  });
}

_LIBSTDAUDIO_NAMESPACE_END
