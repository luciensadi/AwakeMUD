// file: file.h
// author: Andrew Hynek
// contents: a buffer-friendly and graceful error-handling file class

#ifndef __file_h__
#define __file_h__

//struct FILE;

class File
{
  static const int MAX_FILE_LENGTH = 128;

  FILE *fl;
  char filename[MAX_FILE_LENGTH];
  char mode[8];
  int line_num;

public:
  File();
  File(const char *_filename, const char *_mode);
  ~File();

  // access/query functions
  //
  const char *Filename() const
  {
    return filename;
  }
  // const char *Mode() const { return mode; }
  int LineNumber() const  // only working for reading
  {
    return line_num;
  }

  bool IsOpen() const
  {
    return (fl != NULL);
  }
  // inline bool IsWriteable() const;

  bool EoF() const;

  // mutation functions
  //
  bool Open(const char *_filename, const char *_mode);
  bool Close();

  void Rewind();

  // '*' at the beginning of a line marks it as a comment,
  // strips the newline,
  bool GetLine(char *buf, size_t buf_size, bool blank);

  // reads a '~'-terminated string from the file, returns a newly-allocated
  // string.  Much like fread_string, only better.
  char *ReadString(const char* section);
};

#endif // ifndef __file_h__
