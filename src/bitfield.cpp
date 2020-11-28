// file: bitfield.cpp
// author: Andrew Hynek
// contents: implementation of the Bitfield class

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "bitfield.h"
#include "structs.h"
#include "utils.h"

// ______________________________
//
// static vars
// ______________________________

static const int NUM_BUFFERS = 8;
// I wish I could use (Bitfield::TotalWidth()+1), but that's not allowed..
static const int BUFFER_SIZE = 256;
static char buffer_tab[NUM_BUFFERS][BUFFER_SIZE];
static int  buffer_idx = 0;
static const int LOOP_BREAK = 100;

// ______________________________
//
// Bitfield()
// ______________________________

Bitfield::Bitfield()
{
  Clear();
}

Bitfield::Bitfield(dword offset)
{
  Clear();
  SetBit(offset);
}

// ______________________________
//
// testing funcs
// ______________________________

bool Bitfield::IsSet(dword offset) const
{
  const int idx = offset / bits_per_var;
  const int flag = offset % bits_per_var;

  return (data[idx] & (1 << flag));
}

bool Bitfield::AreAnySet(dword one, ...) const
{
  va_list arg_list;
  dword offset;

  if (one == ENDBIT)
    return false;

  if (IsSet(one))
    return true;

  va_start(arg_list, one);

  while (true) {
    offset = va_arg(arg_list, dword);

    if (offset == ENDBIT) {
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

  for (int i = 0; i < TotalWidth(); i++)
    if (IsSet(i))
      count++;

  return count;
}

bool Bitfield::AreAnyShared(const Bitfield &test) const
{
  for (int i = 0; i < TotalWidth(); i++)
    if (IsSet(i) && test.IsSet(i))
      return true;

  return false;
}

int  Bitfield::GetNumShared(const Bitfield &test) const
{
  int count = 0;

  for (int i = 0; i < BITFIELD_SIZE; i++)
    for (int j = 0; j < bits_per_var; j++) {
      const int flag = 1 << j;

      if ((test.data[i] & flag) && (this->data[i] & flag))
        count++;
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
  const int idx = offset / bits_per_var;
  const int flag = offset % bits_per_var;

  data[idx] |= (1 << flag);
}

void Bitfield::SetBits(dword one, ...)
{
  va_list arg_list;
  dword offset;

  if (one == ENDBIT)
    return;

  SetBit(one);

  va_start(arg_list, one);

  while (true) {
    offset = va_arg(arg_list, dword);

    if (offset != ENDBIT)
      SetBit(offset);
    else
      break;
  }

  va_end(arg_list);
}

void Bitfield::SetAll(const Bitfield &two)
{
  for (int i = 0; i < TotalWidth(); i++)
    if (two.IsSet(i) && i > 0)
      SetBit(i);
}

void Bitfield::RemoveBit(dword offset)
{
  const int idx = offset / bits_per_var;
  const int flag = offset % bits_per_var;

  data[idx] &= ~(1 << flag);
}

void Bitfield::RemoveBits(dword one, ...)
{
  va_list arg_list;
  dword offset;

  if (one == ENDBIT)
    return;

  RemoveBit(one);

  va_start(arg_list, one);

  while (true) {
    offset = va_arg(arg_list, dword);

    if (offset != ENDBIT)
      RemoveBit(offset);
    else
      break;
  }

  va_end(arg_list);
}

void Bitfield::RemoveAll(const Bitfield &two)
{
  for (int i = 0; i < TotalWidth(); i++)
    if (IsSet(i) && two.IsSet(i))
      RemoveBit(i);
}

void Bitfield::ToggleBit(dword offset)
{
  const int idx = offset / bits_per_var;
  const int flag = offset % bits_per_var;

  data[idx] ^= (1 << flag);
}

void Bitfield::ToggleBits(dword one, ...)
{
  va_list arg_list;
  dword offset;

  if (one == ENDBIT)
    return;

  ToggleBit(one);

  va_start(arg_list, one);

  while (true) {
    offset = va_arg(arg_list, dword);

    if (offset != ENDBIT)
      ToggleBit(offset);
    else
      break;
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

void Bitfield::PrintBits(char *dest, size_t dest_size,
                         const char *names[], size_t name_cnt)
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
                           "%s%s", first? "" : ", ", names[i]);
      else
        written = snprintf(dest + len, left,
                           "%sUNDEFINED", first? "" : ", ");

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

