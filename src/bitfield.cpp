// file: bitfield.cpp
// author: Andrew Hynek
// contents: implementation of the Bitfield class

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "bitfield.hpp"
#include "structs.hpp"
#include "utils.hpp"

// Explicitly inclusive of ENDBIT, that's a valid offset.
#define OFFSET_IS_VALID(offset) (offset >= 0 && offset <= ENDBIT)

// ______________________________
//
// static vars
// ______________________________

static const int NUM_BUFFERS = 8;
// I wish I could use (Bitfield::TotalWidth()+1), but that's not allowed..
static const int BUFFER_SIZE = 256;
static char buffer_tab[NUM_BUFFERS][BUFFER_SIZE];
static int  buffer_idx = 0;

// Byte bits-set lookup function, because why the hell are we using so many for-loops for a bitfield?
uint8_t count_ones (uint8_t byte)
{
  static const int lookupTable[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
  };

  return lookupTable[byte];
}

// ______________________________
//
// Bitfield()
// ______________________________

Bitfield::Bitfield()
{
  Clear();
}
/*
Bitfield::Bitfield(dword offset)
{
  Clear();
  SetBit(offset);
}
*/

Bitfield::Bitfield(unsigned long long from) {
  Clear();
  for (size_t i = 0; i < sizeof(from) * 8; i++) {
    if (from & (1ULL << i))
      SetBit(i);
  }
}

// ______________________________
//
// testing funcs
// ______________________________

#define BITFIELD_IDX (offset / bits_per_var)
#define BITFIELD_FLAG (offset % bits_per_var)
bool Bitfield::IsSet(dword offset) const
{
  return (data[BITFIELD_IDX] & (1 << BITFIELD_FLAG));
}
#undef BITFIELD_IDX
#undef BITFIELD_FLAG

bool Bitfield::IsSetPrecomputed(int field, int flags) const
{
  return (data[field] & flags);
}

bool Bitfield::AreAnySet(dword one, ...) const
{
  va_list arg_list;
  dword offset;

  if (one == ENDBIT)
    return false;

  if (!OFFSET_IS_VALID(one)) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid initial offset bit %d to Bitfield::AreAnySet().", one);
    return false;
  }

  if (IsSet(one))
    return true;

  va_start(arg_list, one);

  while (true) {
    offset = va_arg(arg_list, dword);

    if (!OFFSET_IS_VALID(offset)) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid initial offset bit %d to Bitfield::AreAnySet().", offset);
      va_end(arg_list);
      return false;
    } else if (offset == ENDBIT) {
      va_end(arg_list);
      return false;
    } else if (IsSet(offset)) {
      va_end(arg_list);
      return true;
    }
  }
}

int  Bitfield::GetNumSet() const
{
  int count = 0;

  for (int i = 0; i < BITFIELD_SIZE; i++) {
    // Convert the unsigned long into bytes.
    unsigned char *bytePtr = (unsigned char *)&(data[i]);
    for (size_t idx = 0; idx < sizeof(bitfield_t); idx++) {
      count += count_ones(bytePtr[idx]);
    }
  }

  return count;
}

bool Bitfield::HasAnythingSetAtAll() const
{
  for (int i = 0; i < BITFIELD_SIZE; i++)
    if (data[i] != 0)
      return TRUE;

  return FALSE;
}

bool Bitfield::AreAnyShared(const Bitfield &test) const
{
  for (int i = 0; i < BITFIELD_SIZE; i++)
    if (data[i] & test.data[i])
      return true;

  return false;
}

int  Bitfield::GetNumShared(const Bitfield &test) const
{
  int count = 0;

  for (int i = 0; i < BITFIELD_SIZE; i++) {
    // Convert the unsigned long into bytes.
    unsigned char *firstBytePtr = (unsigned char *)&(data[i]);
    unsigned char *secondBytePtr = (unsigned char *)&(test.data[i]);
    for (size_t idx = 0; idx < sizeof(bitfield_t); idx++) {
      count += count_ones(firstBytePtr[idx] & secondBytePtr[idx]);
    }
  }

  return count;
}

bool Bitfield::operator==(const Bitfield &two) const
{
  return (memcmp(data, two.data, BITFIELD_SIZE*sizeof(bitfield_t)) == 0);
}

bool Bitfield::operator!=(const Bitfield &two) const
{
  return (memcmp(data, two.data, BITFIELD_SIZE*sizeof(bitfield_t)) != 0);
}

void Bitfield::Clear()
{
  memset(data, 0, BITFIELD_SIZE*sizeof(bitfield_t));
}

void Bitfield::SetBit(dword offset)
{
  //const int idx = offset / bits_per_var;
  //const int flag = offset % bits_per_var;

  data[(offset / bits_per_var)] |= (1 << (offset % bits_per_var));
}

