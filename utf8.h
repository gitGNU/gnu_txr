/* Copyright 2011
 * Kaz Kylheku <kaz@kylheku.com>
 * Vancouver, Canada
 * All rights reserved.
 *
 * BSD License:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. The name of the author may not be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

size_t utf8_from_uc(wchar_t *, const unsigned char *);
size_t utf8_from(wchar_t *, const char *);
size_t utf8_to_uc(unsigned char *, const wchar_t *);
size_t utf8_to(char *, const wchar_t *);
wchar_t *utf8_dup_from_uc(const unsigned char *);
wchar_t *utf8_dup_from(const char *);
char *utf8_dup_to(const wchar_t *);
unsigned char *utf8_dup_to_uc(const wchar_t *);

enum utf8_state { utf8_init, utf8_more1, utf8_more2, utf8_more3 };

typedef struct utf8_decoder {
  enum utf8_state state;
  wchar_t wch;
  int head, tail, back;
  int buf[8];
} utf8_decoder_t;

int utf8_encode(wchar_t, int (*put)(int ch, mem_t *ctx), mem_t *ctx);
void utf8_decoder_init(utf8_decoder_t *);
wint_t utf8_decode(utf8_decoder_t *,int (*get)(mem_t *ctx), mem_t *ctx);

FILE *w_fopen(const wchar_t *, const wchar_t *);
FILE *w_popen(const wchar_t *, const wchar_t *);
