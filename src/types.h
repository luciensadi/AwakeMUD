// file: types.h
// author: Andrew Hynek
// contents: basic type definitions

#ifndef __types_h__
#define __types_h__

// gah...don't like em, but they're nice for backwards compatibility
typedef signed char             sbyte;
typedef unsigned char           ubyte;
typedef signed short int        sh_int;
typedef unsigned short int      ush_int;
#if !defined(WIN32) || defined(__CYGWIN_)
typedef char                    byte;
#else
typedef unsigned char u_char;
typedef unsigned short int u_short;
typedef unsigned int u_int;
typedef unsigned char                    byte;
#endif

typedef signed long rnum_t;
typedef signed long vnum_t;

typedef unsigned short  word;
typedef unsigned int  dword;

typedef signed char dir_t;

#endif // ifndef __types_h__
