./txr.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./stream.h $(top_srcdir)/./gc.h $(top_srcdir)/./unwind.h $(top_srcdir)/./parser.h $(top_srcdir)/./match.h $(top_srcdir)/./utf8.h $(top_srcdir)/./debug.h $(top_srcdir)/./txr.h
./lex.yy.o: config.h $(top_srcdir)/./lib.h y.tab.h $(top_srcdir)/./gc.h $(top_srcdir)/./stream.h $(top_srcdir)/./utf8.h $(top_srcdir)/./unwind.h $(top_srcdir)/./hash.h $(top_srcdir)/./parser.h
./y.tab.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./unwind.h $(top_srcdir)/./regex.h $(top_srcdir)/./utf8.h $(top_srcdir)/./match.h $(top_srcdir)/./hash.h $(top_srcdir)/./eval.h $(top_srcdir)/./parser.h
./match.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./gc.h $(top_srcdir)/./unwind.h $(top_srcdir)/./regex.h $(top_srcdir)/./stream.h $(top_srcdir)/./parser.h $(top_srcdir)/./txr.h $(top_srcdir)/./utf8.h $(top_srcdir)/./filter.h $(top_srcdir)/./hash.h $(top_srcdir)/./debug.h $(top_srcdir)/./eval.h $(top_srcdir)/./match.h
./lib.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./gc.h $(top_srcdir)/./arith.h $(top_srcdir)/./rand.h $(top_srcdir)/./hash.h $(top_srcdir)/./unwind.h $(top_srcdir)/./stream.h $(top_srcdir)/./utf8.h $(top_srcdir)/./filter.h $(top_srcdir)/./eval.h
./regex.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./unwind.h $(top_srcdir)/./regex.h $(top_srcdir)/./txr.h
./gc.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./stream.h $(top_srcdir)/./hash.h $(top_srcdir)/./txr.h $(top_srcdir)/./eval.h $(top_srcdir)/./gc.h
./unwind.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./gc.h $(top_srcdir)/./stream.h $(top_srcdir)/./txr.h $(top_srcdir)/./unwind.h
./stream.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./gc.h $(top_srcdir)/./unwind.h $(top_srcdir)/./stream.h $(top_srcdir)/./utf8.h
./arith.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./unwind.h $(top_srcdir)/./gc.h $(top_srcdir)/./arith.h
./hash.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./gc.h $(top_srcdir)/./unwind.h $(top_srcdir)/./hash.h
./utf8.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./unwind.h $(top_srcdir)/./utf8.h
./filter.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./hash.h $(top_srcdir)/./unwind.h $(top_srcdir)/./match.h $(top_srcdir)/./filter.h $(top_srcdir)/./gc.h
./debug.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./debug.h $(top_srcdir)/./gc.h $(top_srcdir)/./unwind.h $(top_srcdir)/./stream.h $(top_srcdir)/./parser.h
./eval.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./gc.h $(top_srcdir)/./unwind.h $(top_srcdir)/./regex.h $(top_srcdir)/./stream.h $(top_srcdir)/./parser.h $(top_srcdir)/./hash.h $(top_srcdir)/./debug.h $(top_srcdir)/./match.h $(top_srcdir)/./rand.h $(top_srcdir)/./eval.h
./rand.o: config.h $(top_srcdir)/./lib.h $(top_srcdir)/./unwind.h $(top_srcdir)/./gc.h $(top_srcdir)/./arith.h $(top_srcdir)/./rand.h
mpi-1.8.6/mpi.o: $(top_srcdir)/mpi-1.8.6/../config.h $(top_srcdir)/mpi-1.8.6/mpi.h $(top_srcdir)/mpi-1.8.6/logtab.h
mpi-1.8.6/mplogic.o: $(top_srcdir)/mpi-1.8.6/../config.h $(top_srcdir)/mpi-1.8.6/mplogic.h
