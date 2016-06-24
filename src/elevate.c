
#include <stdio.h>
#include <windows.h>
#include <sddl.h>
#include <userenv.h>
#include <shlobj.h>

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

static void ebail(int code, char *msg, HRESULT err) {
  LPVOID lpvMessageBuffer;

  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
    FORMAT_MESSAGE_FROM_SYSTEM,
    NULL, err, 
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
    (LPWSTR)&lpvMessageBuffer, 0, NULL);

  printf("API = %s.\n", msg);
  wprintf(L"error code = %d.\n", err);
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

  if (!LogonUserW(wuser, L".", wpassword,
	LOGON32_LOGON_INTERACTIVE,
	LOGON32_PROVIDER_DEFAULT,
	&hToken)) {
    wbail(127, "LogonUser");
  }

  if (!CreateEnvironmentBlock(&lpvEnv, hToken, TRUE)) {
    CloseHandle(hToken);
    wbail(127, "CreateEnvironmentBlock");
  }

  PROFILEINFOW profileInfo;
  ZeroMemory(&profileInfo, sizeof(profileInfo));
  profileInfo.dwSize = sizeof(profileInfo);
  profileInfo.lpUserName = L"itch-player";

  DWORD shBufSize = 2048;
  wchar_t *shBuf = malloc(sizeof(wchar_t) * shBufSize);

  if (!ImpersonateLoggedOnUser(hToken)) {
    CloseHandle(hToken);
    wbail(127, "ImpersonateLoggedOnUser");
  }

  HRESULT shRes = SHGetFolderPathW(NULL, CSIDL_PROFILE | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, shBuf);
  if (FAILED(shRes)) {
    CloseHandle(hToken);
    ebail(127, "SHGetFolderPathW (1)", shRes);
  }
  wprintf(L"User profile dir = %s\n", shBuf);

  if (!RevertToSelf()) {
    CloseHandle(hToken);
    wbail(127, "ImpersonateLoggedOnUser");
  }

  CloseHandle(hToken);
 
  DWORD envLen;
  wchar_t *ptr = lpvEnv;
  while (1) {
#ifdef DEBUG
    printf(L"[ENV] %s\n", ptr);
#endif
    while ((*ptr) != '\0') {
      ptr++;
    }
    ptr++;

    if (*ptr == '\0') {
      envLen = (DWORD) ((LPVOID) ptr - (LPVOID) lpvEnv);
      wprintf(L"Total environment length: %d\n", envLen);
      break;
    }
  }

  wchar_t *envAddition = L"USERPROFILE=";
  wchar_t *terminator = L"\0";

  DWORD envBufSize = envLen + (sizeof(wchar_t) * (wcslen(envAddition) + wcslen(shBuf) + 1));
  wprintf(L"Environment buffer size: %d\n", envBufSize);

  LPVOID lpvManipEnv = malloc(sizeof(wchar_t) * envBufSize); 
  ZeroMemory(lpvManipEnv, sizeof(wchar_t) * envBufSize);

  BYTE *currentManipEnvSz = lpvManipEnv;

  memcpy(currentManipEnvSz, envAddition, sizeof(wchar_t) * (wcslen(envAddition)));
  currentManipEnvSz += sizeof(wchar_t) * (wcslen(envAddition));

  memcpy(currentManipEnvSz, shBuf, sizeof(wchar_t) * (wcslen(shBuf)));
  currentManipEnvSz += sizeof(wchar_t) * (wcslen(shBuf));
  free(shBuf);

  memcpy(currentManipEnvSz, terminator, sizeof(wchar_t));
  currentManipEnvSz += sizeof(wchar_t) * 1;

  memcpy(currentManipEnvSz, lpvEnv, envLen);
  currentManipEnvSz += envLen;

  if (!DestroyEnvironmentBlock(lpvEnv)) {
    wbail(127, "failed DestroyEnvironmentBlock call");
  }

  memcpy(currentManipEnvSz, terminator, sizeof(wchar_t));

  ptr = lpvManipEnv;
  while (1) {
#ifdef DEBUG
    wprintf(L"[MENV] %s\n", ptr);
#endif
    while ((*ptr) != '\0') {
      ptr++;
    }
    ptr++;

    if (*ptr == '\0') {
      break;
    }
  }

  wchar_t *ExePath;
  toWideChar(argv[4], &ExePath);
  wprintf(L"ExePath = '%s'\n", ExePath);

  wchar_t *Drive = malloc(sizeof(wchar_t) * MAX_PATH);
  Drive[0] = (wchar_t) '\0';

  wchar_t *DirName = malloc(sizeof(wchar_t) * MAX_PATH);
  DirName[0] = (wchar_t) '\0';

  wchar_t *DirPath = malloc(sizeof(wchar_t) * MAX_PATH);
  DirPath[0] = (wchar_t) '\0';

  _wsplitpath(ExePath, Drive, DirName, NULL, NULL);

  if (wcslen(DirName) == 0) {
    GetCurrentDirectoryW(MAX_PATH, DirPath);
  } else {
    wsprintfW(DirPath, L"%s%s", Drive, DirName);
  }

  int LenDirPath = wcslen(DirPath);
  if (DirPath[LenDirPath - 1] == '\\') {
    DirPath[LenDirPath - 1] = '\0';
  }

  wprintf(L"ExePath = '%s'\n", ExePath);
  wprintf(L"DirPath = '%s'\n", DirPath);

  if (!CreateProcessWithLogonW(wuser, L".", wpassword,
    LOGON_WITH_PROFILE, wcommand, wparameters,
    CREATE_UNICODE_ENVIRONMENT,
    lpvManipEnv,
    DirPath,
    &si, &pi)) {
    wbail(127, "CreateProcessWithLogonW");
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD code;
  if (GetExitCodeProcess(pi.hProcess, &code) == 0) {
    // Not sure when this could ever happen.
    wbail(127, "failed GetExitCodeProcess call");
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return code;
}

int printUsersSid () {
  DWORD arbitrarySize = 2048;

  DWORD sidSize = arbitrarySize;
  PSID sid = malloc(sidSize);
  memset(sid, 0, sidSize);

  int ret = CreateWellKnownSid(WinBuiltinUsersSid, NULL, sid, &sidSize);
  if (!ret) {
    wbail(127, "failed CreateWellKnownSid call");
  }

  DWORD cchName = arbitrarySize;
  wchar_t *lpName = malloc(sizeof(wchar_t) * cchName);

  DWORD cchReferencedDomainName = arbitrarySize;
  wchar_t *lpReferencedDomainName = malloc(sizeof(wchar_t) * cchReferencedDomainName);

  SID_NAME_USE sidUse;

  ret = LookupAccountSidW(NULL, sid, lpName, &cchName, lpReferencedDomainName, &cchReferencedDomainName, &sidUse);
  if (!ret) {
    wbail(127, "failed LookupAccountSid call");
  }

  wprintf(L"%s\n", lpName);

  return 0;
}

/**
 * Inspired by / with some code from the following MIT-licensed projects:
 *   - https://github.com/atom/node-runas
 *   - https://github.com/jpassing/elevate
 */
int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

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
  } else if (strcmp("--print-users-sid", argv[1]) == 0) {
    return printUsersSid(argc, argv);
  } else {
    return elevate(argc, argv);
  }
}