void Bitfield::SetBits(dword one, ...)
{
  va_list arg_list;
  dword offset;

  if (!OFFSET_IS_VALID(one)) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid initial offset bit %d to Bitfield::SetBits().", one);
    return;
  }

  if (one == ENDBIT)
    return;

  SetBit(one);

  va_start(arg_list, one);

  while (true) {
    offset = va_arg(arg_list, dword);

    if (!OFFSET_IS_VALID(offset)) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid offset bit %d to Bitfield::AreAnySet().", offset);
      break;
    }
    else if (offset == ENDBIT) {
      break;
    }
    else {
      SetBit(offset);
    }
  }

  va_end(arg_list);
}

void Bitfield::SetAll(const Bitfield &two)
{
  for (int i = 0; i < BITFIELD_SIZE; i++)
    data[i] |= two.data[i];
}

void Bitfield::RemoveBit(dword offset)
{
  //const int idx = offset / bits_per_var;
  //const int flag = offset % bits_per_var;

  data[(offset / bits_per_var)] &= ~(1 << (offset % bits_per_var));
}

void Bitfield::RemoveBits(dword one, ...)
{
  va_list arg_list;
  dword offset;

  if (one == ENDBIT) {
    return;
  }

  if (!OFFSET_IS_VALID(one)) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid initial offset bit %d to Bitfield::RemoveBits().", one);
    return;
  }

  RemoveBit(one);

  va_start(arg_list, one);

  while (true) {
    offset = va_arg(arg_list, dword);

    if (!OFFSET_IS_VALID(offset)) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid offset bit %d to Bitfield::SetBits().", offset);
      break;
    } else if (offset == ENDBIT) {
      break;
    } else {
      RemoveBit(offset);
    }
  }

  va_end(arg_list);
}

void Bitfield::RemoveAll(const Bitfield &two)
{
  for (int i = 0; i < BITFIELD_SIZE; i++)
    data[i] &= ~two.data[i];
}

void Bitfield::ToggleBit(dword offset)
{
  //const int idx = offset / bits_per_var;
  //const int flag = offset % bits_per_var;

  data[(offset / bits_per_var)] ^= (1 << (offset % bits_per_var));
}

void Bitfield::ToggleBits(dword one, ...)
{
  va_list arg_list;
  dword offset;

  if (!OFFSET_IS_VALID(one)) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid initial offset bit %d to Bitfield::ToggleBits().", one);
    return;
  }

  if (one == ENDBIT)
    return;

  ToggleBit(one);

  va_start(arg_list, one);

  while (true) {
    offset = va_arg(arg_list, dword);

    if (!OFFSET_IS_VALID(offset)) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid offset bit %d to Bitfield::ToggleBits().", offset);
      break;
    } else if (offset == ENDBIT) {
      break;
    } else {
      ToggleBit(offset);
    }
  }

  va_end(arg_list);
}

const char *Bitfield::ToString() const
{
  char *buffer = buffer_tab[buffer_idx];
  char *buf_ptr = buffer;

  if (++buffer_idx >= NUM_BUFFERS)
    buffer_idx = 0;

  for (int i = TotalWidth()-1; i >= 0; i--)
    if (IsSet(i))
      *(buf_ptr++) = '1';
    else if (buf_ptr != buffer)
      *(buf_ptr++) = '0';

  if (buf_ptr == buffer) {
    // if no flags were set, just set it to "0":
    strcpy(buffer, "0");
  } else
    *buf_ptr = '\0';

  return (const char *)buffer;
}

void Bitfield::FromString(const char *str)
{
  const size_t len = strlen(str);
  dword offset = 0;

  Clear();

  if (!len)
    return;

  const char *str_ptr = str + len - 1;

  while (true) {
    if (*str_ptr != '0')
      SetBit(offset);

    // if we're already at the beginning of the string, break
    if (str_ptr == str)
      break;
    str_ptr--;

    // if we can't hold any more bits, just break
    if (++offset >= ENDBIT)
      break;
  }
}

void Bitfield::PrintBits(char *dest, size_t dest_size, const char *names[], size_t name_cnt) {
  return PrintBitsColorized(dest, dest_size, names, name_cnt, "", "");
}

void Bitfield::PrintBitsColorized(char *dest, size_t dest_size, const char *names[], size_t name_cnt, const char *color_on, const char *color_off)
{
  size_t len = 0;
  int left = dest_size - 1;
  bool first = true;

  *dest = '\0';

  for (int i = 0; i < TotalWidth() && left > 0; i++)
    if (IsSet(i)) {
      size_t written;

      if ((unsigned)i < name_cnt && *names[i] != '\n')
        written = snprintf(dest + len, left,
                           "%s%s%s%s", first? "" : ", ", color_on, names[i], color_off);
      else
        written = snprintf(dest + len, left,
                           "%s%sUNDEFINED%s", first? "" : ", ", color_on, color_off);

      left -= written;
      len += written;

      if (first)
        first = false;
    }

  if (left < 1) {
    strncpy(dest, "<TRUNCATED>", dest_size);
    len = MIN(dest_size-1, 11);
  }

  *(dest+len) = '\0';
}
