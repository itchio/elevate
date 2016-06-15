
#include <stdio.h>
#include <windows.h>
#include <userenv.h>

#ifndef SEE_MASK_NOASYNC
#define SEE_MASK_NOASYNC 0x00000100
#endif

#define SAFE_APPEND(fmt, arg) \
  { \
    int written = snprintf(tmp, MAX_ARGUMENTS_LENGTH, (fmt), parameters, (arg)); \
    if (written < 0 || written >= MAX_ARGUMENTS_LENGTH) { \
      bail(1, "argument string too long"); \
    } \
    \
    strncpy(parameters, tmp, MAX_ARGUMENTS_LENGTH); \
  }

// cf. https://www.msigeek.com/442/windows-os-version-numbers - Vista is 6.0
#define VISTA_MAJOR_VERSION 6

static void bail(int code, char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(code);
}

static void wbail(int code, char *msg) {
  LPVOID lpvMessageBuffer;

  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
    FORMAT_MESSAGE_FROM_SYSTEM,
    NULL, GetLastError(), 
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
    (LPWSTR)&lpvMessageBuffer, 0, NULL);

  printf("API = %s.\n", msg);
  wprintf(L"error code = %d.\n", GetLastError());
  wprintf(L"message    = %s.\n", (LPWSTR)lpvMessageBuffer);

  LocalFree(lpvMessageBuffer);

  fprintf(stderr, "%s\n", msg);
  exit(code);
}

static int getMajorOSVersion() {
  OSVERSIONINFO ovi;

  ZeroMemory(&ovi, sizeof(OSVERSIONINFO));
  ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&ovi);

  return ovi.dwMajorVersion;
}

int exec(const char *verb, const char *file, const char *parameters) {
  SHELLEXECUTEINFO sei;

  fprintf(stderr, "[elevate] %s %s %s\n", verb, file, parameters);
  fflush(stderr);

  ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
  sei.cbSize = sizeof(SHELLEXECUTEINFO);
  // we want to wait until the program exits to exit with its code
  sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = verb;
  sei.lpFile = file;
  sei.lpParameters = parameters;
  sei.nShow = SW_HIDE;

  if (ShellExecuteEx(&sei) == FALSE || sei.hProcess == NULL) {
    // This happens if user denied permission in UAC dialog, for example
    bail(127, "failed ShellExecuteEx call / null hProcess");
  }

  WaitForSingleObject(sei.hProcess, INFINITE);

  DWORD code;
  if (GetExitCodeProcess(sei.hProcess, &code) == 0) {
    // Not sure when this could ever happen.
    bail(127, "failed GetExitCodeProcess call");
  }

  return code;
}

int elevate(int argc, char** argv) {
  const int MAX_ARGUMENTS_LENGTH = 65536;
  char parameters[MAX_ARGUMENTS_LENGTH];
  char tmp[MAX_ARGUMENTS_LENGTH];

  parameters[0] = '\0';

  for (int i = 2; i < argc; i++) {
    SAFE_APPEND("%s%s ", argv[i])
  }

  fprintf(stderr, "[elevate] %s %s\n", argv[1], parameters);
  fflush(stderr);

  return exec("runas", argv[1], parameters);
}

void msibail() {
  bail(1, "Usage: elevate --msiexec [--install/--uninstall] msiPath installPath logPath");
}

