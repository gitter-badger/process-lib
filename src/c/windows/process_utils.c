#include "process_utils.h"

#include <assert.h>

#if defined(HAS_ATTRIBUTE_LIST)
#include <stdlib.h>

PROCESS_LIB_ERROR
static handle_inherit_list_create(HANDLE *handles, int amount,
                                  LPPROC_THREAD_ATTRIBUTE_LIST *result)
{
  assert(handles);
  assert(amount >= 0);
  assert(result);

  // Get the required size for the attribute list
  SIZE_T attribute_list_size = 0;
  SetLastError(0);
  if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_list_size) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = malloc(attribute_list_size);
  if (!attribute_list) { return PROCESS_LIB_MEMORY_ERROR; }

  SetLastError(0);
  if (!InitializeProcThreadAttributeList(attribute_list, 1, 0,
                                         &attribute_list_size)) {
    free(attribute_list);
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  // Add the handles to be inherited to the attribute list
  SetLastError(0);
  if (!UpdateProcThreadAttribute(attribute_list, 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                 amount * sizeof(HANDLE), NULL, NULL)) {
    DeleteProcThreadAttributeList(attribute_list);
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  *result = attribute_list;

  return PROCESS_LIB_SUCCESS;
}
#endif

PROCESS_LIB_ERROR process_create(wchar_t *command_line,
                                 wchar_t *working_directory, HANDLE child_stdin,
                                 HANDLE child_stdout, HANDLE child_stderr,
                                 PROCESS_INFORMATION *info)
{
  assert(command_line);
  assert(child_stdin);
  assert(child_stdout);
  assert(child_stderr);
  assert(info);

  // Create each process in a new process group so we don't send CTRL-BREAK
  // signals to more than one child process in process_terminate.
  DWORD creation_flags = CREATE_NEW_PROCESS_GROUP;

#if defined(HAS_ATTRIBUTE_LIST)
  PROCESS_LIB_ERROR error;

  // To ensure no handles other than those necessary are inherited we use the
  // approach detailed in https://stackoverflow.com/a/2345126
  HANDLE to_inherit[3] = { child_stdin, child_stdout, child_stderr };
  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list;
  error = handle_inherit_list_create(to_inherit,
                                     sizeof(to_inherit) / sizeof(to_inherit[0]),
                                     &attribute_list);
  if (error) { return error; }

  creation_flags |= EXTENDED_STARTUPINFO_PRESENT;

  STARTUPINFOEXW extended_startup_info =
      { .StartupInfo = { .cb = sizeof(extended_startup_info),
                         .dwFlags = STARTF_USESTDHANDLES,
                         .hStdInput = child_stdin,
                         .hStdOutput = child_stdout,
                         .hStdError = child_stderr },
        .lpAttributeList = attribute_list };

  LPSTARTUPINFOW startup_info_address = &extended_startup_info.StartupInfo;
#else
  STARTUPINFOW startup_info = { .cb = sizeof(startup_info),
                                .dwFlags = STARTF_USESTDHANDLES,
                                .hStdInput = child_stdin,
                                .hStdOutput = child_stdout,
                                .hStdError = child_stderr }

  LPSTARTUPINFOW startup_info_address = &startup_info;
#endif

  // Child processes inherit error mode of their parents. To avoid child
  // processes creating error dialogs we set our error mode to not create error
  // dialogs temporarily.
  DWORD previous_error_mode = SetErrorMode(SEM_NOGPFAULTERRORBOX);

  SetLastError(0);
  BOOL result = CreateProcessW(NULL, command_line, NULL, NULL, TRUE,
                               creation_flags, NULL, working_directory,
                               startup_info_address, info);

  SetErrorMode(previous_error_mode);

#if defined(HAS_ATTRIBUTE_LIST)
  DeleteProcThreadAttributeList(attribute_list);
#endif

  if (!result) {
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND: return PROCESS_LIB_FILE_NOT_FOUND;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  return PROCESS_LIB_SUCCESS;
}
