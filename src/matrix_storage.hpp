#ifndef __matrix_storage_h__
#define __matrix_storage_h__

#include <atomic>

// Global atomic integer to ensure unique IDs
extern std::atomic<long>    matrix_file_id_counter;

/******************************************************************************************************
 * High level functions; the ones you probably want to use when messing with this system
 * 
 * All of these functions adjust memory, perform high level functions like validating skillsofts,
 * and so on. Very important stuff.
 */

/**
 * @brief Deletes a matrix file from all locations where it is stored.
 *
 * If the matrix file is linked to a character, it will deactivate skillsoft functionality
 * before removing the file from the host or device storage.
 * Finally, it extracts and fully removes the file from the system.
 *
 * @param file The matrix file to be deleted.
 */
void                delete_matrix_file(struct matrix_file *file);

/**
 * @brief Creates a copy of a matrix file and places it in a specified device.
 *
 * This function clones the given matrix file and assigns the copy to the specified device.
 * If `to_device` is valid, the copied file is linked to it, updating its file list accordingly.
 *
 * @param file The matrix file to be copied.
 * @param to_device The target device where the copied file should be stored.
 * @return A pointer to the newly created copy of the matrix file.
 */
matrix_file*        copy_matrix_file_to(struct matrix_file *file, obj_data* to_device);

/**
 * @brief Moves a matrix file from its current location to a specified device.
 *
 * The function removes the file from its current host or device before placing it in the
 * new device. It adjusts memory allocations accordingly when transferring the file.
 *
 * @param file The matrix file to be moved.
 * @param to_device The target device where the file will be stored.
 */
void                move_matrix_file_to(struct matrix_file *file, obj_data* to_device);

/**
 * @brief Moves a matrix file from its current location to a specified host.
 *
 * The function removes the file from its current host or device before placing it in the
 * new host. It ensures proper memory adjustments when transferring the file between locations.
 *
 * @param file The matrix file to be moved.
 * @param to_host The target host where the file will be stored.
 */
void                move_matrix_file_to(struct matrix_file *file, host_data* to_host);

/******************************************************************************************************
 * Low level functions, a lot of these shouldn't be necessary but are exposed for advanced functionality.
 * 
 * Using these functions is not recommended unless you're also prepared to do all the functions,
 * and assume nothing about their functionality. No handholding.
 */

/**
 * @brief Calculates the available free memory on a given device.
 *
 * This function computes the free memory by subtracting the used memory from the total memory.
 *
 * @param device The device whose free memory is being calculated.
 * @return The amount of free memory available on the device.
 */
int                 get_device_free_memory(struct obj_data *device);

/**
 * @brief Retrieves the amount of memory currently used on a given device.
 *
 * This function determines how much memory is in use based on the device type.
 * - For `ITEM_DECK_ACCESSORY`, the used memory is stored in `GET_OBJ_VAL(device, 3)`.
 * - For `ITEM_CUSTOM_DECK` and `ITEM_CYBERDECK`, the used memory is stored in `GET_OBJ_VAL(device, 5)`.
 *
 * @param device The device whose used memory is being queried.
 * @return The amount of memory currently in use, or 0 if the device type is unrecognized.
 */
int                 get_device_used_memory(struct obj_data *device);

/**
 * @brief Retrieves the total memory capacity of a given device.
 *
 * This function determines the total memory available on the device based on its type.
 * - For `ITEM_DECK_ACCESSORY`, the total memory is stored in `GET_OBJ_VAL(device, 2)`.
 * - For `ITEM_CUSTOM_DECK` and `ITEM_CYBERDECK`, the total memory is stored in `GET_OBJ_VAL(device, 3)`.
 *
 * @param device The device whose total memory is being queried.
 * @return The total memory available on the device, or 0 if the device type is unrecognized.
 */
int                 get_device_total_memory(struct obj_data *device);


