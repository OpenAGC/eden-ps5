// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <SDL3/SDL.h>

#include "common/logging.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/perf_stats.h"
#include "hid_core/hid_core.h"
#include "input_common/drivers/keyboard.h"
#include "input_common/drivers/mouse.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/main.h"
#include "yuzu_cmd/emu_window/emu_window_sdl3.h"
#include "yuzu_cmd/yuzu_icon.h"

namespace {

#ifdef __PROSPERO__
constexpr u32 Ps5PadButtonOptions = 0x0008;
constexpr u32 Ps5PadButtonUp = 0x0010;
constexpr u32 Ps5PadButtonRight = 0x0020;
constexpr u32 Ps5PadButtonDown = 0x0040;
constexpr u32 Ps5PadButtonLeft = 0x0080;
constexpr u32 Ps5PadButtonTriangle = 0x1000;
constexpr u32 Ps5PadButtonCircle = 0x2000;
constexpr u32 Ps5PadButtonCross = 0x4000;
constexpr u32 Ps5PadButtonSquare = 0x8000;

struct Ps5PadTouch {
    u16 x;
    u16 y;
    u8 finger;
    u8 reserved[3];
};

struct Ps5PadTouchData {
    u8 fingers;
    u8 reserved0[3];
    u32 reserved1;
    Ps5PadTouch touch[2];
};

struct Ps5PadData {
    u32 buttons;
    struct {
        u8 x;
        u8 y;
    } left_stick;
    struct {
        u8 x;
        u8 y;
    } right_stick;
    struct {
        u8 l2;
        u8 r2;
    } analog_buttons;
    u16 padding;
    float quaternion[4];
    float velocity[3];
    float acceleration[3];
    Ps5PadTouchData touch;
    u8 connected;
    u64 timestamp;
    u8 extension[16];
    u8 count;
    u8 unknown[15];
};

static_assert(sizeof(Ps5PadData) == 120);

extern "C" {
int sceUserServiceInitialize(void*);
int sceUserServiceGetLoginUserIdList(int user_ids[4]);
int scePadInit();
int scePadOpen(int user_id, int type, int index, const void* parameters);
int scePadReadState(int handle, Ps5PadData* data);
int scePadClose(int handle);
}

struct ProsperoPadKey {
    u32 button;
    int scancode;
    const char* name;
};

constexpr std::array ProsperoPadKeys{
    ProsperoPadKey{Ps5PadButtonCross, SDL_SCANCODE_A, "Cross/A"},
    ProsperoPadKey{Ps5PadButtonCircle, SDL_SCANCODE_S, "Circle/B"},
    ProsperoPadKey{Ps5PadButtonSquare, SDL_SCANCODE_Z, "Square/X"},
    ProsperoPadKey{Ps5PadButtonTriangle, SDL_SCANCODE_X, "Triangle/Y"},
    ProsperoPadKey{Ps5PadButtonOptions, SDL_SCANCODE_M, "Options/Plus"},
    ProsperoPadKey{Ps5PadButtonLeft, SDL_SCANCODE_1, "DPadLeft"},
    ProsperoPadKey{Ps5PadButtonUp, SDL_SCANCODE_2, "DPadUp"},
    ProsperoPadKey{Ps5PadButtonRight, SDL_SCANCODE_B, "DPadRight"},
};
#endif

int QualificationInputScancode(Eden::PS5::QualificationInputKey key) {
    switch (key) {
    case Eden::PS5::QualificationInputKey::A:
        return SDL_SCANCODE_A;
    case Eden::PS5::QualificationInputKey::B:
        return SDL_SCANCODE_S;
    case Eden::PS5::QualificationInputKey::Left:
        return SDL_SCANCODE_LEFT;
    case Eden::PS5::QualificationInputKey::Up:
        return SDL_SCANCODE_UP;
    case Eden::PS5::QualificationInputKey::Right:
        return SDL_SCANCODE_RIGHT;
    case Eden::PS5::QualificationInputKey::Down:
        return SDL_SCANCODE_DOWN;
    }
    return SDL_SCANCODE_UNKNOWN;
}

} // namespace

