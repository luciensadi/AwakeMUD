// file: bitfield.h
// author: Andrew Hynek
// contents: definition of the Bitfield class, a 128-bit bitvector

#ifndef __bitfield_h__
#define __bitfield_h__

#include <assert.h>

#include "types.hpp"

typedef unsigned int bitfield_t;
#define BITFIELD_BITS_PER_VAR (sizeof(bitfield_t) * 8)

const dword ENDBIT = 127;
class Bitfield
{
  static const int BITFIELD_SIZE = 4;
  static const int bits_per_var = BITFIELD_BITS_PER_VAR;
  static const int total_width = bits_per_var*BITFIELD_SIZE;

  bitfield_t data[BITFIELD_SIZE];

public:
  Bitfield();
  Bitfield(dword offset);

  // testing funcs
  //
  bool IsSet(dword offset) const;
  bool IsSetPrecomputed(int field, int flags) const;

  bool AreAnySet(dword one, ...) const;
  int  GetNumSet() const;
  bool HasAnythingSetAtAll() const;

  bool AreAnyShared(const Bitfield &test) const;
  int  GetNumShared(const Bitfield &test) const;


  bool operator==(const Bitfield &test) const;
  bool operator!=(const Bitfield &test) const;

  // mutation funcs
  //
  void Clear();

  void SetBit(dword offset);
  void SetBits(dword one, ...);    // note: list _must_ be ENDBIT-terminated
  void SetAll(const Bitfield &two);

  void RemoveBit(dword offset);
  void RemoveBits(dword one, ...); // must also be ENDBIT-terminated
  void RemoveAll(const Bitfield &two);

  void ToggleBit(dword offset);
  void ToggleBits(dword one, ...); // ENDBIT-terminated

  // conversion funcs
  //
  const char *ToString() const;    // uses 8 internal buffers..
  void FromString(const char *str); // non-'0' assumed to be set;
  // assumes leading '0's if length < expected

  // prints a comma-separated list of names
  void PrintBits(char *dest, size_t dest_size, const char *names[], size_t name_cnt);
  // Same as above, but adds color to each bit
  void PrintBitsColorized(char *dest, size_t dest_size, const char *names[], size_t name_cnt, const char *color_on, const char *color_off);


  // static TotalWidth() - total number of bits contained
  static int TotalWidth()
  {
    return total_width;
  }
};

#endif // ifndef __bitfield_h__
