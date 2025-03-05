#ifndef __matrix_storage_h__
#define __matrix_storage_h__

matrix_file*        new_program(obj_data *storage, int load_origin);
const char*         keyword_appears_in_file(const char *keyword, struct matrix_file *file, bool search_name=1, bool search_desc=0);
struct matrix_file* get_file_in_list_vis(struct char_data * ch, const char *name, struct matrix_file * list);
void                extract_file(struct matrix_file *file);

#endif