EmuWindow_SDL3::EmuWindow_SDL3(InputCommon::InputSubsystem* input_subsystem_, Core::System& system_)
    : input_subsystem{input_subsystem_}, system{system_} {
    input_subsystem->Initialize();
    SDL_InitFlags init_flags = SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;
#ifndef __PROSPERO__
    init_flags |= SDL_INIT_VIDEO;
#endif
    if (!SDL_Init(init_flags)) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL3: {}, Exiting...", SDL_GetError());
        exit(1);
    }
#ifdef __PROSPERO__
    InitializeProsperoPad();
#endif
}

EmuWindow_SDL3::~EmuWindow_SDL3() {
    SetQualificationInputCycle(false);
#ifdef __PROSPERO__
    ShutdownProsperoPad();
#endif
    system.HIDCore().UnloadInputDevices();
    input_subsystem->Shutdown();
    SDL_Quit();
}

#ifdef __PROSPERO__
void EmuWindow_SDL3::InitializeProsperoPad() {
    constexpr int AlreadyInitialized = static_cast<int>(0x80960003u);
    const int user_result = sceUserServiceInitialize(nullptr);
    if (user_result != 0 && user_result != AlreadyInitialized) {
        LOG_ERROR(Frontend, "Prospero pad input: sceUserServiceInitialize failed result=0x{:08x}",
                  static_cast<u32>(user_result));
        return;
    }
    const int pad_result = scePadInit();
    if (pad_result != 0) {
        LOG_ERROR(Frontend, "Prospero pad input: scePadInit failed result=0x{:08x}",
                  static_cast<u32>(pad_result));
        return;
    }
    int user_ids[4] = {-1, -1, -1, -1};
    const int list_result = sceUserServiceGetLoginUserIdList(user_ids);
    if (list_result != 0) {
        LOG_ERROR(Frontend,
                  "Prospero pad input: sceUserServiceGetLoginUserIdList failed result=0x{:08x}",
                  static_cast<u32>(list_result));
        return;
    }
    for (const int user_id : user_ids) {
        if (user_id == -1) {
            continue;
        }
        prospero_pad_handle = scePadOpen(user_id, 0, 0, nullptr);
        if (prospero_pad_handle >= 0) {
            LOG_INFO(Frontend, "Prospero pad input: initialized user={} handle={}", user_id,
                     prospero_pad_handle);
            return;
        }
        LOG_ERROR(Frontend, "Prospero pad input: scePadOpen failed user={} result=0x{:08x}",
                  user_id, static_cast<u32>(prospero_pad_handle));
        prospero_pad_handle = -1;
    }
    LOG_ERROR(Frontend, "Prospero pad input: no logged-in user has an available controller");
}

void EmuWindow_SDL3::PollProsperoPad() {
    if (prospero_pad_handle < 0) {
        return;
    }
    Ps5PadData pad{};
    const int result = scePadReadState(prospero_pad_handle, &pad);
    if (result != 0) {
        if (!prospero_pad_read_error_logged) {
            LOG_ERROR(Frontend, "Prospero pad input: scePadReadState failed result=0x{:08x}",
                      static_cast<u32>(result));
            prospero_pad_read_error_logged = true;
        }
        return;
    }
    prospero_pad_read_error_logged = false;
    const u32 buttons = pad.connected != 0 ? pad.buttons : 0;
    const u32 changed = buttons ^ prospero_pad_buttons;
    for (const auto& mapping : ProsperoPadKeys) {
        if ((changed & mapping.button) == 0) {
            continue;
        }
        const u8 pressed = (buttons & mapping.button) != 0 ? 1 : 0;
        OnKeyEvent(mapping.scancode, pressed);
        LOG_INFO(Frontend, "Prospero pad input: action={} button={}",
                 pressed != 0 ? "press" : "release", mapping.name);
    }
    prospero_pad_buttons = buttons;
}

