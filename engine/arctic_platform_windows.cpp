// The MIT License(MIT)
//
// Copyright 2017 - 2018 Huldra
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "engine/arctic_platform_def.h"

#ifdef ARCTIC_PLATFORM_WINDOWS

#define IDI_ICON1 129

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <winsock2.h>
#include <Mmsystem.h>
#include <Shlwapi.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>  // NOLINT
#include <sstream>
#include <thread>  // NOLINT
#include <vector>

#include "engine/engine.h"
#include "engine/easy.h"
#include "engine/arctic_input.h"
#include "engine/arctic_platform.h"
#include "engine/byte_array.h"
#include "engine/log.h"
#include "engine/rgb.h"
#include "engine/vec3f.h"

#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")


extern void EasyMain();

namespace arctic {

Ui16 FromBe(Ui16 x) {
  return ntohs(x);
}
Si16 FromBe(Si16 x) {
  return ntohs(x);
}
Ui32 FromBe(Ui32 x) {
  return ntohl(x);
}
Si32 FromBe(Si32 x) {
  return ntohl(x);
}
Ui16 ToBe(Ui16 x) {
  return htons(x);
}
Si16 ToBe(Si16 x) {
  return htons(x);
}
Ui32 ToBe(Ui32 x) {
  return htonl(x);
}
Si32 ToBe(Si32 x) {
  return htonl(x);
}

inline void Check(bool condition, const char *error_message,
  const char *error_message_postfix) {
  if (condition) {
    return;
  }
  Fatal(error_message, error_message_postfix);
}

void Fatal(const char *message, const char *message_postfix) {
  size_t size = 1 +
    strlen(message) +
    (message_postfix ? strlen(message_postfix) : 0);
  char *full_message = static_cast<char *>(LocalAlloc(LMEM_ZEROINIT, size));
  sprintf_s(full_message, size, "%s%s", message,
    (message_postfix ? message_postfix : ""));
  Log(full_message);
  StopLogger();
  MessageBox(NULL, full_message, "Arctic Engine", MB_OK | MB_ICONERROR);
  ExitProcess(1);
}

static void FatalWithLastError(const char* message_prefix,
  const char* message_infix = nullptr,
  const char* message_postfix = nullptr) {
  DWORD dw = GetLastError();
  char *message_info = "";
  char *message = "";
  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    dw,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&message_info,
    0, NULL);

  size_t size = 1 +
    strlen(message_prefix) +
    strlen(message_info) +
    (message_infix ? strlen(message_infix) : 0) +
    (message_postfix ? strlen(message_postfix) : 0);
  message = static_cast<char *>(LocalAlloc(LMEM_ZEROINIT, size));
  sprintf_s(message, size, "%s%s%s%s", message_prefix, message_info,
    (message_infix ? message_infix : ""),
    (message_postfix ? message_postfix : ""));
  Fatal(message);
}

static void CheckWithLastError(bool condition, const char *message_prefix,
  const char *message_infix = nullptr,
  const char *message_suffix = nullptr) {
  if (condition) {
    return;
  }
  FatalWithLastError(message_prefix, message_infix, message_suffix);
}

static const PIXELFORMATDESCRIPTOR pfd = {
  sizeof(PIXELFORMATDESCRIPTOR),
  1,
  PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
  PFD_TYPE_RGBA,
  32,
  0, 0, 0, 0, 0, 0, 8, 0,
  0, 0, 0, 0, 0,  // accum
  32,             // zbuffer
  0,              // stencil!
  0,              // aux
  PFD_MAIN_PLANE,
  0, 0, 0, 0
};

struct SystemInfo {
  HWND window_handle;
  HWND inner_window_handle;
  Si32 screen_width;
  Si32 screen_height;
};

SystemInfo g_system_info;
static bool g_is_full_screen = false;
static Si32 g_window_width = 0;
static Si32 g_window_height = 0;


