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

#include <linux/slab.h>
#include <linux/printk.h>

#include "aesd-circular-buffer.h"

static uint8_t circular_buffer_size = AESDCHAR_DEFAULT_MAX_WRITE_OPERATIONS_SUPPORTED;

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
    size_t i = 0;
    for (i = start_entry; i < circular_buffer_size + start_entry; ++i)
    {
        // Calculate the index taking into account wrapping around to the beginning
        size_t index = i % circular_buffer_size;

        // Check if the entry at this index is empty
        if (buffer->entry[index].buffptr == NULL)
        {
            // If we've searched all entries, exit the loop
            if (i == start_entry + circular_buffer_size - 1)
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
const char * aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    const char * ret_val = NULL;
    // If the buffer is full, overwrite the oldest entry and advance buffer->out_offs
    if (buffer->full)
    {
        // Mark the oldest entry as unused and return pointer to be freed
        ret_val = buffer->entry[buffer->out_offs].buffptr;
        buffer->entry[buffer->out_offs].buffptr = NULL;
        buffer->entry[buffer->out_offs].size = 0;
        // Advance out_offs to the next index
        buffer->out_offs = (buffer->out_offs + 1) % circular_buffer_size;
    }

    // Copy data from the new entry to the current input position and advance in_offs
    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;
    // Advance in_offs to the next index
    buffer->in_offs = (buffer->in_offs + 1) % circular_buffer_size;

    // Mark the buffer as full if in_offs reaches out_offs
    if (buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }

    return ret_val;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
* and allocates @param m_circular_buffer_size elements in the circular buffer
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer, uint8_t m_circular_buffer_size)
{
    circular_buffer_size = m_circular_buffer_size;

    if (buffer->entry != NULL)
    {
        kfree((void*)buffer->entry);
    }

    buffer->entry = kmalloc(circular_buffer_size * sizeof(struct aesd_buffer_entry), GFP_KERNEL);

    if (buffer->entry == NULL)
    {
        printk(KERN_ERR "Error allocating circular buffer entries");
        return;
    }

    memset(buffer->entry, 0, circular_buffer_size * sizeof(struct aesd_buffer_entry));
    buffer->in_offs = 0;
    buffer->out_offs = 0;
    buffer->full = false;
    
    printk(KERN_INFO "AESD circular buffer initialized with size %d", circular_buffer_size);
}

/**
* Frees circular buffer memory allocated in aesd_circular_buffer_init()
*/
void aesd_circular_buffer_cleanup(struct aesd_circular_buffer *buffer)
{
    kfree((void*)buffer->entry);
}
