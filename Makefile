build: decode.c
	gcc -o decode decode.c

clean:
	rm decode
	rm out.ppm
	rm out.png