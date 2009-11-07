/* Copyright 2009
 * Kaz Kylheku <kkylheku@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>
#include <errno.h>
#include "lib.h"
#include "gc.h"
#include "unwind.h"
#include "stream.h"

obj_t *std_input, *std_output, *std_error;

struct strm_ops {
  struct cobj_ops cobj_ops;
  obj_t *(*put_string)(obj_t *, const char *);
  obj_t *(*put_char)(obj_t *, int);
  obj_t *(*get_line)(obj_t *);
  obj_t *(*get_char)(obj_t *);
  obj_t *(*vcformat)(obj_t *, const char *fmt, va_list vl);
  obj_t *(*vformat)(obj_t *, const char *fmt, va_list vl);
  obj_t *(*close)(obj_t *, obj_t *);
};

static obj_t *common_equal(obj_t *self, obj_t *other)
{
  return self == other ? t : nil;
}

static void common_destroy(obj_t *obj)
{
  (void) close_stream(obj, nil);
}

obj_t *common_vformat(obj_t *stream, const char *fmt, va_list vl)
{
  int ch;

  for (; (ch = *fmt) != 0; fmt++) {
    obj_t *obj;

    if (ch == '~') {
      ch = *++fmt;
      if (ch == 0)
        abort();
      switch (ch) {
      case '~':
        put_cchar(stream, ch);
        continue;
      case 'a':
        obj = va_arg(vl, obj_t *);
        if (obj == nao)
          abort();
        obj_pprint(obj, stream);
        continue;
      case 's':
        obj = va_arg(vl, obj_t *);
        if (obj == nao)
          abort();
        obj_print(obj, stream);
        continue;
      default:
        abort();
      }
      continue;
    }

    put_cchar(stream, ch);
  }

  if (va_arg(vl, obj_t *) != nao)
    internal_error("unterminated format argument list");
  return t;
}

struct stdio_handle {
  FILE *f;
  obj_t *descr;
};

void stdio_stream_print(obj_t *stream, obj_t *out)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
  format(out, "#<~s ~s>", stream->co.cls, h->descr, nao);
}

void stdio_stream_destroy(obj_t *stream)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
  common_destroy(stream);
  free(h);
}

void stdio_stream_mark(obj_t *stream)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
  gc_mark(h->descr);
}

static obj_t *stdio_maybe_read_error(obj_t *stream)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
  if (ferror(h->f)) {
    clearerr(h->f);
    uw_throwf(file_error, "error reading ~a: ~a/~s",
              stream, num(errno), string(strerror(errno)));
  }
  return nil;
}

static obj_t *stdio_maybe_write_error(obj_t *stream)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
  if (ferror(h->f)) {
    clearerr(h->f);
    uw_throwf(file_error, "error writing ~a: ~a/~s",
              stream, num(errno), string(strerror(errno)));
  }
  return nil;
}

static obj_t *stdio_put_string(obj_t *stream, const char *s)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
  return (h->f && fputs(s, h->f) != EOF) ? t : stdio_maybe_write_error(stream);
}

static obj_t *stdio_put_char(obj_t *stream, int ch)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
  return (h->f && putc(ch, h->f) != EOF) ? t : stdio_maybe_write_error(stream);
}

static char *snarf_line(FILE *in)
{
  const size_t min_size = 512;
  size_t size = 0;
  size_t fill = 0;
  char *buf = 0;

  for (;;) {
    int ch = getc(in);

    if (ch == EOF && buf == 0)
      break;

    if (fill >= size) {
      size_t newsize = size ? size * 2 : min_size;
      buf = chk_realloc(buf, newsize);
      size = newsize;
    }

    if (ch == '\n' || ch == EOF) {
      buf[fill++] = 0;
      break;
    }
    buf[fill++] = ch;
  }

  if (buf)
    buf = chk_realloc(buf, fill);

  return buf;
}

static obj_t *stdio_get_line(obj_t *stream)
{
  if (stream->co.handle == 0) {
    return nil;
  } else {
    struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
    char *line = snarf_line(h->f);
    if (!line)
      return stdio_maybe_read_error(stream);
    return string_own(line);
  }
}

obj_t *stdio_get_char(obj_t *stream)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;
  if (h->f) {
    int ch = getc(h->f);
    return (ch != EOF) ? chr(ch) : stdio_maybe_read_error(stream);
  }
  return nil;
}

obj_t *stdio_vcformat(obj_t *stream, const char *fmt, va_list vl)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;

  if (h->f) {
    int n = vfprintf(h->f, fmt, vl);
    return (n >= 0) ? num(n) : stdio_maybe_write_error(stream);
  }
  return nil;
}

static obj_t *stdio_close(obj_t *stream, obj_t *throw_on_error)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;

  if (h->f != 0 && h->f != stdin && h->f != stdout) {
    int result = fclose(h->f);
    h->f = 0;
    if (result == EOF && throw_on_error) {
      uw_throwf(file_error, "error closing ~a: ~a/~s",
                stream, num(errno), string(strerror(errno)));
    }
    return result != EOF ? t : nil;
  }
  return nil;
}

static struct strm_ops stdio_ops = {
  { common_equal,
    stdio_stream_print,
    stdio_stream_destroy,
    stdio_stream_mark },
  stdio_put_string,
  stdio_put_char,
  stdio_get_line,
  stdio_get_char,
  stdio_vcformat,
  common_vformat,
  stdio_close
};

static obj_t *pipe_close(obj_t *stream, obj_t *throw_on_error)
{
  struct stdio_handle *h = (struct stdio_handle *) stream->co.handle;

  if (h->f != 0) {
    int status = pclose(h->f);

    h->f = 0;

    if (status != 0 && throw_on_error) {
      if (status < 0) {
        uw_throwf(process_error, "unable to obtain status of command ~a: ~a/~s",
                  stream, num(errno), string(strerror(errno)), nao);
      } else if (WIFEXITED(status)) {
        int exitstatus = WEXITSTATUS(status);
        uw_throwf(process_error, "pipe ~a terminated with status ~a",
                  stream, num(exitstatus), nao);
      } else if (WIFSIGNALED(status)) {
        int termsig = WTERMSIG(status);
        uw_throwf(process_error, "pipe ~a terminated by signal ~a",
                  stream, num(termsig), nao);

      } else if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
        uw_throwf(process_error, "processes of closed pipe ~a still running",
                  stream, nao);
      } else {
        uw_throwf(file_error, "strange status in when closing pipe ~a",
                  stream, nao);
      }
    }

    return status == 0 ? t : nil;
  }
  return nil;
}

static struct strm_ops pipe_ops = {
  { common_equal,
    stdio_stream_print,
    stdio_stream_destroy,
    stdio_stream_mark },
  stdio_put_string,
  stdio_put_char,
  stdio_get_line,
  stdio_get_char,
  stdio_vcformat,
  common_vformat,
  pipe_close
};

void string_in_stream_mark(obj_t *stream)
{
  obj_t *stuff = (obj_t *) stream->co.handle;
  gc_mark(stuff);
}

static obj_t *string_in_get_line(obj_t *stream)
{
  obj_t *pair = (obj_t *) stream->co.handle;
  obj_t *string = car(pair);
  obj_t *pos = cdr(pair);

  /* TODO: broken, should only scan to newline */
  if (lt(pos, length(string))) {
    obj_t *result = sub_str(string, pos, nil);
    *cdr_l(pair) = length_str(string);
    return result;
  }

  return nil;
}

