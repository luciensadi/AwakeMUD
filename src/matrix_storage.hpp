#ifndef __matrix_storage_h__
#define __matrix_storage_h__

#include <atomic>

// Global atomic integer to ensure unique IDs
std::atomic<int>    matrix_file_id_counter{1};

matrix_file*        create_matrix_file(obj_data *storage, int load_origin);
const char*         keyword_appears_in_file(const char *keyword, struct matrix_file *file, bool search_name=1, bool search_desc=0);
struct matrix_file* get_matrix_file_in_list_vis(struct char_data * ch, const char *name, struct matrix_file * list);
void                extract_matrix_file(struct matrix_file *file);
bool                program_can_be_copied(struct matrix_file *prog);

#endif