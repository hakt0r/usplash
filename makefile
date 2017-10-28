build_clean:
	rm -f *.gch
	rm -f *.dbg

udg: build_clean
	#gcc -rdynamic -Os -std=c11 -o udg udg.c
	diet gcc -static -Os -std=c11 -o udg udg.c
	strip ./udg
	du -h udg
udg.dbg: build_clean
	gcc -O0 -fbuiltin -g -std=c11 -o udg.dbg udg.c -D_DEFAULT_SOURCE
udg-valgrind: udg.dbg
	DEBUG=true valgrind --tool=memcheck --log-file=valgrind.log -v \
  --track-fds=yes --track-origins=yes \
	--leak-check=full --free-fill=be \
	./udg.dbg -D -o CTRL_C -a 'ls' -p haha
  # --show-leak-kinds=all
udg-nemiver: udg.dbg
	DEBUG=true nemiver ./udg.dbg -D -o CTRL_D -a 'killall udg' -p haha

usplash: build_clean
	#gcc -rdynamic -Os -std=c11 -o usplash usplash.c
	diet gcc -Os --static -std=c11 -o usplash usplash.c
	strip usplash && du -h usplash && md5sum usplash && sha1sum usplash
usplash.dbg: build_clean
	gcc -O0 -fbuiltin -g -std=c11 -o usplash.dbg  usplash.c -D_DEFAULT_SOURCE
usplash-gdb: usplash.dbg
	DEBUG=true gdb ./usplash.dbg
usplash-nemiver: usplash.dbg
	DEBUG=true nemiver ./usplash.dbg -m
usplash-valgrind: usplash.dbg
	DEBUG=true valgrind --tool=memcheck --log-file=valgrind.log -v \
  --track-fds=yes --track-origins=yes \
	--leak-check=full --free-fill=be \
	./usplash.dbg -D -m
  # --show-leak-kinds=all
usplash-tests:
	./usplash.dbg -i a -t no_a   -a 'echo this is a.0';
	./usplash.dbg -i a -t no_a.1 -a 'echo this is a.1';
	./usplash.dbg -i a -t no_a.2 -a 'echo this is a.2';
	./usplash.dbg -i a -t no_a.3 -a 'echo this is a.3';
	./usplash.dbg -i b -t no_b   -a 'echo this is b.0';
	./usplash.dbg -i b -t no_b.1 -a 'echo this is b.1';
	./usplash.dbg -i b -t no_b.2 -a 'echo this is b.2';
	# index-test
	./usplash.dbg -i d:a -t AAA -a 'echo ABC:a';
	./usplash.dbg -i d:b -t BBB -a 'echo ABC:b';
	./usplash.dbg -i d:c -t CCC -a 'echo ABC:c';
	./usplash.dbg -d d:c
	./usplash.dbg -d d
	# load-test
	for i in $$(seq 0 1000); do sleep 0.01; ./usplash.dbg -i x -t load:$$i  -a ls $$i & done
	# action
	rm -f ./testfile
	./usplash.dbg -i c -t testcase -a 'touch ./testfile'
	./usplash.dbg -r c || echo ok-failed
	sleep 0.1 && sync && test -e ./testfile
	rm -f ./testfile
	./usplash.dbg -d a
	./usplash.dbg -d b
	./usplash.dbg -q :


contrib/lib/upscaledb.a:
	sh tools/build_upscaledb.sh

taskd-release: build_clean
	diet gcc -Os --static -std=c11 -o taskd taskd.c
	strip taskd && du -h taskd && md5sum taskd && sha1sum taskd
taskd-debug: build_clean
	gcc -g -std=c11 -o taskd.dbg  taskd.c -D_DEFAULT_SOURCE
taskd-gdb: build_clean taskd-debug
	DEBUG=true gdb ./taskd.dbg daemon
taskd-nemiver: build_clean taskd-debug
	DEBUG=true nemiver ./taskd.dbg daemon
taskd-test: taskd-debug
	./taskd daemon
taskd-valgrind: taskd-debug
	DEBUG=true valgrind --tool=memcheck --log-file=valgrind.log -v \
  --track-fds=yes --track-origins=yes \
	--leak-check=full --free-fill=be \
	./taskd.dbg daemon
