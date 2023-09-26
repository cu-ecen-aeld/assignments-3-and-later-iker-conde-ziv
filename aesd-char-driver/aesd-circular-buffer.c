/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    size_t total_chars = 0;
    size_t start_entry = buffer->out_offs;

    // Loop through the entries starting from buffer->out_offs
    for (size_t i = start_entry; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED + start_entry; ++i)
    {
        // Calculate the index taking into account wrapping around to the beginning
        size_t index = i % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

        // Check if the entry at this index is empty
        if (buffer->entry[index].buffptr == NULL)
        {
            // If we've searched all entries, exit the loop
            if (i == start_entry + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
            {
                break;
            }
            continue;
        }

        // Calculate the total chars including the current entry
        total_chars += buffer->entry[index].size;

        // Check if the char_offset is within this entry
        if (char_offset < total_chars)
        {
            // Calculate the byte offset within the entry
            *entry_offset_byte_rtn = char_offset - (total_chars - buffer->entry[index].size);

            // Return a pointer to the corresponding entry structure
            return &buffer->entry[index];
        }
    }

    // If char_offset is not found in any entry, return NULL
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    // If the buffer is full, overwrite the oldest entry and advance buffer->out_offs
    if (buffer->full)
    {
        // Mark the oldest entry as unused
        buffer->entry[buffer->out_offs].buffptr = NULL;
        buffer->entry[buffer->out_offs].size = 0;
        // Advance out_offs to the next index
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // Copy data from the new entry to the current input position and advance in_offs
    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;
    // Advance in_offs to the next index
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // Mark the buffer as full if in_offs reaches out_offs
    if (buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
