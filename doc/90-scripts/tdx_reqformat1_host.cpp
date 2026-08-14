#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr UINT kFetchMessage = WM_USER + 100;  // 1124, as used by TdxZdView100.
constexpr UINT kLoginMessage = WM_APP + 1;
constexpr UINT_PTR kPollTimer = 1;

using SetEnvironmentFn = int(__cdecl *)(const char *, const char *, int);
using InitializeFn = int(__cdecl *)();
using SetUserFn = void(__cdecl *)(const char *, int, int);
using LoginAnonymousFn = int(__cdecl *)(int, HWND, int);
using IsLoggedInFn = int(__cdecl *)(int);
using GetSessionFn = void *(__cdecl *)(int);
using SetActiveSessionFn = void(__cdecl *)(void *);
using UninitializeFn = void(__cdecl *)();
using CreateHandleFn = void *(__cdecl *)(HWND, int);
using CreateHandleExFn = void *(__cdecl *)(void *, HWND, int);
using DeleteHandleFn = void(__cdecl *)(void *);
using ConfigureFn = int(__thiscall *)(void *, int, const char *, char *, int);
using StartFn = bool(__thiscall *)(void *);

struct Options {
  std::string install = "C:\\new_tdx";
  std::string entry;
  std::string params;
  std::string output;
  DWORD timeout_ms = 30000;
  int request_format = 1;
  bool allow_network = false;
  bool verbose = false;
};

Options g_options;
HMODULE g_module = nullptr;
HWND g_window = nullptr;
void *g_fetch_handle = nullptr;
ULONGLONG g_started_at = 0;
bool g_request_started = false;
int g_exit_code = 1;

SetEnvironmentFn g_set_environment = nullptr;
InitializeFn g_initialize = nullptr;
SetUserFn g_set_user = nullptr;
LoginAnonymousFn g_login_anonymous = nullptr;
IsLoggedInFn g_is_logged_in = nullptr;
GetSessionFn g_get_session = nullptr;
SetActiveSessionFn g_set_active_session = nullptr;
UninitializeFn g_uninitialize = nullptr;
CreateHandleFn g_create_handle = nullptr;
CreateHandleExFn g_create_handle_ex = nullptr;
DeleteHandleFn g_delete_handle = nullptr;

void PrintUsage() {
  std::fputs(
      "Usage: tdx_reqformat1_host.exe --allow-network --entry NAME "
      "[--format 0..3] [--params RAW] [--output FILE] [--root DIR] "
      "[--timeout SECONDS] [--verbose]\n"
      "\n"
      "RAW is the comma-separated content placed inside Params:[...].\n"
      "The program is a 32-bit message-loop host for the official TPData100.dll.\n",
      stderr);
}

bool ParseArguments(int argc, char **argv) {
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--allow-network") {
      g_options.allow_network = true;
    } else if (argument == "--verbose") {
      g_options.verbose = true;
    } else if (argument == "--help" || argument == "-h") {
      PrintUsage();
      std::exit(0);
    } else if (argument == "--entry" && index + 1 < argc) {
      g_options.entry = argv[++index];
    } else if (argument == "--params" && index + 1 < argc) {
      g_options.params = argv[++index];
    } else if (argument == "--output" && index + 1 < argc) {
      g_options.output = argv[++index];
    } else if (argument == "--root" && index + 1 < argc) {
      g_options.install = argv[++index];
    } else if (argument == "--timeout" && index + 1 < argc) {
      const long seconds = std::strtol(argv[++index], nullptr, 10);
      if (seconds <= 0 || seconds > 600) {
        std::fprintf(stderr, "invalid --timeout value\n");
        return false;
      }
      g_options.timeout_ms = static_cast<DWORD>(seconds * 1000);
    } else if (argument == "--format" && index + 1 < argc) {
      const long format = std::strtol(argv[++index], nullptr, 10);
      if (format < 0 || format > 3) {
        std::fprintf(stderr, "invalid --format value\n");
        return false;
      }
      g_options.request_format = static_cast<int>(format);
    } else {
      std::fprintf(stderr, "unknown or incomplete argument: %s\n",
                   argument.c_str());
      return false;
    }
  }
  if (!g_options.allow_network) {
    std::fprintf(stderr,
                 "network gate is closed; add --allow-network to send\n");
    return false;
  }
  if (g_options.entry.empty()) {
    std::fprintf(stderr, "--entry is required\n");
    return false;
  }
  return true;
}

std::string JoinPath(const std::string &left, const char *right) {
  if (!left.empty() && (left.back() == '\\' || left.back() == '/')) {
    return left + right;
  }
  return left + "\\" + right;
}

std::string AbsolutePath(const std::string &path) {
  char buffer[32768] = {};
  const DWORD length =
      GetFullPathNameA(path.c_str(), static_cast<DWORD>(sizeof(buffer)), buffer,
                       nullptr);
  if (length == 0 || length >= sizeof(buffer)) {
    return path;
  }
  return std::string(buffer, length);
}