KeyCode TranslateKeyCode(WPARAM word_param) {
  if (word_param >= 'A' && word_param <= 'Z') {
    return static_cast<KeyCode>(word_param - 'A' + kKeyA);
  }
  if (word_param >= '0' && word_param <= '9') {
    return static_cast<KeyCode>(word_param - '0' + kKey0);
  }
  if (word_param >= VK_F1 && word_param <= VK_F12) {
    return static_cast<KeyCode>(word_param - VK_F1 + kKeyF1);
  }
  if (word_param >= VK_NUMPAD0 && word_param <= VK_NUMPAD9) {
    return static_cast<KeyCode>(word_param - VK_NUMPAD0 + kKeyNumpad0);
  }

  switch (word_param) {
  case VK_LEFT:
    return kKeyLeft;
  case VK_RIGHT:
    return kKeyRight;
  case VK_UP:
    return kKeyUp;
  case VK_DOWN:
    return kKeyDown;
  case VK_BACK:
    return kKeyBackspace;
  case VK_TAB:
    return kKeyTab;
  case VK_RETURN:
    return kKeyEnter;
  case VK_HOME:
    return kKeyHome;
  case VK_END:
    return kKeyEnd;
  case VK_PRIOR:
    return kKeyPageUp;
  case VK_NEXT:
    return kKeyPageDown;
  case VK_SHIFT:
    return kKeyShift;
  case VK_LSHIFT:
    return kKeyLeftShift;
  case VK_RSHIFT:
    return kKeyRightShift;
  case VK_CONTROL:
    return kKeyControl;
  case VK_LCONTROL:
    return kKeyLeftControl;
  case VK_RCONTROL:
    return kKeyRightControl;
  case VK_MENU:
    return kKeyAlt;
  case VK_LMENU:
    return kKeyLeftAlt;
  case VK_RMENU:
    return kKeyRightAlt;
  case VK_ESCAPE:
    return kKeyEscape;
  case VK_SPACE:
    return kKeySpace;
  case VK_PAUSE:
    return kKeyPause;
  case VK_NUMLOCK:
    return kKeyNumLock;
  case VK_SCROLL:
    return kKeyScrollLock;
  case VK_CAPITAL:
    return kKeyCapsLock;
  case VK_SNAPSHOT:
    return kKeyPrintScreen;
  case VK_INSERT:
    return kKeyInsert;
  case VK_DELETE:
    return kKeyDelete;
  case VK_DIVIDE:
    return kKeyNumpadSlash;
  case VK_MULTIPLY:
    return kKeyNumpadAsterisk;
  case VK_SUBTRACT:
    return kKeyNumpadMinus;
  case VK_ADD:
    return kKeyNumpadPlus;
  case VK_DECIMAL:
    return kKeyNumpadPeriod;
  case VK_OEM_COMMA:
    return kKeyComma;
  case VK_OEM_PERIOD:
    return kKeyPeriod;
  case VK_OEM_MINUS:
    return kKeyMinus;
  case VK_OEM_PLUS:
    return kKeyEquals;
  case VK_OEM_1:
    return kKeySemicolon;
  case VK_OEM_2:
    return kKeySlash;
  case VK_OEM_3:
    return kKeyGraveAccent;
  case VK_OEM_4:
    return kKeyLeftSquareBracket;
  case VK_OEM_5:
    return kKeyBackslash;
  case VK_OEM_6:
    return kKeyRightSquareBracket;
  case VK_OEM_7:
    return kKeyApostrophe;
  case VK_OEM_8:
    return kKeySectionSign;
  }
  return kKeyUnknown;
}

void OnMouse(KeyCode key, WPARAM word_param, LPARAM long_param, bool is_down) {
  Check(g_window_width != 0, "Could not obtain window width in OnMouse");
  Check(g_window_height != 0, "Could not obtain window height in OnMouse");
  Si32 x = GET_X_LPARAM(long_param);
  Si32 y = g_window_height - GET_Y_LPARAM(long_param);
  Vec2F pos(static_cast<float>(x) / static_cast<float>(g_window_width - 1),
    static_cast<float>(y) / static_cast<float>(g_window_height - 1));
  InputMessage msg;
  msg.kind = InputMessage::kMouse;
  msg.keyboard.key = key;
  msg.keyboard.key_state = (is_down ? 1 : 2);
  msg.mouse.pos = pos;
  msg.mouse.wheel_delta = 0;
  PushInputMessage(msg);
}

