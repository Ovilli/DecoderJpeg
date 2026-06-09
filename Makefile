decode: decode.c
	gcc -o decode decode.c


build: decode.c
	make clean
	gcc -o decode decode.c
	make run


clean:
	rm decode
	rm out.ppm
	rm out.png

run:
	./decode