decode: decode.c
	gcc -o decode decode.c -lm

build: decode.c
	make clean
	gcc -o decode decode.c -lm
	make run

clean:
	rm -f decode out.ppm out.png

run:
	./decode 44-baseline.jpg