void OnMouseWheel(WPARAM word_param, LPARAM long_param) {
  Check(g_window_width != 0, "Could not obtain window width in OnMouseWheel");
  Check(g_window_height != 0,
    "Could not obtain window height in OnMouseWheel");

  Si32 fw_keys = GET_KEYSTATE_WPARAM(word_param);
  Si32 z_delta = GET_WHEEL_DELTA_WPARAM(word_param);

  Si32 x = GET_X_LPARAM(long_param);
  Si32 y = g_window_height - GET_Y_LPARAM(long_param);

  Vec2F pos(static_cast<float>(x) / static_cast<float>(g_window_width - 1),
    static_cast<float>(y) / static_cast<float>(g_window_height - 1));
  InputMessage msg;
  msg.kind = InputMessage::kMouse;
  msg.keyboard.key = kKeyCount;
  msg.keyboard.key_state = false;
  msg.mouse.pos = pos;
  msg.mouse.wheel_delta = z_delta;
  PushInputMessage(msg);
}

void ToggleFullscreen() {
  SetFullScreen(!g_is_full_screen);
}

bool IsFullScreen() {
  return g_is_full_screen;
}

void SetFullScreen(bool is_enable) {
  if (is_enable == g_is_full_screen) {
    return;
  }
  g_is_full_screen = is_enable;
  if (g_is_full_screen) {
    SetWindowLong(g_system_info.window_handle, GWL_STYLE,
      WS_POPUP | WS_VISIBLE);
    ShowWindow(g_system_info.window_handle, SW_RESTORE);
    SetWindowPos(g_system_info.window_handle, HWND_TOP, 0, 0, 0, 0,
      SWP_FRAMECHANGED | SWP_NOSIZE);
    ShowWindow(g_system_info.window_handle, SW_SHOWMAXIMIZED);
  } else {
    SetWindowLong(g_system_info.window_handle, GWL_STYLE,
      WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(g_system_info.window_handle, 0, 0, 0, 0, 0,
      SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOZORDER);
  }
  RECT client_rect;
  GetClientRect(g_system_info.window_handle, &client_rect);
  SetWindowPos(g_system_info.inner_window_handle, 0, 0, 0,
    client_rect.right - client_rect.left,
    client_rect.bottom - client_rect.top,
    SWP_NOZORDER);
}

void OnKey(WPARAM word_param, LPARAM long_param, bool is_down) {
  KeyCode key = TranslateKeyCode(word_param);
  InputMessage msg;
  msg.kind = InputMessage::kKeyboard;
  msg.keyboard.key = key;
  msg.keyboard.key_state = (is_down ? 1 : 2);
  PushInputMessage(msg);
}

LRESULT CALLBACK WndProc(HWND window_handle, UINT message,
  WPARAM word_param, LPARAM long_param) {
  switch (message) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(window_handle, &ps);
    // TODO(Huldra): Add any drawing code that uses hdc here...
    EndPaint(window_handle, &ps);
    break;
  }
  case WM_KEYUP:
    arctic::OnKey(word_param, long_param, false);
    break;
  case WM_KEYDOWN:
    arctic::OnKey(word_param, long_param, true);
    break;
  case WM_SYSKEYDOWN:
    if (word_param == VK_RETURN && (HIWORD(long_param) & KF_ALTDOWN)) {
      ToggleFullscreen();
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(window_handle, message, word_param, long_param);
  }
  return 0;
}