void EmuWindow_SDL3::ShutdownProsperoPad() {
    if (prospero_pad_handle < 0) {
        return;
    }
    for (const auto& mapping : ProsperoPadKeys) {
        if ((prospero_pad_buttons & mapping.button) != 0) {
            OnKeyEvent(mapping.scancode, 0);
        }
    }
    prospero_pad_buttons = 0;
    const int result = scePadClose(prospero_pad_handle);
    if (result != 0) {
        LOG_ERROR(Frontend, "Prospero pad input: scePadClose failed result=0x{:08x}",
                  static_cast<u32>(result));
    } else {
        LOG_INFO(Frontend, "Prospero pad input: closed handle={}", prospero_pad_handle);
    }
    prospero_pad_handle = -1;
}
#endif

InputCommon::MouseButton EmuWindow_SDL3::SDLButtonToMouseButton(u32 button) const {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return InputCommon::MouseButton::Left;
    case SDL_BUTTON_RIGHT:
        return InputCommon::MouseButton::Right;
    case SDL_BUTTON_MIDDLE:
        return InputCommon::MouseButton::Wheel;
    case SDL_BUTTON_X1:
        return InputCommon::MouseButton::Backward;
    case SDL_BUTTON_X2:
        return InputCommon::MouseButton::Forward;
    default:
        return InputCommon::MouseButton::Undefined;
    }
}

/// @brief Translates pixel position to float position
EmuWindow_SDL3::FloatPairNonHFA EmuWindow_SDL3::MouseToTouchPos(s32 touch_x, s32 touch_y) const {
#ifdef __PROSPERO__
    const int w = Layout::ScreenUndocked::Width;
    const int h = Layout::ScreenUndocked::Height;
#else
    int w = 0, h = 0;
    SDL_GetWindowSize(render_window, &w, &h);
#endif
    const float fx = float(touch_x) / w;
    const float fy = float(touch_y) / h;
    return {
        std::clamp<float>(fx, 0.0f, 1.0f),
        std::clamp<float>(fy, 0.0f, 1.0f),
        0
    };
}

void EmuWindow_SDL3::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    const auto mouse_button = SDLButtonToMouseButton(button);
    if (state != 0) {
        auto const [touch_x, touch_y, _] = MouseToTouchPos(x, y);
        input_subsystem->GetMouse()->PressButton(x, y, mouse_button);
        input_subsystem->GetMouse()->PressMouseButton(mouse_button);
        input_subsystem->GetMouse()->PressTouchButton(touch_x, touch_y, mouse_button);
    } else {
        input_subsystem->GetMouse()->ReleaseButton(mouse_button);
    }
    input_subsystem->GetMouse()->NotifyChanged();
}

void EmuWindow_SDL3::OnMouseMotion(s32 x, s32 y) {
    auto const [touch_x, touch_y, _] = MouseToTouchPos(x, y);
    input_subsystem->GetMouse()->Move(x, y, 0, 0);
    input_subsystem->GetMouse()->MouseMove(touch_x, touch_y);
    input_subsystem->GetMouse()->TouchMove(touch_x, touch_y);
    input_subsystem->GetMouse()->NotifyChanged();
}

void EmuWindow_SDL3::OnFingerDown(float x, float y, std::size_t id) {
    input_subsystem->GetTouchScreen()->TouchPressed(x, y, id);
}

void EmuWindow_SDL3::OnFingerMotion(float x, float y, std::size_t id) {
    input_subsystem->GetTouchScreen()->TouchMoved(x, y, id);
}

void EmuWindow_SDL3::OnFingerUp() {
    input_subsystem->GetTouchScreen()->ReleaseAllTouch();
}

