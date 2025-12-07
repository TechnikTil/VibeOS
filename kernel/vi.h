/*
 * VibeOS vi Editor
 *
 * A minimal vi-like text editor.
 * Supports basic modal editing: normal mode and insert mode.
 */

#ifndef VI_H
#define VI_H

// Run the vi editor on a file
// Returns 0 on success, -1 on error
int vi_edit(const char *filename);

#endif