LRESULT CALLBACK InnerWndProc(HWND inner_window_handle, UINT message,
  WPARAM word_param, LPARAM long_param) {
  switch (message) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(inner_window_handle, &ps);
    // TODO(Huldra): Add any drawing code that uses hdc here...
    EndPaint(inner_window_handle, &ps);
    break;
  }
  case WM_LBUTTONUP:
    arctic::OnMouse(kKeyMouseLeft, word_param, long_param, false);
    break;
  case WM_LBUTTONDOWN:
    arctic::OnMouse(kKeyMouseLeft, word_param, long_param, true);
    break;
  case WM_RBUTTONUP:
    arctic::OnMouse(kKeyMouseRight, word_param, long_param, false);
    break;
  case WM_RBUTTONDOWN:
    arctic::OnMouse(kKeyMouseRight, word_param, long_param, true);
    break;
  case WM_MBUTTONUP:
    arctic::OnMouse(kKeyMouseWheel, word_param, long_param, false);
    break;
  case WM_MBUTTONDOWN:
    arctic::OnMouse(kKeyMouseWheel, word_param, long_param, true);
    break;
  case WM_MOUSEMOVE:
    arctic::OnMouse(kKeyCount, word_param, long_param, false);
    break;
  case WM_MOUSEWHEEL:
    arctic::OnMouseWheel(word_param, long_param);
    break;
  default:
    return DefWindowProc(inner_window_handle, message,
      word_param, long_param);
  }
  return 0;
}

HBRUSH g_black_brush;

bool CreateMainWindow(HINSTANCE instance_handle, int cmd_show,
  SystemInfo *system_info) {
  char title_bar_text[] = {"Arctic Engine"};
  char window_class_name[] = {"ArcticEngineWindowClass"};
  char inner_window_class_name[] = {"ArcticEngineInnterWindowClass"};

  Si32 screen_width = GetSystemMetrics(SM_CXSCREEN);
  Si32 screen_height = GetSystemMetrics(SM_CYSCREEN);

  /*{
    DEVMODE dmScreenSettings;                   // Device Mode
    memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
  // Makes Sure Memory's Cleared
  dmScreenSettings.dmSize = sizeof(dmScreenSettings);
  // Size Of The Devmode Structure
  dmScreenSettings.dmPelsWidth = screen_width;
  // Selected Screen Width
  dmScreenSettings.dmPelsHeight = screen_height;
  // Selected Screen Height
  dmScreenSettings.dmBitsPerPel = 32;
  // Selected Bits Per Pixel
  dmScreenSettings.dmFields =
  DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
  // Try To Set Selected Mode And Get Results.
  // NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
  if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN)
  != DISP_CHANGE_SUCCESSFUL) {
  // If The Mode Fails, Offer Two Options.  Quit Or Run In A Window.
  MessageBox(NULL, "The requested fullscreen mode is not" \
  " supported by\nthe video card. Setting windowed mode.",
  "Arctic Engine", MB_OK | MB_ICONEXCLAMATION);
  }
  }*/

  g_black_brush = CreateSolidBrush(0);

  WNDCLASSEX wcex;
  memset(&wcex, 0, sizeof(wcex));
  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = arctic::WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = instance_handle;
  wcex.hIcon = LoadIcon(instance_handle, MAKEINTRESOURCE(IDI_ICON1));
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = g_black_brush;
  wcex.lpszClassName = window_class_name;
  // wcex.hIconSm = LoadIcon(wcex.hInstance,
  //      MAKEINTRESOURCE(IDI_SMALL_APP_ICON));

  ATOM register_class_result = RegisterClassEx(&wcex);

  g_window_width = screen_width;
  g_window_height = screen_height;

  HWND window_handle = CreateWindowExA(WS_EX_APPWINDOW,
    window_class_name, title_bar_text,
    WS_OVERLAPPEDWINDOW,
    0, 0, screen_width, screen_height, nullptr, nullptr,
    instance_handle, nullptr);
  if (!window_handle) {
    return false;
  }

  memset(&wcex, 0, sizeof(wcex));
  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wcex.lpfnWndProc = arctic::InnerWndProc;
  wcex.hInstance = instance_handle;
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = g_black_brush;
  wcex.lpszClassName = inner_window_class_name;

  ATOM register_inner_class_result = RegisterClassEx(&wcex);

  RECT client_rect;
  GetClientRect(window_handle, &client_rect);
  HWND inner_window_handle = CreateWindowExA(0,
    inner_window_class_name, "", WS_CHILD | WS_VISIBLE, 0, 0,
    client_rect.right - client_rect.left,
    client_rect.bottom - client_rect.top,
    window_handle, 0, instance_handle, 0);
  if (!inner_window_handle) {
    return false;
  }
  //  ShowWindow(window_handle, cmd_show);
  ShowWindow(window_handle, SW_MINIMIZE);
  ShowWindow(window_handle, SW_MAXIMIZE);
  UpdateWindow(window_handle);

  Check(!!system_info, "Error, system_info: nullptr in CreateMainWindow");
  system_info->window_handle = window_handle;
  system_info->inner_window_handle = inner_window_handle;
  system_info->screen_width = screen_width;
  system_info->screen_height = screen_height;
  return true;
}

