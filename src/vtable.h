// file: vtable.h
// author: Andrew Hynek
// contents: a value table (VTable) class

#ifndef __vtable_h__
#define __vtable_h__

class File;

class VTable
{
  static const int MAX_SECTION_LENGTH = 32;
  static const int MAX_FIELD_LENGTH = 32;
  static const int MAX_LINE_LENGTH = 256;

  struct field
  {
    char name[MAX_FIELD_LENGTH];
    char line[MAX_LINE_LENGTH];
    char *multiline;

    field() : multiline(NULL)
    {
      *name = '\0';
      *line = '\0';
    }

    ~field();

    field &operator=(const field &two)
    {
      // do a deep copy
      strcpy(name, two.name);
      strcpy(line, two.line);

      if (two.multiline) {
        if (!multiline) {
          multiline = new char[strlen(two.multiline)+1];
          strcpy(multiline, two.multiline);
        } else if (strcmp(multiline, two.multiline)) {
          delete [] multiline;
          multiline = new char[strlen(two.multiline)+1];
          strcpy(multiline, two.multiline);
        }
      } else if (multiline) {
        delete [] multiline;
        multiline = NULL;
      }

      return (*this);
    }
  };

  struct section
  {
    char name[MAX_SECTION_LENGTH]; // NULL for top
    section *parent;

    section *sub_tab;
    int sub_cnt;
    int sub_size;

    field *field_tab;
    int field_cnt;
    int field_size;

    section() : parent(NULL),
        sub_tab(NULL), sub_cnt(0), sub_size(0),
        field_tab(NULL), field_cnt(0), field_size(0)
    {
      *name = '\0';
    }

    ~section();

    section &operator=(const section &two)
    {
      strcpy(name, two.name);

      parent = two.parent;

      if (two.sub_tab && two.sub_cnt > 0) {
        sub_size = two.sub_size;
        sub_cnt = two.sub_cnt;

        if (sub_tab)
          delete [] sub_tab;

        sub_tab = new section[sub_size];

        for (int i = 0; i < sub_cnt; i++)
          sub_tab[i] = two.sub_tab[i];
      } else {
        sub_cnt = 0;
        sub_size = 0;

        if (sub_tab) {
          delete [] sub_tab;
          sub_tab = NULL;
        }
      }

      if (two.field_tab && two.field_cnt > 0) {
        field_size = two.field_size;
        field_cnt = two.field_cnt;

        if (field_tab)
          delete [] field_tab;

        field_tab = new field[field_size];

        for (int i = 0; i < field_cnt; i++)
          field_tab[i] = two.field_tab[i];
      } else {
        field_cnt = 0;
        field_size = 0;

        if (field_tab) {
          delete [] field_tab;
          field_tab = NULL;
        }
      }

      return (*this);
    }
  };

  section top;

public:
  int  NumSections() const;
  int  NumSubsections(const char *sect);
  int  NumFields(const char *sect_name);

  bool DoesSectionExist(const char *sect);
  bool DoesFieldExist(const char *where);

  // returns the number of fields successfully parsed
  int Parse(File *in);

  int GetInt(const char *where, int defawlt);
  long GetLong(const char *where, long defawlt);

  // assumes lookup_tab is "\n"-terminated, returns index of string in tab
  int LookupInt(const char *where, const char **lookup_tab, int defawlt);

  const char *GetString(const char *where, const char *defawlt);

  float GetFloat(const char *where, float defawlt);

  // returns a pointer (don't delete) to field name of nth field in section
  const char *GetIndexField(const char *sect_name, int n);

  // like GetIndexField, but for subsection names
  const char *GetIndexSection(const char *sect_name, int n);

  // returns the int of the nth field in section
  int GetIndexInt(const char *sect_name, int n, int defawlt);

  const char *GetIndexString(const char *sect_name, int n,
                             const char *defawlt);
  // float GetIndexFloat();

  // bool AddInt(const char *where, int n);
  // bool AddString(const char *where, const char *str);
  // bool AddFloat(const char *where, float f);

private:
  static void separate(const char *where,
                       char section[MAX_SECTION_LENGTH],
                       char field[MAX_FIELD_LENGTH]);

  void resize_subsection_tab(section *ptr, int empty = 5);
  void resize_field_tab(section *ptr, int empty = 5);

  field *find_field(const char *where);
  section *find_section(const char *sect);
  section *look_for_section(section *ptr, const char *name);

  int count_subsections(const section *ptr) const;
};

#endif // ifndef __vtable_h__