static obj_t *string_in_get_char(obj_t *stream)
{
  obj_t *pair = (obj_t *) stream->co.handle;
  obj_t *string = car(pair);
  obj_t *pos = cdr(pair);

  if (lt(pos, length_str(string))) {
    *cdr_l(pair) = plus(pos, one);
    return chr_str(string, pos);
  }

  return nil;
}

static struct strm_ops string_in_ops = {
  { common_equal,
    cobj_print_op,
    0,
    string_in_stream_mark },
  0,
  0,
  string_in_get_line,
  string_in_get_char,
  0,
  0,
  0
};

struct string_output {
  char *buf;
  size_t size;
  size_t fill;
};

static void string_out_stream_destroy(obj_t *stream)
{
  struct string_output *so = (struct string_output *) stream->co.handle;

  if (so) {
    free(so->buf);
    so->buf = 0;
    free(so);
    stream->co.handle = 0;
  }
}

static obj_t *string_out_put_string(obj_t *stream, const char *s)
{
  struct string_output *so = (struct string_output *) stream->co.handle;

  if (so == 0) {
    return nil;
  } else {
    size_t len = strlen(s);
    size_t old_size = so->size;
    size_t required_size = len + so->fill + 1;

    if (required_size < len)
      return nil;

    while (so->size <= required_size) {
      so->size *= 2;
      if (so->size < old_size)
        return nil;
    }

    so->buf = chk_realloc(so->buf, so->size);
    memcpy(so->buf + so->fill, s, len + 1);
    so->fill += len;
    return t;
  }
}