static std::mutex g_sound_mixer_mutex;
struct SoundBuffer {
  easy::Sound sound;
  float volume = 1.0f;
  Si32 next_position = 0;
};
struct SoundMixerState {
  float master_volume = 0.7f;
  std::vector<SoundBuffer> buffers;
};
SoundMixerState g_sound_mixer_state;

void StartSoundBuffer(easy::Sound sound, float volume) {
  SoundBuffer buffer;
  buffer.sound = sound;
  buffer.volume = volume;
  buffer.next_position = 0;
  buffer.sound.GetInstance()->IncPlaying();
  std::lock_guard<std::mutex> lock(g_sound_mixer_mutex);
  g_sound_mixer_state.buffers.push_back(buffer);
}

void StopSoundBuffer(easy::Sound sound) {
  std::lock_guard<std::mutex> lock(g_sound_mixer_mutex);
  for (size_t idx = 0; idx < g_sound_mixer_state.buffers.size(); ++idx) {
    SoundBuffer &buffer = g_sound_mixer_state.buffers[idx];
    if (buffer.sound.GetInstance() == sound.GetInstance()) {
      buffer.sound.GetInstance()->DecPlaying();
      if (idx != g_sound_mixer_state.buffers.size() - 1) {
        g_sound_mixer_state.buffers[idx] =
          g_sound_mixer_state.buffers[
            g_sound_mixer_state.buffers.size() - 1];
      }
      g_sound_mixer_state.buffers.pop_back();
      idx--;
    }
  }
}

void SetMasterVolume(float volume) {
  std::lock_guard<std::mutex> lock(g_sound_mixer_mutex);
  g_sound_mixer_state.master_volume = volume;
}

float GetMasterVolume() {
  std::lock_guard<std::mutex> lock(g_sound_mixer_mutex);
  return g_sound_mixer_state.master_volume;
}