int msiexec(int argc, char** argv) {
  const int MAX_ARGUMENTS_LENGTH = 65536;
  char parameters[MAX_ARGUMENTS_LENGTH];
  char tmp[MAX_ARGUMENTS_LENGTH];

  parameters[0] = '\0';

  if (argc < 6) {
    fprintf(stderr, "Not enough arguments, expected 6, got %d\n", argc);
    msibail();
  }

  const char* command = argv[2];
  const char* msiPath = argv[3];
  const char* installPath = argv[4];
  const char* logPath = argv[5];

  SAFE_APPEND("%s%s ", "ALLUSERS=2");
  SAFE_APPEND("%s%s ", "MSIINSTALLPERUSER=1");
  SAFE_APPEND("%sTARGETDIR=\"%s\" ", installPath);
  SAFE_APPEND("%sINSTALLDIR=\"%s\" ", installPath);
  SAFE_APPEND("%sAPPDIR=\"%s\" ", installPath);

  SAFE_APPEND("%s%s ", "/norestart");
  SAFE_APPEND("%s%s ", "/quiet");
  SAFE_APPEND("%s%s ", "/l*v");
  SAFE_APPEND("%s%s ", logPath);

  char *verb = "open";
  if (strcmp(command, "--install") == 0) {
    SAFE_APPEND("%s%s ", "/i");
  } else if (strcmp(command, "--uninstall") == 0) {
    SAFE_APPEND("%s%s ", "/x");
  } else if (strcmp(command, "--elevated-install") == 0) {
    SAFE_APPEND("%s%s ", "/i");
    verb = "runas";
  } else if (strcmp(command, "--elevated-uninstall") == 0) {
    SAFE_APPEND("%s%s ", "/x");
    verb = "runas";
  } else {
    fprintf(stderr, "Command must be --install or --uninstall, got %s\n", command);
    msibail();
  }

  SAFE_APPEND("%s%s ", msiPath);

  return exec(verb, "msiexec", parameters);
}

void toWideChar (const char *s, wchar_t **ws) {
  int wchars_num = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  *ws = malloc(wchars_num * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, s, -1, *ws, wchars_num);
}

int runas(int argc, char** argv) {
  const int MAX_ARGUMENTS_LENGTH = 65536;
  char parameters[MAX_ARGUMENTS_LENGTH];
  char tmp[MAX_ARGUMENTS_LENGTH];

  parameters[0] = '\0';

  if (argc < 5) {
    fprintf(stderr, "Not enough arguments, expected 6, got %d\n", argc);
    msibail();
  }

  const char* user = argv[2];
  const char* password = argv[3];
  const char* command = argv[4];

  for (int i = 4; i < argc; i++) {
    SAFE_APPEND("%s\"%s\" ", argv[i]);
  }

  WCHAR* wuser;
  WCHAR* wpassword;
  WCHAR* wcommand;
  WCHAR* wparameters;
  toWideChar(user, &wuser);
  toWideChar(password, &wpassword);
  toWideChar(command, &wcommand);
  toWideChar(parameters, &wparameters);

  HANDLE hToken;
  LPVOID lpvEnv;

  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

  STARTUPINFOW si;
  ZeroMemory(&si, sizeof(STARTUPINFOW));
  si.cb = sizeof(STARTUPINFOW);

  if (!LogonUserW(wuser, L".", wpassword, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken)) {
    wbail(127, "LogonUser");
  }

  if (!CreateEnvironmentBlock(&lpvEnv, hToken, TRUE)) {
    wbail(127, "CreateEnvironmentBlock");
  }

  if (!CreateProcessWithLogonW(wuser, L".", wpassword,
    LOGON_WITH_PROFILE, wcommand, wparameters,
    CREATE_UNICODE_ENVIRONMENT, lpvEnv, NULL,
    &si, &pi)) {
    wbail(127, "CreateProcessWithLogonW");
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD code;
  if (GetExitCodeProcess(pi.hProcess, &code) == 0) {
    // Not sure when this could ever happen.
    wbail(127, "failed GetExitCodeProcess call");
  }

  if (!DestroyEnvironmentBlock(lpvEnv)) {
    wbail(127, "failed DestroyEnvironmentBlock call");
  }

  CloseHandle(hToken);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return code;
}

/**
 * Inspired by / with some code from the following MIT-licensed projects:
 *   - https://github.com/atom/node-runas
 *   - https://github.com/jpassing/elevate
 */
int main(int argc, char** argv) {
  if (getMajorOSVersion() < VISTA_MAJOR_VERSION) {
    bail(1, "elevate requires Windows Vista or above");
  }

  if (argc < 2) {
    bail(1, "Usage: elevate PROGRAM ARGS");
  }

  if (strcmp("-V", argv[1]) == 0) {
    printf("%s\n", ELEVATE_VERSION);
    return 0;
  }

  if (strcmp("--msiexec", argv[1]) == 0) {
    return msiexec(argc, argv);
  } else if (strcmp("--runas", argv[1]) == 0) {
    return runas(argc, argv);
  } else {
    return elevate(argc, argv);
  }
}
