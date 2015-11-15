
#include <stdio.h>
#include <windows.h>

#ifndef SEE_MASK_NOASYNC
#define SEE_MASK_NOASYNC 0x00000100
#endif

// cf. https://www.msigeek.com/442/windows-os-version-numbers - Vista is 6.0
#define VISTA_MAJOR_VERSION 6

static void bail(int code, char *msg) {
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

  if (strcmp("-v", argv[1]) == 0) {
    printf("elevate version %s\n", ELEVATE_VERSION);
    return 0;
  }

  const int MAX_ARGUMENTS_LENGTH = 65536;
  char parameters[MAX_ARGUMENTS_LENGTH];
  char tmp[MAX_ARGUMENTS_LENGTH];

  parameters[0] = '\0';

  for (int i = 2; i < argc; i++) {
    int written = snprintf(tmp, MAX_ARGUMENTS_LENGTH, "%s%s ", parameters, argv[i]);
    if (written < 0 || written >= MAX_ARGUMENTS_LENGTH) {
      bail(1, "argument string too long");
    }

    strncpy(parameters, tmp, MAX_ARGUMENTS_LENGTH);
  }

  fprintf(stderr, "[elevate] %s %s\n", argv[1], parameters);
  fflush(stderr);

  SHELLEXECUTEINFO sei;
  ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
  sei.cbSize = sizeof(SHELLEXECUTEINFO);
  // we want to wait until the program exits to exit with its code
  sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
  // would be "open" if we didn't need privilege elevation
  sei.lpVerb = "runas";
  sei.lpFile = argv[1];
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