static obj_t *string_out_put_char(obj_t *stream, int ch)
{
  char mini[2];
  mini[0] = ch;
  mini[1] = 0;
  return string_out_put_string(stream, mini);
}

obj_t *string_out_vcformat(obj_t *stream, const char *fmt, va_list vl)
{
  struct string_output *so = (struct string_output *) stream->co.handle;

  if (so == 0) {
    return nil;
  } else {
    int nchars, nchars2;
    char dummy_buf[1];
    size_t old_size = so->size;
    size_t required_size;
    va_list vl_copy;

#if defined va_copy
    va_copy (vl_copy, vl);
#elif defined __va_copy
    __va_copy (vl_copy, vl);
#else
    vl_copy = vl;
#endif

    nchars = vsnprintf(dummy_buf, 0, fmt, vl_copy);

#if defined va_copy || defined __va_copy
    va_end (vl_copy);
#endif

    bug_unless (nchars >= 0);

    required_size = so->fill + nchars + 1;

    if (required_size < so->fill)
      return nil;

    while (so->size <= required_size) {
      so->size *= 2;
      if (so->size < old_size)
        return nil;
    }

    so->buf = chk_realloc(so->buf, so->size);
    nchars2 = vsnprintf(so->buf + so->fill, so->size-so->fill, fmt, vl);
    bug_unless (nchars == nchars2);
    so->fill += nchars;
    return t;
  }
}

static struct strm_ops string_out_ops = {
  { common_equal,
    cobj_print_op,
    string_out_stream_destroy,
    0 },
  string_out_put_string,
  string_out_put_char,
  0,
  0,
  string_out_vcformat,
  common_vformat,
  0,
};

static obj_t *dir_get_line(obj_t *stream)
{
  DIR *handle = (DIR *) stream->co.handle;

  if (handle == 0) {
    return nil;
  } else {
    for (;;) {
      struct dirent *e = readdir(handle);
      if (!e)
        return nil;
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
        continue;
      return string(e->d_name);
    }
  }
}

static obj_t *dir_close(obj_t *stream, obj_t *throw_on_error)
{
  if (stream->co.handle != 0) {
    closedir((DIR *) stream->co.handle);
    stream->co.handle = 0;
    return t;
  }

  return nil;
}

static struct strm_ops dir_ops = {
  { common_equal,
    cobj_print_op,
    common_destroy,
    0 },
  0,
  0,
  dir_get_line,
  0,
  0,
  0,
  dir_close
};


obj_t *make_stdio_stream(FILE *f, obj_t *descr, obj_t *input, obj_t *output)
{
  struct stdio_handle *h = (struct stdio_handle *) chk_malloc(sizeof *h);
  h->f = f;
  h->descr = descr;
  return cobj((void *) h, stream_t, &stdio_ops.cobj_ops);
}