void EmuWindow_SDL3::OnKeyEvent(int key, u8 state) {
    if (state != 0) {
        input_subsystem->GetKeyboard()->PressKey(static_cast<std::size_t>(key));
    } else {
        input_subsystem->GetKeyboard()->ReleaseKey(static_cast<std::size_t>(key));
    }
}

void EmuWindow_SDL3::SetQualificationInputCycle(bool enabled, u32 press_limit,
                                                Eden::PS5::QualificationInputProfile profile) {
    if (qualification_input_cycle_enabled == enabled &&
        qualification_input_press_limit == press_limit && qualification_input_profile == profile) {
        return;
    }
    if (qualification_input_held_key != 0) {
        OnKeyEvent(qualification_input_held_key, 0);
        qualification_input_held_key = 0;
    }
    qualification_input_cycle_enabled = enabled;
    qualification_input_cycle_capped = false;
    qualification_input_cycle_started =
        enabled && Eden::PS5::QualificationInputMayStart(profile, GetPresentedFrameCount());
    qualification_input_press_limit = enabled ? press_limit : 0;
    qualification_input_profile = enabled ? profile : Eden::PS5::QualificationInputProfile::Generic;
    qualification_input_direction = 0;
    qualification_input_press_count = 0;
    qualification_input_last_step_ms = SDL_GetTicks();
    LOG_INFO(Frontend,
             "PS5 qualification input cycle: enabled={} profile={} interval_ms={} press_limit={}",
             enabled, Eden::PS5::QualificationInputProfileName(qualification_input_profile),
             Eden::PS5::QualificationInputStepIntervalMs(qualification_input_profile),
             qualification_input_press_limit);
}

void EmuWindow_SDL3::AdvanceQualificationInputCycle() {
    if (!qualification_input_cycle_enabled) {
        return;
    }
    if (!qualification_input_cycle_started) {
        const u32 presented_frame_count = GetPresentedFrameCount();
        if (!Eden::PS5::QualificationInputMayStart(qualification_input_profile,
                                                   presented_frame_count)) {
            return;
        }
        qualification_input_cycle_started = true;
        qualification_input_last_step_ms = SDL_GetTicks();
        LOG_INFO(Frontend,
                 "PS5 qualification input cycle: started profile={} presented_frames={}",
                 Eden::PS5::QualificationInputProfileName(qualification_input_profile),
                 presented_frame_count);
        return;
    }
    const u64 step_interval_ms =
        Eden::PS5::QualificationInputStepIntervalMs(qualification_input_profile);
    const u64 now = SDL_GetTicks();
    if (now - qualification_input_last_step_ms < step_interval_ms) {
        return;
    }
    qualification_input_last_step_ms = now;
    if (qualification_input_held_key != 0) {
        const auto released_key = Eden::PS5::QualificationInputKeyForPress(
            qualification_input_profile, qualification_input_press_count - 1);
        OnKeyEvent(qualification_input_held_key, 0);
        qualification_input_held_key = 0;
        if (qualification_input_profile == Eden::PS5::QualificationInputProfile::Flappy) {
            LOG_INFO(Frontend,
                     "PS5 qualification input: profile=flappy action=release key={} ordinal={} "
                     "limit={}",
                     Eden::PS5::QualificationInputKeyName(released_key),
                     qualification_input_press_count, qualification_input_press_limit);
        }
        if (qualification_input_press_limit != 0 &&
            qualification_input_press_count >= qualification_input_press_limit) {
            qualification_input_cycle_capped = true;
            LOG_INFO(Frontend,
                     "PS5 qualification input cycle: stopped profile={} presses={} limit={}",
                     Eden::PS5::QualificationInputProfileName(qualification_input_profile),
                     qualification_input_press_count, qualification_input_press_limit);
        }
        return;
    }
    if (qualification_input_cycle_capped) {
        return;
    }

    const auto pressed_key = Eden::PS5::QualificationInputKeyForPress(
        qualification_input_profile, qualification_input_direction++);
    qualification_input_held_key = QualificationInputScancode(pressed_key);
    OnKeyEvent(qualification_input_held_key, 1);
    ++qualification_input_press_count;
    if (qualification_input_profile == Eden::PS5::QualificationInputProfile::Flappy) {
        LOG_INFO(Frontend,
                 "PS5 qualification input: profile=flappy action=press key={} ordinal={} limit={}",
                 Eden::PS5::QualificationInputKeyName(pressed_key), qualification_input_press_count,
                 qualification_input_press_limit);
    } else if (qualification_input_press_count <= 9 || qualification_input_press_count % 32 == 0) {
        LOG_INFO(Frontend, "PS5 qualification input cycle: presses={}",
                 qualification_input_press_count);
    }
}

