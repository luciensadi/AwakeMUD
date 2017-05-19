// file: vtable.cpp
// author: Andrew Hynek
// contents: implementation of the VTable class

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "vtable.h"
#include "utils.h"    // for str_cmp(), log()

VTable::field::~field()
{
  if (multiline) {
    delete [] multiline;
    multiline = NULL;
  }
}

VTable::section::~section()
{
  *name = '\0';
  parent = NULL;

  if (sub_tab) {
    delete [] sub_tab;
    sub_tab = NULL;
  }
  sub_cnt = 0;
  sub_size = 0;

  if (field_tab) {
    delete [] field_tab;
    field_tab = NULL;
  }
  field_cnt = 0;
  field_size = 0;
}

int  VTable::NumSections() const
{
  return (count_subsections(&top)+1);
}

int  VTable::NumSubsections(const char *sect)
{
  section *ptr = find_section(sect);

  if (!ptr)
    return -1;

  return ptr->sub_cnt;
}

int  VTable::NumFields(const char *sect)
{
  section *ptr = find_section(sect);

  if (!ptr)
    return -1;

  return ptr->field_cnt;
}

bool VTable::DoesSectionExist(const char *sect)
{
  return (find_section(sect) != NULL);
}

bool VTable::DoesFieldExist(const char *where)
{
  return (find_field(where) != NULL);
}

#define SKIP { in->GetLine(line, MAX_LINE_LENGTH*2, FALSE); continue; }

int  VTable::Parse(File *in)
{
  char line[MAX_LINE_LENGTH*2];
  int depth = 0, total_fields = 0;
  section *sec_ptr = &top;

  in->GetLine(line, MAX_LINE_LENGTH*2, FALSE);
  while (!in->EoF() && str_cmp(line, "BREAK")) {
    char *line_ptr = line;
    int d;
    bool multiline_input = false;

    // skip spaces
    while (*line_ptr && *line_ptr == ' ')
      line_ptr++;

    // calculate depth (1 per leading tab)
    d = 0;
    while (*line_ptr == '\t') {
      line_ptr++;
      d++;
    }

    //    printf("depth=%d, d=%d, line_ptr=%s\n", depth, d, line_ptr);

    if (d > depth) {
      log("Warning: depth doesn't match field depth (%d != %d) (%s, line %d)",
          d, depth, in->Filename(), in->LineNumber());
      SKIP;
    } else
      while (d < depth) {
        // while the current depth < sec_ptr_depth, pull out a level
        sec_ptr = sec_ptr->parent;
        depth--;
      }

    { // check for section title
      char *open = strchr(line_ptr, '[');
      char *close = strchr(line_ptr, ']');

      // if it's there at at the beginning of the line,
      // descend into another level
      if (open && close && close > open && open == line_ptr) {
        // check to see if we need a new table
        if (!sec_ptr->sub_tab || sec_ptr->sub_cnt >= sec_ptr->sub_size)
          resize_subsection_tab(sec_ptr);

        section *new_sec = sec_ptr->sub_tab+sec_ptr->sub_cnt;
        sec_ptr->sub_cnt++;

        // set the data:
        new_sec->parent = sec_ptr;

        char *src = open+1, *dst = new_sec->name;
        while (*src != ']') { // we know ']' is there, for sure
          *(dst++) = *(src++);
        }
        *dst = '\0';

        // set depth and sec_ptr, then skip
        sec_ptr = new_sec;
        depth++;

        //printf("read section title %s, inc'ing depth\n", sec_ptr->name);

        SKIP;
      }
    }

    // so we know by this point that we have a potential field,
    // so first check to see if we need a new table
    if (!sec_ptr->field_tab || sec_ptr->field_cnt >= sec_ptr->field_size)
      resize_field_tab(sec_ptr);

    // then get the next field entry
    field *field_ptr = sec_ptr->field_tab+sec_ptr->field_cnt;
    char *name_ptr = field_ptr->name;

    // copy the field name, until ":\t" or ":$"
    while (*line_ptr) {
      if (*line_ptr == ':' &&
          (*(line_ptr+1) == '\t' || *(line_ptr+1) == '$'))
        break;

      if((name_ptr-field_ptr->name) > MAX_FIELD_LENGTH)
        field_ptr->name[MAX_FIELD_LENGTH - 1] = '\0';

      *(name_ptr++) = *(line_ptr++);
    }
    *name_ptr = '\0';

    // make sure we've got a field name
    if (!*line_ptr) {
      log("Invalid field: '%s' (%s, line %d)",
          line, in->Filename(), in->LineNumber());
      SKIP;
    }

    //printf("continuing with field %s\n", field_ptr->name);

    // and if we do, advance the field_cnt and continue
    sec_ptr->field_cnt++;
    total_fields++;

    // advance line_ptr past the ":\t" or ":$", and skip the spaces
    if (*line_ptr == ':') {
      if (*(line_ptr+1) == '$') {
        multiline_input = true;
        line_ptr += 2;
      } else if (*(line_ptr+1) == '\t')
        line_ptr += 2;

      while (*line_ptr && (*line_ptr == ' ' || *line_ptr == '\t'))
        line_ptr++;
    }

    // now if we have a single line, just copy the rest of the line
    if (!multiline_input) {
      memset(field_ptr->line, 0, MAX_LINE_LENGTH);
      strncpy(field_ptr->line, line_ptr, MAX_LINE_LENGTH);
    } else {
      // otherwise use ReadString to get a '~'-terminated multiline string
      field_ptr->multiline = in->ReadString();
    }

    in->GetLine(line, MAX_LINE_LENGTH*2, FALSE);
  }

  return total_fields;
}