void SoundMixerThreadFunction() {
  Si32 bytes_per_sample = 2;

  WAVEFORMATEX format;
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = 44100;
  format.nAvgBytesPerSec =
    bytes_per_sample * format.nChannels * format.nSamplesPerSec;
  format.nBlockAlign = bytes_per_sample * format.nChannels;
  format.wBitsPerSample = 8 * bytes_per_sample;
  format.cbSize = 0;

  HWAVEOUT wave_out_handle;
  MMRESULT result = waveOutOpen(&wave_out_handle, WAVE_MAPPER,
    &format, 0, 0, WAVE_FORMAT_DIRECT);

  Ui32 buffer_count = 10ull;
  Ui64 buffer_duration_us = 10000ull;
  Ui32 buffer_samples_per_channel =
    static_cast<Ui32>(
      static_cast<Ui64>(format.nSamplesPerSec) *
      buffer_duration_us / 1000000ull);
  Ui32 buffer_samples_total = format.nChannels * buffer_samples_per_channel;
  Ui32 buffer_bytes = bytes_per_sample * buffer_samples_total;

  std::vector<WAVEHDR> wave_headers(buffer_count);
  std::vector<std::vector<Si16>> wave_buffers(buffer_count);
  std::vector<Si16> tmp(buffer_samples_total);
  std::vector<Si32> mix(buffer_samples_total);
  memset(&(mix[0]), 0, 2 * buffer_bytes);
  for (Ui32 i = 0; i < wave_headers.size(); ++i) {
    wave_buffers[i].resize(buffer_samples_total);
    memset(&(wave_buffers[i][0]), 0, buffer_bytes);

    memset(&wave_headers[i], 0, sizeof(WAVEHDR));
    wave_headers[i].dwBufferLength = buffer_bytes;
    wave_headers[i].lpData = reinterpret_cast<char*>(&(wave_buffers[i][0]));
    waveOutPrepareHeader(wave_out_handle,
      &wave_headers[i], sizeof(WAVEHDR));
    wave_headers[i].dwFlags = WHDR_DONE;
  }
  Check(result == MMSYSERR_NOERROR, "Error in SoundMixerThreadFunction");

  int cur_buffer_idx = 0;
  bool do_continue = true;
  timeBeginPeriod(1);
  while (do_continue) {
    while (!(wave_headers[cur_buffer_idx].dwFlags & WHDR_DONE)) {
      Sleep(0);
    }
    do {
      result = waveOutUnprepareHeader(wave_out_handle,
        &wave_headers[cur_buffer_idx], sizeof(WAVEHDR));
      if (result == WAVERR_STILLPLAYING) {
        Sleep(0);
      }
    } while (result == WAVERR_STILLPLAYING);

    wave_headers[cur_buffer_idx].dwFlags = 0;
    waveOutPrepareHeader(wave_out_handle,
      &wave_headers[cur_buffer_idx], sizeof(WAVEHDR));

    float master_volume = 1.0f;
    {
      memset(mix.data(), 0, 2 * buffer_bytes);
      std::lock_guard<std::mutex> lock(g_sound_mixer_mutex);
      master_volume = g_sound_mixer_state.master_volume;
      for (Ui32 idx = 0;
        idx < g_sound_mixer_state.buffers.size(); ++idx) {
        SoundBuffer &sound = g_sound_mixer_state.buffers[idx];

        Ui32 size = buffer_samples_per_channel;
        size = sound.sound.StreamOut(sound.next_position, size,
          tmp.data(), buffer_samples_total);
        Si16 *in_data = tmp.data();
        for (Ui32 i = 0; i < size; ++i) {
          mix[i * 2] += static_cast<Si32>(
            static_cast<float>(in_data[i * 2]) * sound.volume);
          mix[i * 2 + 1] += static_cast<Si32>(
            static_cast<float>(in_data[i * 2 + 1]) * sound.volume);
          ++sound.next_position;
        }

        if (sound.next_position == sound.sound.DurationSamples()
          || size == 0) {
          sound.sound.GetInstance()->DecPlaying();
          g_sound_mixer_state.buffers[idx] =
            g_sound_mixer_state.buffers[
              g_sound_mixer_state.buffers.size() - 1];
          g_sound_mixer_state.buffers.pop_back();
          --idx;
        }
      }
    }

    Si16* out_data = &(wave_buffers[cur_buffer_idx][0]);
    for (Ui32 i = 0; i < buffer_samples_total; ++i) {
      out_data[i] = static_cast<Si16>(Clamp(
        static_cast<float>(mix[i]) * master_volume, -32767.0, 32767.0));
    }

    waveOutWrite(wave_out_handle,
      &wave_headers[cur_buffer_idx], sizeof(WAVEHDR));
    cur_buffer_idx = (cur_buffer_idx + 1) % wave_headers.size();
  }
  timeEndPeriod(1);

  for (Ui32 i = 0; i < wave_headers.size(); ++i) {
    do {
      result = waveOutUnprepareHeader(wave_out_handle,
        &wave_headers[i], sizeof(WAVEHDR));
    } while (result == WAVERR_STILLPLAYING);
  }
  waveOutClose(wave_out_handle);
  return;
}