bool EmuWindow_SDL3::IsOpen() const {
    return is_open;
}

bool EmuWindow_SDL3::IsShown() const {
    return is_shown;
}

void EmuWindow_SDL3::SetPresentedFrameLimit(u32 limit) {
    presented_frames.store(0, std::memory_order_relaxed);
    presented_frame_limit.store(limit, std::memory_order_release);
}

u32 EmuWindow_SDL3::GetPresentedFrameCount() const {
    return presented_frames.load(std::memory_order_acquire);
}

void EmuWindow_SDL3::OnFrameDisplayed() {
    const u32 limit = presented_frame_limit.load(std::memory_order_acquire);
    if (limit == 0) {
        return;
    }
    u32 count = presented_frames.load(std::memory_order_relaxed);
    while (count < limit &&
           !presented_frames.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
    }
    if (count + 1 != limit) {
        return;
    }
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    if (!SDL_PushEvent(&quit)) {
        LOG_ERROR(Frontend, "Failed to enqueue the frame-limit shutdown event: {}", SDL_GetError());
    }
}

void EmuWindow_SDL3::OnResize() {
#ifdef __PROSPERO__
    UpdateCurrentFramebufferLayout(Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height);
#else
    int width, height;
    SDL_GetWindowSizeInPixels(render_window, &width, &height);
    UpdateCurrentFramebufferLayout(width, height);
#endif
}

void EmuWindow_SDL3::ShowCursor(bool show_cursor) {
    if (show_cursor) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

void EmuWindow_SDL3::Fullscreen() {
    SDL_DisplayMode display_mode;
    switch (Settings::values.fullscreen_mode.GetValue()) {
    case Settings::FullscreenMode::Exclusive:
        // Set window size to render size before entering fullscreen in exclusive mode.
        if (const SDL_DisplayMode* display_mode_ptr =
                SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(render_window))) {
            display_mode = *display_mode_ptr;
            SDL_SetWindowSize(render_window, display_mode.w, display_mode.h);
            SDL_SetWindowFullscreenMode(render_window, &display_mode);
        } else {
            LOG_ERROR(Frontend, "SDL_GetDesktopDisplayMode failed: {}", SDL_GetError());
        }

        if (SDL_SetWindowFullscreen(render_window, true)) {
            return;
        }

        LOG_ERROR(Frontend, "Fullscreening failed: {}", SDL_GetError());
        LOG_INFO(Frontend, "Attempting to use borderless fullscreen...");
        [[fallthrough]];
    case Settings::FullscreenMode::Borderless:
        SDL_SetWindowFullscreenMode(render_window, nullptr);
        if (SDL_SetWindowFullscreen(render_window, true)) {
            return;
        }

        LOG_ERROR(Frontend, "Borderless fullscreening failed: {}", SDL_GetError());
        [[fallthrough]];
    default:
        // Fallback algorithm: Maximise window.
        // Works on all systems (unless something is seriously wrong), so no fallback for this one.
        LOG_INFO(Frontend, "Falling back on a maximised window...");
        SDL_MaximizeWindow(render_window);
        break;
    }
}