obj_t *make_pipe_stream(FILE *f, obj_t *descr, obj_t *input, obj_t *output)
{
  struct stdio_handle *h = (struct stdio_handle *) chk_malloc(sizeof *h);
  h->f = f;
  h->descr = descr;
  return cobj((void *) h, stream_t, &pipe_ops.cobj_ops);
}

obj_t *make_string_input_stream(obj_t *string)
{
  return cobj((void *) cons(string, zero), stream_t, &string_in_ops.cobj_ops);
}

obj_t *make_string_output_stream(void)
{
  struct string_output *so = (struct string_output *) chk_malloc(sizeof *so);
  so->size = 128;
  so->buf = (char *) chk_malloc(so->size);
  so->fill = 0;
  so->buf[0] = 0;
  return cobj((void *) so, stream_t, &string_out_ops.cobj_ops);
}

obj_t *get_string_from_stream(obj_t *stream)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  if (stream->co.ops == &string_out_ops.cobj_ops) {
    struct string_output *so = (struct string_output *) stream->co.handle;
    obj_t *out = nil;

    stream->co.handle = 0;

    if (!so)
      return out;

    so->buf = chk_realloc(so->buf, so->fill + 1);
    out = string_own(so->buf);
    free(so);
    return out;
  } else if (stream->co.ops == &string_in_ops.cobj_ops) {
    obj_t *pair = (obj_t *) stream->co.handle;
    return pair ? car(pair) : nil;
  } else {
    abort(); /* not a string input or output stream */
  }
}

obj_t *make_dir_stream(DIR *dir)
{
  return cobj((void *) dir, stream_t, &dir_ops.cobj_ops);
}

obj_t *close_stream(obj_t *stream, obj_t *throw_on_error)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->close ? ops->close(stream, throw_on_error) : nil;
  }
}

obj_t *get_line(obj_t *stream)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->get_line ? ops->get_line(stream) : nil;
  }
}

obj_t *get_char(obj_t *stream)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->get_char ? ops->get_char(stream) : nil;
  }
}

obj_t *vformat(obj_t *stream, const char *str, va_list vl)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->vformat ? ops->vformat(stream, str, vl) : nil;
  }
}

obj_t *vcformat(obj_t *stream, const char *string, va_list vl)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->vcformat ? ops->vcformat(stream, string, vl) : nil;
  }
}

obj_t *format(obj_t *stream, const char *str, ...)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    va_list vl;
    obj_t *ret;

    va_start (vl, str);
    ret = ops->vformat ? ops->vformat(stream, str, vl) : nil;
    va_end (vl);
    return ret;
  }
}

obj_t *cformat(obj_t *stream, const char *string, ...)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    va_list vl;
    obj_t *ret;

    va_start (vl, string);
    ret = ops->vformat ? ops->vcformat(stream, string, vl) : nil;
    va_end (vl);
    return ret;
  }
}

obj_t *put_string(obj_t *stream, obj_t *string)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->put_string ? ops->put_string(stream, c_str(string)) : nil;
  }
}

obj_t *put_cstring(obj_t *stream, const char *str)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->put_string ? ops->put_string(stream, str) : nil;
  }
}

obj_t *put_char(obj_t *stream, obj_t *ch)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->put_char ? ops->put_char(stream, c_chr(ch)) : nil;
  }
}

obj_t *put_cchar(obj_t *stream, int ch)
{
  type_check (stream, COBJ);
  type_assert (stream->co.cls == stream_t, ("~a is not a stream", stream));

  {
    struct strm_ops *ops = (struct strm_ops *) stream->co.ops;
    return ops->put_char ? ops->put_char(stream, ch) : nil;
  }
}

obj_t *put_line(obj_t *stream, obj_t *string)
{
  return (put_string(stream, string), put_cchar(stream, '\n'));
}

void stream_init(void)
{
  protect(&std_input, &std_output, &std_error, (obj_t **) 0);
  std_input = make_stdio_stream(stdin, string("stdin"), t, nil);
  std_output = make_stdio_stream(stdout, string("stdout"), nil, t);
  std_error = make_stdio_stream(stderr, string("stderr"), nil, t);
}