void EngineThreadFunction(SystemInfo system_info) {
  //  Init opengl start

  HDC hdc = GetDC(system_info.inner_window_handle);
  Check(hdc != nullptr, "Can't get the Device Context. Code: WIN01.");

  unsigned int pixel_format = ChoosePixelFormat(hdc, &pfd);
  Check(pixel_format != 0, "Can't choose the Pixel Format. Code: WIN02.");

  BOOL is_ok = SetPixelFormat(hdc, pixel_format, &pfd);
  Check(!!is_ok, "Can't set the Pixel Format. Code: WIN03.");

  HGLRC hrc = wglCreateContext(hdc);
  Check(hrc != nullptr, "Can't create the GL Context. Code: WIN04.");

  is_ok = wglMakeCurrent(hdc, hrc);
  Check(!!is_ok, "Can't make the GL Context current. Code: WIN05.");

  arctic::easy::GetEngine()->Init(system_info.screen_width,
    system_info.screen_height);
  //  Init opengl end

  EasyMain();

  ExitProcess(0);
}

void Swap() {
  HDC hdc = wglGetCurrentDC();
  BOOL res = SwapBuffers(hdc);
  CheckWithLastError(res != FALSE, "SwapBuffers error in Swap.");

  RECT client_rect;
  GetClientRect(g_system_info.window_handle, &client_rect);

  Si32 wid = client_rect.right - client_rect.left;
  Si32 hei = client_rect.bottom - client_rect.top;
  if (g_window_width != wid || g_window_height != hei) {
    SetWindowPos(g_system_info.inner_window_handle, 0, 0, 0,
      wid, hei, SWP_NOZORDER);
    RECT rect;
    res = GetClientRect(g_system_info.inner_window_handle, &rect);
    g_window_width = wid;
    g_window_height = hei;
    arctic::easy::GetEngine()->OnWindowResize(rect.right, rect.bottom);
  }
}

bool IsVSyncSupported() {
  const char* (WINAPI *wglGetExtensionsStringEXT)();
  wglGetExtensionsStringEXT = reinterpret_cast<const char* (WINAPI*)()>(  // NOLINT
    wglGetProcAddress("wglGetExtensionsStringEXT"));
  if (wglGetExtensionsStringEXT == nullptr) {
    return false;
  }
  // CheckWithLastError(wglGetExtensionsStringEXT != nullptr,
  // "Error in wglGetProcAddress(\"wglGetExtensionsStringEXT\"): ");
  const char *extensions = wglGetExtensionsStringEXT();
  if (strstr(extensions, "WGL_EXT_swap_control") == nullptr) {
    return false;
  }
  return true;
}

bool SetVSync(bool is_enable) {
  if (!IsVSyncSupported()) {
    return false;
  }
  bool (APIENTRY *wglSwapIntervalEXT)(int);
  wglSwapIntervalEXT = reinterpret_cast<bool (APIENTRY *)(int)>(  // NOLINT
    wglGetProcAddress("wglSwapIntervalEXT"));
  CheckWithLastError(wglSwapIntervalEXT != nullptr,
    "Error in wglGetProcAddress(\"wglSwapIntervalEXT\"): ");
  bool is_ok = wglSwapIntervalEXT(is_enable ? 1 : 0);
  CheckWithLastError(is_ok, "Error in SetVSync: ");
  return is_ok;
}

Trivalent DoesDirectoryExist(const char *path) {
  struct stat info;
  if (stat(path, &info) != 0) {
    return kTrivalentFalse;
  } else if (info.st_mode & S_IFDIR) {
    return kTrivalentTrue;
  } else {
    return kTrivalentUnknown;
  }
}

bool MakeDirectory(const char *path) {
  BOOL is_ok = CreateDirectory(path, NULL);
  return is_ok;
}

