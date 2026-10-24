# Host-side tests for the editor core (no hardware needed).
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined
CORE = storage_host.cpp ../src/editor/text_buffer.cpp ../src/editor/layout.cpp

run: fwtest reopentest syntaxtest epub x3
	./fwtest
	./reopentest
	./syntaxtest

fwtest: test_main.cpp $(CORE) ../src/editor/*.h
	$(CXX) $(CXXFLAGS) -o $@ test_main.cpp $(CORE)

reopentest: reopen_test.cpp $(CORE) ../src/editor/*.h
	$(CXX) $(CXXFLAGS) -o $@ reopen_test.cpp $(CORE)

syntaxtest: syntax_test.cpp ../src/editor/syntax.cpp ../src/editor/syntax.h
	$(CXX) $(CXXFLAGS) -o $@ syntax_test.cpp ../src/editor/syntax.cpp

EPUB_SRC = ../src/export/epub.cpp storage_host.cpp
epubtest: epub_test.cpp $(EPUB_SRC) ../src/export/epub.h
	$(CXX) $(CXXFLAGS) -o $@ epub_test.cpp $(EPUB_SRC)

# Converts the samples in epub/ and checks the results (python3 zipfile +
# XML well-formedness; epubcheck too when it is installed).
epub: epubtest
	for f in epub/*.md epub/*.txt epub/*.cpp; do ./epubtest $$f $${f%.*}.epub || exit 1; done
	python3 check_epub.py epub/*.epub

X3_SRC = ../src/net/x3.cpp net_posix.cpp storage_host.cpp
x3test: x3_test.cpp $(X3_SRC) ../src/net/x3.h ../src/net/net.h
	$(CXX) $(CXXFLAGS) -o $@ x3_test.cpp $(X3_SRC)

# Upload client against a local mock of CrossPoint's file-transfer server.
x3: x3test epub
	rm -rf /tmp/x3sd; python3 mock_crosspoint.py 8765 /tmp/x3sd & echo $$! > /tmp/mock.pid; sleep 1; \
	./x3test http://127.0.0.1:8765/ epub/rich.epub; r=$$?; kill `cat /tmp/mock.pid`; \
	cmp epub/rich.epub "/tmp/x3sd/Books/Il mio racconto è.epub" && exit $$r

clean:
	rm -f fwtest reopentest syntaxtest epubtest x3test epub/*.epub