int        adjust_device_memory(struct obj_data *device, int change);
/**
 * @brief Creates a new matrix file and optionally stores it in a specified device.
 *
 * This function initializes a new matrix file, assigning it a unique ID and setting its creation time.
 * If a storage device is provided, the file is added to its list of stored files.
 *
 * @param storage The device where the new matrix file should be stored. If NULL, the file is created without storage.
 * @param load_origin The origin identifier for the file's creation.
 * @return A pointer to the newly created matrix file.
 */
matrix_file*        create_matrix_file(obj_data *storage, int load_origin);

/**
 * @brief Converts an object into a matrix file.
 *
 * This function extracts a matrix file from an object if it already contains one.
 * Otherwise, it creates a new matrix file based on the provided object's properties.
 *
 * @param prog Pointer to the obj_data structure representing the program object.
 * @param device Pointer to the obj_data structure representing the target device 
 *               where the matrix file will be stored.
 * @return A pointer to the created or extracted matrix_file.
 *
 * @note If `prog` already contains a matrix file, it is removed from the object
 *       and transferred to `device`, if provided.
 * 
 * @warning The function assumes `prog` is valid. Passing a NULL pointer may lead to
 *          undefined behavior.
 */
matrix_file*        obj_to_matrix_file(obj_data *prog, obj_data *device);
matrix_file*        obj_to_matrix_file(obj_data *prog);

/**
 * @brief Creates a duplicate of a matrix file.
 *
 * This function clones an existing matrix file, copying all relevant attributes
 * to a newly allocated matrix_file structure.
 *
 * @param source Pointer to the matrix_file structure that will be cloned.
 * @return Pointer to the newly created matrix_file structure.
 *
 * @note The cloned file is independent of the source and does not retain any 
 *       references to the original object or device it was linked to.
 * 
 * @warning The function assumes `source` is valid. Passing a NULL pointer will 
 *          likely result in undefined behavior.
 */
matrix_file*        clone_matrix_file(struct matrix_file *source);

/**
 * @brief Converts a matrix file into an object.
 *
 * This function creates an optical chip object and associates it with 
 * the given matrix file. The chip is named based on the file type and rating.
 *
 * @param file Pointer to the matrix_file structure to be converted.
 * @return Pointer to the newly created obj_data structure representing the chip.
 *
 * @note The function hides the matrix file on the chip for easier handling.
 *       If the file was previously associated with another object, it is removed.
 */
obj_data*           matrix_file_to_obj(matrix_file *file);

/**
 * @brief Finds an internal storage device for a character, optionally checking for available free space.
 *
 * This function retrieves a list of internal storage devices available to the given character and returns
 * the first device that meets the specified free space requirement. If `free_space` is 0, it simply returns
 * the first available storage device.
 *
 * @param ch The character whose internal storage devices are being searched.
 * @param free_space The minimum amount of free memory required. If 0, the first available device is returned.
 * @return A pointer to the first storage device that meets the criteria, or NULL if no suitable device is found.
 */
obj_data*           find_internal_storage(struct char_data *ch, int free_space=0);

/**
 * @brief Determines if a keyword appears in a matrix file's name or content.
 *
 * This function searches for a given keyword in the file's name and/or content,
 * depending on the specified search parameters.
 *
 * @param keyword The keyword to search for.
 * @param file Pointer to the matrix_file structure to be searched.
 * @param search_name If true, searches for the keyword in the file's name.
 * @param search_desc If true, searches for the keyword in the file's content.
 * @return "name" if the keyword is found in the file's name, 
 *         "content" if found in the content, or NULL if not found.
 *
 * @note If both `search_name` and `search_desc` are true, priority is given 
 *       to the name if the keyword appears in both.
 *
 * @warning Logs a system error if a NULL file is received.
 */
const char*         keyword_appears_in_file(const char *keyword, struct matrix_file *file, bool search_name=1, bool search_desc=0);

