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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <dirent.h>
#include <setjmp.h>
#include "lib.h"
#include "stream.h"
#include "gc.h"
#include "unwind.h"
#include "parser.h"
#include "match.h"
#include "txr.h"

const char *version = "018";
const char *progname = "txr";
const char *spec_file = "stdin";
obj_t *spec_file_str;

/*
 * Can implement an emergency allocator here from a fixed storage
 * pool, which sets an OOM flag. Program can check flag
 * and gracefully terminate instead of aborting like this.
 */
void *oom_realloc_handler(void *old, size_t size)
{
  fprintf(stderr, "%s: out of memory\n", progname);
  puts("false");
  abort();
}

void help(void)
{
  const char *text =
"\n"
"txr version %s\n"
"\n"
"copyright 2009, Kaz Kylheku <kkylheku@gmail.com>\n"
"\n"
"usage:\n"
"\n"
"  %s [ options ] query-file { data-file }*\n"
"\n"
"The query-file or data-file arguments may be specified as -, in which case\n"
"standard input is used. All data-file arguments which begin with a !\n"
"character are treated as command pipes. Those which begin with a $\n"
"are interpreted as directories to read. Leading arguments which begin\n"
"with a - followed by one or more characters, and which are not arguments to\n"
"options are interpreted as options. The -- option indicates the end of the\n"
"options.\n"
"\n"
"If no data-file arguments sare supplied, then the query itself must open a\n"
"a data source prior to attempting to make any pattern match, or it will\n"
"simply fail due to a match which has run out of data.\n"
"\n"
"options:\n"
"\n"
"-Dvar=value            Pre-define variable var, with the given value.\n"
"                       A list value can be specified using commas.\n"
"-Dvar                  Predefine variable var, with empty string value.\n"
"-q                     Quiet: don't report errors during query matching.\n"
"-v                     Verbose: extra logging from matcher.\n"
"-b                     Don't dump list of bindings.\n"
"-a num                 Generate array variables up to num-dimensions.\n"
"                       Default is 1. Additional dimensions are fudged\n"
"                       by generating numeric suffixes\n"
"-c query-text          The query is read from the query-text argument\n"
"                       itself. The query-file argument is omitted in\n"
"                       this case; the first argument is a data file.\n"
"-f query-file          Specify the query-file as an option argument.\n"
"                       option, instead of the query-file argument.\n"
"                       This allows #! scripts to pass options through\n"
"                       to the utility.\n"
"--help                 You already know!\n"
"--version              Display program version\n"
"\n"
"Options that take no argument can be combined. The -q and -v options\n"
"are mutually exclusive; the right-most one dominates.\n"
"\n"
  ;
  fprintf(stdout, text, version, progname);
}

void hint(void)
{
  fprintf(stderr, "%s: incorrect arguments: try --help\n", progname);
}

obj_t *remove_hash_bang_line(obj_t *spec)
{
  if (!consp(spec))
    return spec;

  {
    obj_t *shbang = string(strdup("#!"));
    obj_t *firstline = first(spec);
    obj_t *items = rest(firstline);

    if (stringp(first(items))) {
      obj_t *twochars = sub_str(first(items), zero, two);
      if (equal(twochars, shbang))
        return rest(spec);
    }

    return spec;
  }
}

