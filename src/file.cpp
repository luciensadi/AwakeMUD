// file: file.cpp
// author: Andrew Hynek
// contents: implementation of a simple file class

#include <stdio.h>
#include <string.h>
#include <time.h>   // for awake.h (time_t)

#include "file.hpp"
#include "awake.hpp"  // for MAX_STRING_LENGTH
#include "utils.hpp"  // for log()
#include <new>

File::File()
{
  fl = NULL;
  *filename = '\0';
  *mode = '\0';
}

File::File(const char *_filename, const char *_mode)
{
  fl = NULL;
  *filename = '\0';
  *mode = '\0';

  Open(_filename, _mode);
}

File::~File()
{
  Close();
}

bool File::EoF() const
{
  return feof(fl);
}

bool File::IsWriteable() const
{
  return (IsOpen() && (strchr(mode, 'w') || strchr(mode, 'a')));
}

bool File::Open(const char *_filename, const char *_mode)
{
  Close();

  fl = fopen(_filename, _mode);

  if (!fl)
    return false;

  memset(filename, 0, MAX_FILE_LENGTH);
  strncpy(filename, _filename, MAX_FILE_LENGTH);

  memset(mode, 0, 8);
  strncpy(mode, _mode, 8);

  line_num = 0;

  return true;
}

bool File::Close()
{
  if (IsOpen()) {
    fclose(fl);
    fl = NULL;
    *filename = '\0';
    *mode = '\0';

    return true;
  }

  return false;
}

void File::Rewind()
{
  rewind(fl);
  line_num = 0;
}

int  File::Seek(long offset, int origin)
{
  return fseek(fl, offset, origin);
}

bool File::GetLine(char *buf, size_t buf_size, bool blank)
{
  while (!feof(fl)) {
    fgets(buf, buf_size, fl);
    line_num++;

    if (*buf && *buf != '*' && (blank ? TRUE : *buf != '\n')) {
      char *ptr = strchr(buf, '\n');

      if (ptr && ptr < (buf + buf_size))
        *ptr = '\0';

      // Add a fix for \r character
      ptr = strchr(buf, '\r');

      if (ptr && ptr < (buf + buf_size))
              *ptr = '\0';

      break;
    }
  }

  return (!feof(fl));
}

char *File::ReadString(const char* section)
{
  char buf[MAX_STRING_LENGTH+8];
  char line[512+8];
  size_t buf_len = 0;
  bool skip_to_end = false, done = false;

  memset(buf, 0, MAX_STRING_LENGTH+8);

  while (!done) {
    if (!GetLine(line, 512, FALSE)) {
      log_vfprintf("FATAL ERROR: format error in %s, line %d: expecting ~-terminated string for section '%s'.",
          Filename(), LineNumber(), section);
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
      break;
    }

    char *ptr = strchr(line, '~');

    // if we find '~', replace it with '\0' and break, otherwise cat "\r\n"
    if (ptr) {
      *ptr = '\0';
      done = true;
    } else
      strlcat(line, "\r\n", sizeof(line));

    size_t line_len = strlen(line);

    if ((buf_len + line_len) >= (size_t)MAX_STRING_LENGTH) {
      // we can handle this gracefully: just fill buf, then set skip_to_end

      log_vfprintf("Error: string too long (%s, line %d) -- truncating section '%s'.",
          Filename(), LineNumber(), section);

      strncpy(buf+buf_len, line, MAX_STRING_LENGTH - buf_len);
      *(buf+MAX_STRING_LENGTH) = '\0';
      strlcat(buf, "\r\n", sizeof(buf));

      // if line doesn't have the '~', then we need to find it later
      skip_to_end = (strchr(line, '~') == NULL);

      break;
    }

    strlcat(buf, line, sizeof(buf));
    buf_len += line_len;
  }

  if (skip_to_end) {
    // if we're here, we need to find the '~'
    while (!EoF()) {
      if (!GetLine(line, 512, FALSE)) {
        log_vfprintf("Error: format error in %s, line %d: expecting ~-termed string for section '%s'.",
            Filename(), LineNumber(), section);
        break;
      }

      if (strchr(line, '~') != NULL)
        break;
    }
  }

  if (buf_len > 0) {
    size_t res_sz = buf_len+1;
    char *res = new char[res_sz];
    strlcpy(res, buf, res_sz);

    return res;
  }

  return NULL;
}

int  File::Print(const char *format, ...)
{
#ifdef vfprintf

  va_list arg_list;

  va_start(arg_list, format);
  int res = vfprintf(fl, format, arg_list);
  va_end(arg_list);

  return res;
#else

  return 0;
#endif
}