template <typename T>
bool Resolve(T *target, const char *name) {
  *target = reinterpret_cast<T>(GetProcAddress(g_module, name));
  if (*target == nullptr) {
    std::fprintf(stderr, "missing TPData export: %s\n", name);
    return false;
  }
  return true;
}

void Finish(int exit_code) {
  g_exit_code = exit_code;
  KillTimer(g_window, kPollTimer);
  PostQuitMessage(exit_code);
}

bool WriteResponse(const void *data, size_t size) {
  FILE *stream = stdout;
  if (!g_options.output.empty()) {
    if (fopen_s(&stream, g_options.output.c_str(), "wb") != 0 ||
        stream == nullptr) {
      std::fprintf(stderr, "cannot open output file: %s\n",
                   g_options.output.c_str());
      return false;
    }
  }
  const size_t written = std::fwrite(data, 1, size, stream);
  if (stream != stdout) {
    std::fclose(stream);
  } else {
    std::fflush(stream);
  }
  if (written != size) {
    std::fprintf(stderr, "short response write: %zu/%zu\n", written, size);
    return false;
  }
  std::fprintf(stderr, "response-bytes=%zu\n", size);
  return true;
}

bool StartRequest() {
  void *session = g_get_session(100);
  std::fprintf(stderr, "session=%p\n", session);
  g_fetch_handle =
      session != nullptr
          ? g_create_handle_ex(session, g_window, kFetchMessage)
          : g_create_handle(g_window, kFetchMessage);
  if (g_fetch_handle == nullptr) {
    std::fprintf(stderr, "CreateFetchDataHandle returned NULL\n");
    return false;
  }
  void **vtable = *reinterpret_cast<void ***>(g_fetch_handle);
  auto configure = reinterpret_cast<ConfigureFn>(vtable[5]);
  auto start = reinterpret_cast<StartFn>(vtable[6]);
  const int configured =
      configure(g_fetch_handle, g_options.request_format,
                g_options.entry.c_str(),
                const_cast<char *>(g_options.params.c_str()), 1);
  if (!configured) {
    std::fprintf(stderr, "reqformat=%d configure failed\n",
                 g_options.request_format);
    return false;
  }
  if (g_options.verbose) {
    const auto object = static_cast<unsigned char *>(g_fetch_handle);
    const char *request_body =
        *reinterpret_cast<const char **>(object + 128);
    std::fprintf(stderr, "configured-entry=%s\nconfigured-body=%s\n",
                 reinterpret_cast<const char *>(object + 64),
                 request_body != nullptr ? request_body : "<null>");
  }
  if (!start(g_fetch_handle)) {
    std::fprintf(stderr, "reqformat=%d start failed\n",
                 g_options.request_format);
    return false;
  }
  g_request_started = true;
  std::fprintf(stderr, "request-started format=%d entry=%s\n",
               g_options.request_format, g_options.entry.c_str());
  return true;
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM w_param,
                                 LPARAM l_param) {
  if (message == WM_TIMER && w_param == kPollTimer) {
    if (GetTickCount64() - g_started_at > g_options.timeout_ms) {
      std::fprintf(stderr, "%s timed out\n",
                   g_request_started ? "request" : "anonymous login");
      Finish(2);
      return 0;
    }
    if (!g_request_started && g_is_logged_in(100)) {
      std::fprintf(stderr, "anonymous-login=ready\n");
      if (!StartRequest()) {
        Finish(3);
      }
      return 0;
    }
  } else if (message == kFetchMessage) {
    void *handle = reinterpret_cast<void *>(l_param);
    if (handle == nullptr) {
      std::fprintf(stderr, "completion message has a NULL handle\n");
      Finish(4);
      return 0;
    }
    const auto object = static_cast<unsigned char *>(handle);
    const unsigned int error_text_size =
        *reinterpret_cast<unsigned int *>(object + 36);
    const unsigned int error_text_capacity =
        *reinterpret_cast<unsigned int *>(object + 40);
    const char *error_text =
        error_text_capacity < 16
            ? reinterpret_cast<const char *>(object + 20)
            : *reinterpret_cast<const char **>(object + 20);
    std::fprintf(stderr,
                 "completion wparam=%lu ready=%u error-code=%d error=%.*s\n",
                 static_cast<unsigned long>(w_param),
                 static_cast<unsigned int>(object[12]),
                 *reinterpret_cast<int *>(object + 16),
                 static_cast<int>(error_text_size),
                 error_text != nullptr ? error_text : "");
    const auto bytes = *reinterpret_cast<unsigned char **>(
        object + 48);
    const size_t size = *reinterpret_cast<unsigned int *>(
        object + 52);
    const bool has_response = bytes != nullptr && size != 0;
    const bool written = has_response && WriteResponse(bytes, size);
    if (!has_response) {
      std::fprintf(stderr, "TPData response buffer is empty\n");
    }
    if (handle == g_fetch_handle) {
      g_fetch_handle = nullptr;
    }
    g_delete_handle(handle);
    Finish(written ? 0 : 5);
    return 0;
  } else if (message == kLoginMessage) {
    return 0;
  } else if (message == WM_DESTROY) {
    PostQuitMessage(g_exit_code);
    return 0;
  }
  return DefWindowProcA(window, message, w_param, l_param);
}