#undef SKIP

int  VTable::GetInt(const char *where, int defawlt)
{
  field *ptr = find_field(where);

  if (!ptr)
    return defawlt;

  return atoi(ptr->line);
}

long VTable::GetLong(const char *where, long defawlt)
{
  field *ptr = find_field(where);

  if (!ptr)
    return defawlt;

  return atol(ptr->line);
}

int  VTable::LookupInt(const char *where,
                       const char **lookup_tab, int defawlt)
{
  field *ptr = find_field(where);

  if (!ptr)
    return defawlt;

  int i;
  for (i = 0; *lookup_tab[i] != '\n'; i++)
    if (!str_cmp(ptr->line, lookup_tab[i]))
      return i;

  return defawlt;
}

const char *VTable::GetString(const char *where, const char *defawlt)
{
  field *ptr = find_field(where);

  if (!ptr) {
    return defawlt;
  }

  return (ptr->multiline? ptr->multiline : ptr->line);
}

float VTable::GetFloat(const char *where, float defawlt)
{
  field *ptr = find_field(where);

  if (!ptr)
    return defawlt;

  return (float)atof(ptr->line);
}

const char *VTable::GetIndexField(const char *sect_name, int n)
{
  section *ptr = find_section(sect_name);

  if (!ptr)
    return NULL;

  return ptr->field_tab[n].name;
}

const char *VTable::GetIndexSection(const char *sect_name, int n)
{
  section *ptr = find_section(sect_name);

  if (!ptr)
    return NULL;

  return ptr->sub_tab[n].name;
}

int  VTable::GetIndexInt(const char *sect_name, int n, int defawlt)
{
  section *ptr = find_section(sect_name);

  if (!ptr)
    return defawlt;

  return atoi(ptr->field_tab[n].line);
}

const char *VTable::GetIndexString(const char *sect_name, int n,
                                   const char *defawlt)
{
  section *ptr = find_section(sect_name);

  if (!ptr)
    return defawlt;

  return (ptr->field_tab[n].multiline? ptr->field_tab[n].multiline
          : ptr->field_tab[n].line);
}

void VTable::separate(const char *where,
                      char sect_name[MAX_SECTION_LENGTH],
                      char field_name[MAX_FIELD_LENGTH])
{
  const char *src_ptr = where;
  char *sec_ptr = sect_name, *field_ptr = field_name;

  *sec_ptr = *field_ptr = '\0';

  while (*src_ptr && *src_ptr == ' ')
    src_ptr++;

  char *delim = strchr(src_ptr, '/');

  if (!delim) {
    strncpy(field_name, src_ptr, MAX_FIELD_LENGTH);
    *(field_name + MAX_FIELD_LENGTH - 1) = '\0';
  } else {
    if ((delim-src_ptr) < MAX_SECTION_LENGTH) {
      strncpy(sect_name, src_ptr, delim-src_ptr);
      *(sect_name + (delim-src_ptr)) = '\0';
    } else {
      strncpy(sect_name, src_ptr, MAX_SECTION_LENGTH);
      *(sect_name + MAX_SECTION_LENGTH - 1) = '\0';
    }

    strncpy(field_name, delim+1, MAX_FIELD_LENGTH);
    *(field_name + MAX_FIELD_LENGTH - 1) = '\0';
  }
}

void VTable::resize_subsection_tab(section *ptr, int empty)
{
  if (ptr->sub_tab && ptr->sub_cnt < ptr->sub_size)
    return;

  ptr->sub_size += empty;
  section *new_tab = new section[ptr->sub_size];

  if (ptr->sub_tab) {
    for (int k = 0; k < ptr->sub_cnt; k++)
      new_tab[k] = ptr->sub_tab[k];

    delete [] ptr->sub_tab;
  }

  ptr->sub_tab = new_tab;
}

void VTable::resize_field_tab(section *ptr, int empty)
{
  if (ptr->field_tab && ptr->field_cnt < ptr->field_size)
    return;

  ptr->field_size += empty;
  field *new_tab = new field[ptr->field_size];

  if (ptr->field_tab) {
    for (int k = 0; k < ptr->field_cnt; k++)
      new_tab[k] = ptr->field_tab[k];

    delete [] ptr->field_tab;
  }

  ptr->field_tab = new_tab;
}

VTable::field *VTable::find_field(const char *where)
{
  char sect[MAX_SECTION_LENGTH], field_name[MAX_FIELD_LENGTH];

  separate(where, sect, field_name);

  section *ptr = find_section(sect);

  if (!ptr || !*field_name)
    return NULL;

  for (int i = 0; i < ptr->field_cnt; i++)
    if (!str_cmp(ptr->field_tab[i].name, field_name))
      return (ptr->field_tab+i);

  return NULL;
}

VTable::section *VTable::find_section(const char *sect)
{
  if (!sect || !*sect)
    return &top;

  return (look_for_section(&top, sect));
}

VTable::section *VTable::look_for_section(section *ptr,
    const char *name)
{
  if (!ptr)
    return NULL;

  if (!str_cmp(ptr->name, name))
    return ptr;

  for (int i = 0; i < ptr->sub_cnt; i++) {
    section *ret = look_for_section(ptr->sub_tab+i, name);

    if (ret)
      return ret;
  }

  return NULL;
}

int  VTable::count_subsections(const section *ptr) const
{
  if (!ptr)
    return 0;

  int count = 0;

  for (int i = 0; i < ptr->sub_cnt; i++)
    count += count_subsections(ptr->sub_tab+i);

  return count;
}