bool GetCurrentPath(std::string *out_dir) {
  char cwd[1 << 20];
  DWORD res = GetCurrentDirectoryA(sizeof(cwd), cwd);
  if (res > 0) {
    out_dir->assign(cwd);
    return true;
  }
  return false;
}

bool GetDirectoryEntries(const char *path,
     std::deque<DirectoryEntry> *out_entries) {
  Check(out_entries,
        "GetDirectoryEntries Error. Unexpected nullptr in out_entries!");
  out_entries->clear();

  std::string canonic_path = CanonicalizePath(path);
  if (canonic_path.size() == 0) {
    std::stringstream info;
    info << "GetDirectoryEntries can't canonize path: \"" << path << "\""
      << std::endl;
    Log(info.str().c_str());
    return false;
  }
  std::stringstream search_pattern;
  search_pattern << canonic_path << "\\*";

  WIN32_FIND_DATA find_data;
  HANDLE find_handle = FindFirstFile(search_pattern.str().c_str(), &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    std::stringstream info;
    info << "GetDirectoryEntires error in FindFirstFile, path: \""
      << path << "\"" << std::endl;
    Log(info.str().c_str());
    return false;
  }

  while (true) {
    DirectoryEntry entry;
    entry.title = find_data.cFileName;
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      entry.is_directory = kTrivalentTrue;
    } else {
      entry.is_file = kTrivalentTrue;
    }
    out_entries->push_back(entry);
    if (FindNextFile(find_handle, &find_data) == 0) {
      break;
    }
  }

  DWORD last_error = GetLastError();
  if (last_error != ERROR_NO_MORE_FILES) {
    FatalWithLastError("GetDirectoryEntries error in FindNextFile");
  }
  FindClose(find_handle);
  return true;
}


std::string CanonicalizePath(const char *path) {
  Check(path, "CanonicalizePath error, path can't be nullptr");
  char canonic_path[MAX_PATH];
  BOOL is_ok = PathCanonicalize(canonic_path, path);
  std::string result;
  if (is_ok) {
    result.assign(canonic_path);
  }
  return result;
}

std::string RelativePathFromTo(const char *from, const char *to) {
  std::string from_abs = CanonicalizePath(from);
  std::string to_abs = CanonicalizePath(to);
  char relative_path[MAX_PATH];
  BOOL is_ok = PathRelativePathTo(relative_path,
    from_abs.c_str(), FILE_ATTRIBUTE_DIRECTORY,
    to_abs.c_str(), FILE_ATTRIBUTE_DIRECTORY);
  std::string result;
  if (is_ok) {
    result.assign(relative_path);
  }
  return result;
}

}  // namespace arctic

int APIENTRY wWinMain(_In_ HINSTANCE instance_handle,
  _In_opt_ HINSTANCE prev_instance_handle,
  _In_ LPWSTR command_line,
  _In_ int cmd_show) {
  UNREFERENCED_PARAMETER(prev_instance_handle);
  UNREFERENCED_PARAMETER(command_line);

  arctic::StartLogger();

  BOOL is_ok_w = SetProcessDPIAware();
  arctic::Check(is_ok_w != FALSE,
    "Error from SetProessDPIAware! Code: WIN06.");

  DisableProcessWindowsGhosting();

  bool is_ok = arctic::CreateMainWindow(instance_handle, cmd_show,
    &arctic::g_system_info);
  arctic::Check(is_ok, "Can't create the Main Window! Code: WIN07.");

  arctic::easy::GetEngine();

  std::thread sound_thread(arctic::SoundMixerThreadFunction);
  std::thread engine_thread(arctic::EngineThreadFunction,
    arctic::g_system_info);
  while (true) {
    MSG msg;
    BOOL ret = GetMessage(&msg, NULL, 0, 0);
    if (ret == 0) {
      break;
    } else if (ret == -1) {
      // handle the error and possibly exit
      break;
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  arctic::StopLogger();
  ExitProcess(0);
  //    engine_thread.join();
  return 0;
}

#endif  // ARCTIC_PLATFORM_WINDOWS