int main(int argc, char **argv)
{
  obj_t *stack_bottom_0 = nil;
  obj_t *specstring = nil;
  obj_t *spec = nil;
  obj_t *bindings = nil;
  int match_loglevel = opt_loglevel;
  progname = argv[0] ? argv[0] : progname;
  obj_t *stack_bottom_1 = nil;

  init(progname, oom_realloc_handler, &stack_bottom_0, &stack_bottom_1);

  protect(&spec_file_str, 0);

  yyin_stream = std_input;
  protect(&yyin_stream, 0);

  if (argc <= 1) {
    hint();
    return EXIT_FAILURE;
  }

  argc--, argv++;

  while (argc > 0 && (*argv)[0] == '-') {
    if (!strcmp(*argv, "--")) {
      argv++, argc--;
      break;
    }

    if (!strcmp(*argv, "-"))
      break;

    if (!strncmp(*argv, "-D", 2)) {
      char *var = *argv + 2;
      char *equals = strchr(var, '=');
      char *has_comma = (equals != 0) ? strchr(equals, ',') : 0;

      if (has_comma) {
        char *val = equals + 1;
        obj_t *list = nil;

        *equals = 0;

        for (;;) {
          size_t piece = strcspn(val, ",");
          char comma_p = val[piece];

          val[piece] = 0;

          list = cons(string(strdup(val)), list);

          if (!comma_p)
            break;

          val += piece + 1;
        }

        list = nreverse(list);
        bindings = cons(cons(intern(string(strdup(var))), list), bindings);
      } else if (equals) {
        char *val = equals + 1;
        *equals = 0;
        bindings = cons(cons(intern(string(strdup(var))),
                        string(strdup(val))), bindings);
      } else {
        bindings = cons(cons(intern(string(strdup(var))),
                        null_string), bindings);
      }

      argc--, argv++;
      continue;
    }

    if (!strcmp(*argv, "--version")) {
      printf("%s: version %s\n", progname, version);
      return 0;
    }

    if (!strcmp(*argv, "--help")) {
      help();
      return 0;
    }

    if (!strcmp(*argv, "-a") || !strcmp(*argv, "-c") || !strcmp(*argv, "-f")) {
      long val;
      char *errp;
      char opt = (*argv)[1];

      if (argc == 1) {
        fprintf(stderr, "%s: option %c needs argument\n", progname, opt);

        return EXIT_FAILURE;
      }

      argv++, argc--;

      switch (opt) {
      case 'a':
        val = strtol(*argv, &errp, 10);
        if (*errp != 0) {
          fprintf(stderr, "%s: option %c needs numeric argument, not %s\n",
                  progname, opt, *argv);
          return EXIT_FAILURE;
        }

        opt_arraydims = val;
        break;
      case 'c':
        specstring = string(strdup(*argv));
        break;
      case 'f':
        spec_file_str = string(strdup(*argv));
        break;
      }

      argv++, argc--;
      continue;
    }

    if (!strcmp(*argv, "--gc-debug")) {
      opt_gc_debug = 1;
      argv++, argc--;
      continue;
    }

    {
      char *popt;
      for (popt = (*argv)+1; *popt != 0; popt++) {
        switch (*popt) {
        case 'v':
          match_loglevel = 2;
          break;
        case 'q':
          match_loglevel = 0;
          break;
        case 'b':
          opt_nobindings = 1;
          break;
        case 'a':
        case 'c':
        case 'D':
          fprintf(stderr, "%s: option -%c does not clump\n", progname, *popt);
          return EXIT_FAILURE;
        case '-':
          fprintf(stderr, "%s: unrecognized long option: --%s\n",
                  progname, popt + 1);
          return EXIT_FAILURE;
        default:
          fprintf(stderr, "%s: unrecognized option: %c\n", progname, *popt);
          return EXIT_FAILURE;
        }
      }

      argc--, argv++;
    }
  }

  if (specstring && spec_file_str) {
    fprintf(stderr, "%s: cannot specify both -f and -c\n", progname);
    return EXIT_FAILURE;
  }

  if (specstring) {
    spec_file = "cmdline";
    spec_file_str = string(strdup(spec_file));
    yyin_stream = make_string_input_stream(specstring);
  } else if (spec_file_str) {
    if (strcmp(c_str(spec_file_str), "-") != 0) {
      FILE *in = fopen(c_str(spec_file_str), "r");
      if (in == 0)
        uw_throwcf(file_error, "unable to open %s", c_str(spec_file_str));
      yyin_stream = make_stdio_stream(in, t, nil);
    } else {
      spec_file = "stdin";
    }
  } else {
    if (argc < 1) {
      hint();
      return EXIT_FAILURE;
    }

    if (strcmp(*argv, "-") != 0) {
      FILE *in = fopen(*argv, "r");
      if (in == 0)
        uw_throwcf(file_error, "unable to open %s", *argv);
      yyin_stream = make_stdio_stream(in, t, nil);
      spec_file = *argv;
    } else {
      spec_file = "stdin";
    }
    argc--, argv++;
    spec_file_str = string(strdup(spec_file));
  }


  {
    int gc = gc_state(0);
    yyparse();
    gc_state(gc);

    if (errors)
      return EXIT_FAILURE;
    spec = remove_hash_bang_line(get_spec());

    opt_loglevel = match_loglevel;

    if (opt_loglevel >= 2) {
      format(std_error, "spec:\n~s\n", spec, nao);
      format(std_error, "bindings:\n~s\n", bindings, nao);
    }

    {
      int retval;
      list_collect_decl(filenames, iter);

      while (*argv)
        list_collect(iter, string(*argv++));

      retval = extract(spec, filenames, bindings);

      return errors ? EXIT_FAILURE : retval;
    }
  }
}