bool InitializeHost(HINSTANCE instance) {
  SetCurrentDirectoryA(g_options.install.c_str());
  const std::string module_path =
      JoinPath(g_options.install, "ZDPlugins\\TPData100.dll");
  g_module = LoadLibraryExA(module_path.c_str(), nullptr,
                            LOAD_WITH_ALTERED_SEARCH_PATH);
  if (g_module == nullptr) {
    std::fprintf(stderr, "LoadLibraryEx failed: %lu (%s)\n", GetLastError(),
                 module_path.c_str());
    return false;
  }
  if (!Resolve(&g_set_environment, "ITPConn_SetEnvironPath") ||
      !Resolve(&g_initialize, "ITPConn_Init") ||
      !Resolve(&g_set_user, "ITPConn_SetUser") ||
      !Resolve(&g_login_anonymous, "ITPConn_LoginAnony") ||
      !Resolve(&g_is_logged_in, "ITPConn_IsLogined") ||
      !Resolve(&g_get_session, "ITPConn_GetSession") ||
      !Resolve(&g_set_active_session, "ITPConn_SetActiveSession") ||
      !Resolve(&g_uninitialize, "ITPConn_UnInit") ||
      !Resolve(&g_create_handle, "CreateFetchDataHandle") ||
      !Resolve(&g_create_handle_ex, "CreateFetchDataHandleEx") ||
      !Resolve(&g_delete_handle, "DeleteFetchDataHandle")) {
    return false;
  }

  WNDCLASSA window_class = {};
  window_class.lpfnWndProc = WindowProcedure;
  window_class.hInstance = instance;
  window_class.lpszClassName = "TdxReqformat1Host";
  if (!RegisterClassA(&window_class) &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    std::fprintf(stderr, "RegisterClass failed: %lu\n", GetLastError());
    return false;
  }
  g_window = CreateWindowExA(0, window_class.lpszClassName,
                             "TDX reqformat=1 host", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, instance, nullptr);
  if (g_window == nullptr) {
    std::fprintf(stderr, "CreateWindowEx failed: %lu\n", GetLastError());
    return false;
  }

  const std::string install_path = JoinPath(g_options.install, "");
  const std::string user_path = JoinPath(g_options.install, "T0001\\");
  g_set_environment(install_path.c_str(), user_path.c_str(), 0);
  if (!g_initialize()) {
    std::fprintf(stderr, "ITPConn_Init failed\n");
    return false;
  }
  SYSTEMTIME local_time = {};
  GetLocalTime(&local_time);
  const int trading_date = local_time.wYear * 10000 +
                           local_time.wMonth * 100 +
                           local_time.wDay;
  g_set_user("", 0, trading_date);
  const int login_started =
      g_login_anonymous(100, g_window, static_cast<int>(kLoginMessage));
  if (!login_started) {
    std::fprintf(stderr, "ITPConn_LoginAnony failed\n");
    return false;
  }
  void *session = g_get_session(100);
  if (session != nullptr) {
    g_set_active_session(session);
  }
  g_started_at = GetTickCount64();
  SetTimer(g_window, kPollTimer, 100, nullptr);
  std::fprintf(stderr, "anonymous-login=started\n");
  return true;
}

void Cleanup() {
  if (g_fetch_handle != nullptr && g_delete_handle != nullptr) {
    g_delete_handle(g_fetch_handle);
    g_fetch_handle = nullptr;
  }
  if (g_uninitialize != nullptr) {
    g_uninitialize();
  }
  if (g_window != nullptr) {
    DestroyWindow(g_window);
    g_window = nullptr;
  }
  if (g_module != nullptr) {
    FreeLibrary(g_module);
    g_module = nullptr;
  }
}

}  // namespace

int main(int argc, char **argv) {
  if (!ParseArguments(argc, argv)) {
    PrintUsage();
    return 64;
  }
  g_options.install = AbsolutePath(g_options.install);
  if (!g_options.output.empty()) {
    g_options.output = AbsolutePath(g_options.output);
  }
  if (!InitializeHost(GetModuleHandleA(nullptr))) {
    Cleanup();
    return 1;
  }
  MSG message = {};
  while (GetMessageA(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageA(&message);
  }
  Cleanup();
  return g_exit_code;
}
