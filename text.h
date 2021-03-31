/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        text.h ( Live Once Library, C++ )
 *
 * COMMENTS:
 */

#ifndef __text__
#define __text__

#include "buf.h"

void text_init();
void text_shutdown();

void text_registervariable(const char *variable, const char *value);
const char *text_lookupvariable(const char *variable);

inline void text_registervariable(BUF variable, const char *value)
{ text_registervariable(variable.buffer(), value); }
inline void text_registervariable(BUF variable, BUF value)
{ text_registervariable(variable.buffer(), value.buffer()); }
inline void text_registervariable(const char *variable, BUF value)
{ text_registervariable(variable, value.buffer()); }
inline const char *text_lookupvariable(BUF variable) { return text_lookupvariable(variable.buffer()); }

bool text_hasentry(const char *key);
inline bool text_hasentry(BUF buf)
{ return text_hasentry(buf.buffer()); }
bool text_hasentry(const char *dict, const char *word);
inline bool text_hasentry(const char *dict, BUF word)
{ return text_hasentry(dict, word.buffer()); }


BUF text_lookup(const char *key);
inline BUF text_lookup(BUF buf)
{ return text_lookup(buf.buffer()); }

BUF text_lookup(const char *dict, const char *word);
inline BUF text_lookup(const char *dict, BUF word)
{ return text_lookup(dict, word.buffer()); }
BUF text_lookup(const char *dict, const char *word, const char *subword);

// Sets and \n or \r to \0, useful for the results of
// is.getline() if the source file was a dos system.
void text_striplf(char *line);

bool text_hasnonws(const char *line);
int text_firstnonws(const char *line);
int text_lastnonws(const char *line);

// Queries the names.txt database
const char *text_getname(int idx);
void text_shufflenames();

#endif

