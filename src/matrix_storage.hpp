#ifndef __matrix_storage_h__
#define __matrix_storage_h__

#include <atomic>

// Global atomic integer to ensure unique IDs
extern std::atomic<long>    matrix_file_id_counter;

// High level functions; the ones you probably want to use when messing with this system
void                delete_matrix_file(struct matrix_file file);
matrix_file*        copy_matrix_file_to(struct matrix_file *file, obj_data* to_device);
void                move_matrix_file_to(struct matrix_file *file, obj_data* to_device);
void                move_matrix_file_to(struct matrix_file *file, host_data* to_host);

// Low level functions, a lot of these shouldn't be necessary but are exposed for advanced functionality.
int                 get_device_free_memory(struct obj_data *device);
matrix_file*        create_matrix_file(obj_data *storage, int load_origin);
matrix_file*        obj_to_matrix_file(obj_data *prog, obj_data *device);
matrix_file*        obj_to_matrix_file(obj_data *prog);
matrix_file*        clone_matrix_file(struct matrix_file *source);
obj_data*           matrix_file_to_obj(matrix_file *file);
obj_data*           find_internal_storage(struct char_data *ch, int free_space=0);
const char*         keyword_appears_in_file(const char *keyword, struct matrix_file *file, bool search_name=1, bool search_desc=0);
struct matrix_file* get_matrix_file_in_list_vis(struct char_data *ch, const char *name, struct matrix_file *list);
obj_data*           find_obj_in_vector_vis(struct char_data * ch, const char *name, std::vector<obj_data *> &list);
void                extract_matrix_file(struct matrix_file *file);
bool                handle_matrix_file_transfer(struct char_data *ch, char *argument);
bool                program_can_be_copied(struct matrix_file *prog);
bool                perform_get_matrix_file_from_device(struct char_data *ch, struct matrix_file *file,
                        struct obj_data *device, int mode);
bool                can_file_fit(struct matrix_file *file, struct obj_data *device);

std::vector<struct obj_data*>      get_internal_storage_devices(struct char_data *ch);
std::vector<struct obj_data*>      get_storage_devices(struct char_data *ch, bool only_relevant = FALSE);

#endif