void EmuWindow_SDL3::WaitEvent() {
    // Called on main thread
    SDL_Event event{};

    SDL_ClearError();
    const bool received_event = qualification_input_cycle_enabled
#ifdef __PROSPERO__
                                    || prospero_pad_handle >= 0
#endif
                                    ? SDL_WaitEventTimeout(&event, 50)
                                    : SDL_WaitEvent(&event);
    if (!received_event) {
        const char* error = SDL_GetError();
        if (!error || strcmp(error, "") == 0) {
            // https://github.com/libsdl-org/SDL/issues/5780
            // Sometimes SDL will return without actually having hit an error condition;
            // just ignore it in this case.
#ifdef __PROSPERO__
            PollProsperoPad();
#endif
            AdvanceQualificationInputCycle();
            return;
        }

        LOG_CRITICAL(Frontend, "SDL_WaitEvent failed: {}", error);
        exit(1);
    }

    switch (event.type) {
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
        OnResize();
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        is_shown = false;
        OnResize();
        break;
    case SDL_EVENT_WINDOW_EXPOSED:
        is_shown = true;
        OnResize();
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        is_open = false;
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        OnKeyEvent(static_cast<int>(event.key.scancode), event.key.down ? 1 : 0);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        // ignore if it came from touch
        if (event.button.which != SDL_TOUCH_MOUSEID)
            OnMouseMotion(event.motion.x, event.motion.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        // ignore if it came from touch
        if (event.button.which != SDL_TOUCH_MOUSEID) {
            OnMouseButton(event.button.button, event.button.down ? 1 : 0,
                          static_cast<s32>(event.button.x), static_cast<s32>(event.button.y));
        }
        break;
    case SDL_EVENT_FINGER_DOWN:
        OnFingerDown(event.tfinger.x, event.tfinger.y,
                     static_cast<std::size_t>(event.tfinger.touchID));
        break;
    case SDL_EVENT_FINGER_MOTION:
        OnFingerMotion(event.tfinger.x, event.tfinger.y,
                       static_cast<std::size_t>(event.tfinger.touchID));
        break;
    case SDL_EVENT_FINGER_UP:
        OnFingerUp();
        break;
    case SDL_EVENT_QUIT:
        is_open = false;
        break;
    default:
        break;
    }

    if (is_open) {
#ifdef __PROSPERO__
        PollProsperoPad();
#endif
        AdvanceQualificationInputCycle();
    } else {
        SetQualificationInputCycle(false);
    }

    const u32 current_time = SDL_GetTicks();
    if (current_time > last_time + 2000) {
        const auto results = system.GetAndResetPerfStats();
        const auto title = fmt::format("{} | {}-{} | FPS: {:.0f} ({:.0f}%)",
                                       Common::g_build_fullname,
                                       Common::g_scm_branch,
                                       Common::g_scm_desc,
                                       results.average_game_fps,
                                       results.emulation_speed * 100.0);
        if (render_window != nullptr) {
            SDL_SetWindowTitle(render_window, title.c_str());
        }
        last_time = current_time;
    }
}

// Credits to Samantas5855 and others for this function.
void EmuWindow_SDL3::SetWindowIcon() {
    SDL_IOStream* const yuzu_icon_stream = SDL_IOFromConstMem((void*)yuzu_icon, yuzu_icon_size);
    if (yuzu_icon_stream == nullptr) {
        LOG_WARNING(Frontend, "Failed to create Eden icon stream.");
        return;
    }
    SDL_Surface* const window_icon = SDL_LoadBMP_IO(yuzu_icon_stream, true);
    if (window_icon == nullptr) {
        LOG_WARNING(Frontend, "Failed to read BMP from stream.");
        return;
    }
    // The icon is attached to the window pointer
    SDL_SetWindowIcon(render_window, window_icon);
    SDL_DestroySurface(window_icon);
}

void EmuWindow_SDL3::OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) {
    if (render_window != nullptr) {
        SDL_SetWindowMinimumSize(render_window, minimal_size.first, minimal_size.second);
    }
}