/**
 * @brief Searches for a visible matrix file in a given list by name.
 *
 * This function iterates through a list of matrix files to find one that matches
 * a given name. If multiple files have the same name, it selects the one based 
 * on the specified occurrence number.
 *
 * @param ch Pointer to the character searching for the file.
 * @param name The name of the matrix file to search for.
 * @param list Pointer to the head of the matrix file list to search within.
 * 
 * @return Pointer to the found `matrix_file`, or `NULL` if no matching file is found.
 *
 * @note The function supports searching for numbered instances of a file name
 *       (e.g., "2.file" to get the second occurrence). If the file list is empty 
 *       or the number is invalid, the function returns `NULL`.
 */
struct matrix_file* get_matrix_file_in_list_vis(struct char_data *ch, const char *name, struct matrix_file *list);

/**
 * @brief Extracts a matrix file from the world.
 *
 * This function removes a matrix file from its current location, whether in a host 
 * or an object, and then deletes it. It ensures that a file is only removed from 
 * one list at a time, logging an error if multiple pointers are set.
 *
 * @param file Pointer to the matrix_file structure representing the file to be extracted.
 *
 * @note The function checks if the file is within a host or an object before removal.
 *       If both `file->in_host` and `file->in_obj` are set, a system error is logged.
 *       The function deletes the file once extracted.
 *
 * @warning If `ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE` is defined, additional 
 *          pointer verification is performed, which may impact performance.
 */
void                extract_matrix_file(struct matrix_file *file);

/**
 * @brief Determines if a program can be copied.
 *
 * This function checks whether a given matrix program can be copied 
 * based on its type. Certain program types are restricted from copying.
 *
 * @param prog Pointer to the matrix_file structure representing the program.
 * @return `TRUE` if the program can be copied, `FALSE` otherwise.
 *
 * @note Programs of certain types (e.g., SOFT_ASIST_COLD, SOFT_HARDENING, etc.)
 *       are explicitly restricted from being copied.
 */
bool                program_can_be_copied(struct matrix_file *prog);
/**
 * @brief Determines if a matrix file can fit on a given device.
 *
 * This function checks whether a matrix file can be stored on a device based on 
 * available memory.
 *
 * @param file Pointer to the matrix_file structure representing the file to store.
 * @param device Pointer to the obj_data structure representing the target device.
 * @return `TRUE` if the file can fit on the device, `FALSE` otherwise.
 *
 * @note If the file has a size of `0`, it is always considered to fit.
 * 
 * @warning This function assumes `device` is valid. Passing a NULL pointer may 
 *          lead to undefined behavior.
 */
bool                can_file_fit(struct matrix_file *file, struct obj_data *device);

/**
 * @brief Retrieves a list of internal storage devices for a character.
 *
 * This function searches for internal cyberware storage (e.g., headware memory)
 * within the character's cyberware.
 *
 * @param ch Pointer to the character whose internal storage devices are being retrieved.
 * @return A vector containing pointers to internal storage device objects.
 *
 */
std::vector<struct obj_data*>      get_internal_storage_devices(struct char_data *ch);
/**
 * @brief Retrieves a list of storage devices available to a character.
 *
 * This function gathers all storage devices a character has access to, 
 * including cyberware memory, cyberdecks, and other relevant devices.
 *
 * @param ch Pointer to the character whose storage devices are being retrieved.
 * @param only_relevant If true, limits the result to only headware memory and deck 
 *                      while the character is actively decking.
 * @return A vector containing pointers to available storage device objects.
 *
 * @note If `only_relevant` is true and the character is in the Matrix (`PLR_MATRIX` flag),
 *       the function only returns headware memory and the active cyberdeck.
 * @note The function ensures that storage devices are deduplicated before returning the list.
 */
std::vector<struct obj_data*>      get_storage_devices(struct char_data *ch, bool only_relevant = FALSE);

obj_data*           find_obj_in_vector_vis(struct char_data * ch, const char *name, std::vector<obj_data *> &list);
bool                perform_get_matrix_file_from_device(struct char_data *ch, struct matrix_file *file,
                        struct obj_data *device, int mode);
bool                handle_matrix_file_transfer(struct char_data *ch, char *argument);

#endif