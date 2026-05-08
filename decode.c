#include "stdio.h"
#include "stdint.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int width = 128;
int height = 128;
uint8_t* buffer;


void flush_buffer(const char *ppm_path , const char* png_path){
    FILE *f = fopen(ppm_path,"w");
    fprintf(f,"P3\n");
    fprintf(f,"%d %d\n",width,height);
    fprintf(f,"255\n");

    for (int y = 0;y < height;y++){
        for (int x = 0; x < width; x ++){
            uint8_t r = buffer[(y * width + x)*3 + 0];
            uint8_t g = buffer[(y * width + x)*3 + 1];
            uint8_t b = buffer[(y * width + x)*3 + 2];
            fprintf(f,"%d %d %d\n",r,g,b);
        }
    }
    fclose(f);
    char s[255];
    sprintf(s,"convert %s %s", ppm_path, png_path);
    system(s);
}

int main(int argc,const char** argv){
    printf("JPEG DECODER\n");
    buffer = malloc(width*height*3);
    memset(buffer, 0, height*height*3);

    
    for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
        int index = (y * width + x) * 3;

        buffer[index + 0] = x * 2;
        buffer[index + 1] = y * 2;
        buffer[index + 2] = 128;
    }
    }
    flush_buffer("out.ppm","out.png");
    free(buffer);

    return 0;
}