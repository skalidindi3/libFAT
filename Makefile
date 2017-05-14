main: main.cpp fat32.h
	clang++ -std=c++11 -o main main.cpp

.PHONY: info
info:
	hdiutil imageinfo ./floppy.img

clean:
	rm main
