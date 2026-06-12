#include "stdio.h"
#include <endian.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

int width = 160;
int height = 90;
uint8_t* buffer;
FILE* f;

uint8_t q_tables[4][64];


void start_up(){
    printf("###########################\n");
    printf("DECODE.C    MADE BY SPECHT\n ");
    printf("###########################\n");
}

typedef struct{
    uint8_t count[17];
    uint8_t symbols[256];

    int mincode[17];
    int maxcode[17];
    int valprt[17];
}HuffmannTable;

typedef struct{
    int coefficient[64];
}Block;

HuffmannTable dc_table[4];
HuffmannTable ac_table[4];
uint8_t bit_buffer = 0;
uint8_t bits_left = 0;

void init_buffer(int width, int height){
    buffer = malloc(width*height*3);
    memset(buffer, 0, height*height*3);
}


uint8_t read_byte(){
    uint8_t b = fgetc(f);
    return b;
}

void flush_buffer(const char *ppm_path , const char* png_path){
    FILE *f = fopen(ppm_path,"w");
    fprintf(f,"P3\n");
    fprintf(f,"%d %d\n",width,height);
    fprintf(f,"255\n");


    int z = 0;
    for(int i = 0; i < width * height * 3;i++){
        uint8_t b = buffer[i];
        fprintf(f,"%d \n",b);
    }
    fclose(f);
    char s[255];
    sprintf(s,"convert %s %s", ppm_path, png_path);
    system(s);
}
void set_pixel(int x,int y, int r,int g, int b){

    if (x >= width || x < 0){
        return;
    }
    if(y >= height || y < 0){
        return;
    }
    int offset = (y*width+x)*3;
    buffer[offset + 0] = r;
    buffer[offset + 1] = g;
    buffer[offset + 2] = b;
} 

uint16_t read_word(){
    uint8_t b0 = fgetc(f);
    uint8_t b1 = fgetc(f);
    return((uint16_t)b0 << 8) | b1;
}


void assert(int condition , const char* message){
    if(!condition){
        printf("Assertion Faild : NOT! %s\n",message);
        exit(1);
    }
}

void parse_app0(){
    uint16_t lenght = read_word();
    fseek(f,lenght - 2,SEEK_CUR);
}

void parse_dqt(){
    uint16_t lenght = read_word();
    assert(lenght==67,"DQT: exactly one Quantisation Table\n");

    uint16_t pf = read_byte();
    uint16_t pq = pf >> 4;
    uint16_t tq = pf & 15; // 00001111 = > last 4 Bits 

    assert(pq == 0,"DQT bit precision\n");
    assert(tq < 4,"DQT , slot in range\n");


    fread(q_tables[tq],64,1,f);
    printf("DQT: Read Table #%d\n",tq);
}

void parse_sof0(){
    uint16_t lenght = read_word();
    assert(lenght==17,"SOF0: lenght");

    uint8_t p = read_byte();
    assert(p==8,"SQT0 sample precision"); 
    height = read_word();
    width = read_word();

    init_buffer(width,height);
    uint8_t nf = read_byte();
    assert(nf==3, "SOF0 number of image Components");
    assert(read_byte() == 0x01,"1:1");
    assert(read_byte() == 0x22,"2:2");
    assert(read_byte() == 0x00,"0:0");
    assert(read_byte() == 0x02,"0:2");
    assert(read_byte() == 0x11,"1:1");
    assert(read_byte() == 0x01,"0:1");
    assert(read_byte() == 0x03,"0:3");
    assert(read_byte() == 0x11,"1:1");
    assert(read_byte() == 0x01,"0:1");
}

void parse_dht(){
    uint16_t lenght = read_word();
    uint8_t pf = read_byte();

    uint8_t tc = pf >> 4;
    uint8_t th = pf & 15;

    assert(tc < 2, "DHT DC or ACE Table deteceted in has");
    assert(th < 4,"DHT : not exeeed bounts 0...3 " );

    HuffmannTable* ht = (tc == 0) ? (&dc_table[th]) : (&ac_table[th]);
    printf("DHT : Reading %s table #%d.\n ",tc == 0  ? "DC" : "AC" , th);
    int symbol_count = 0;
    for (int i = 1; i <=16; i++){
        uint8_t count = read_byte();
        ht->count[i] = count;
        symbol_count += count;
    }
    printf("DHT : We have %d symbols in %s table %d\n",symbol_count,tc==0? "DC ":"AC ",th);
    fread(ht->symbols,1,symbol_count,f);

    int code = 0;
    int p = 0;
    for (int i = 1; i <= 16; i ++){
        if(ht->count[i] == 0){
            ht->mincode[i]=-1;
            ht->maxcode[i]=-1;
            ht->valprt[i]=-1;
        }else {
            ht->mincode[i] = code;
            ht->maxcode[i] = code+ ht->count[i] - 1;
            ht->valprt[i] = p;
        }
        p += ht->count[i];
        code += ht->count[i];
        code <<= 1;
        //printf("Huffman Code %d \n",code);
       // printf("Huffman p %d \n",p);   
    }

}
int read_huffman(){
    if (bits_left == 0){
        bit_buffer =  read_byte();
        bits_left = 8;
    }
    int value;
    value = bit_buffer >> 7;
    bit_buffer <<= 1;
    bits_left--;
    return value;
}

int read_huffman_symbols(HuffmannTable* ht){
    int code = 0;
    for(int i = 1; i  <= 16; i++){
        code <<=1;
        code += read_huffman();
            if(code >= ht->mincode[i] && code <= ht->maxcode[i]){
                int entry = ht->valprt[i];
                entry += code - ht->mincode[i];
                return ht->symbols[entry];
            }
    }
}
int read_n_bits_sign_extend(int n){
    int cal_value = 0;
    for(int i = 0; i < n ; i++){
        uint8_t bit = read_huffman();
        cal_value <<=1;
        cal_value += bit;
    }
    if(!(cal_value >> (n-1))){
        cal_value -= (1<<n)-1;
        return cal_value;
    }
}

int read_dc_diff(HuffmannTable* ht){
    int symbol = read_huffman_symbols(ht);
    int dc_diff = read_n_bits_sign_extend(symbol);
    return dc_diff;

}


void parse_sos(){
    uint16_t lenght = read_word();
    assert(lenght==12, "SOS : segment lenght 12 it is ");
    uint8_t ns = read_byte();
    assert(ns==3, "SOS : 3 image Componentents it has");
    HuffmannTable* use_dc[4];
    HuffmannTable* use_ac[4];
    for(int i = 0; i < ns ; i ++){
        uint8_t cs = read_byte();
        assert(cs <= 8, "SOS : 0 < cs < 7");
        uint8_t pf = read_byte();
        uint8_t dc = pf >> 4;
        uint8_t ac = pf & 15;
        use_dc[cs] = &dc_table[dc];
        use_ac[cs] = &ac_table[ac];
    }
    uint8_t ss = read_byte();
    uint8_t se = read_byte();
    uint8_t sa = read_byte();
    assert(ss == 0, "SOS : specturm start at == 0");
    assert(se==63,"SOS : spectral end == 63");
    printf("Reading Huffmancodes ... \n");

    // Test to read Huffman
    //for (int i = 0; i < 16; i++){
    //    int bit = read_huffman();
    //    printf("Bit : %d\n",bit);
    //}

    //int symbol = read_huffman_symbols(&dc_table[0]);
    //printf("First DC Luma symbols is %d\n",symbol);
    //printf("First DC : reading somthing %d\n",read_n_bits_sign_extend(symbol));

    int last_dc[4] = {0,0,0,0};
    int dc_diff = read_dc_diff(use_dc[1]);
    int dc = last_dc[1]+dc_diff;
    last_dc[1]=dc;

}


int main(int argc,const char** argv){
    start_up();

    //init_buffer(width, height);
    
    // Test for Colors with Set Pixel
    //for(int x_pix = 0; x_pix < width; x_pix++){
    //    for(int y_pix = 0;y_pix < height;y_pix++){
    //        set_pixel(x_pix, y_pix, x_pix*10, x_pix*10, y_pix*x_pix*8);
    //    }
    //}

    f = fopen(argv[1],"r");

    uint16_t marker = read_word();
    printf("READ : read marker :  %x\n",marker);
    assert(marker == 0xffd8,"JPEG marker found");

    while (1){
        marker = read_word();
        printf("READ : read marker %x\n",marker);
        switch (marker) {
            case 0xFFE0:
                parse_app0();
                break;
            case 0XFFDB:
                parse_dqt();
                break;
            case 0XFFC0:
                parse_sof0();
                break;
            case 0XFFC4:
                parse_dht();
                break;
            case 0xFFDA:
                parse_sos();
                break;
            default:
                printf("HELP !\n");
                exit(1);
        }
    }
    
    fclose(f);
    flush_buffer("out.ppm","out.png");
    free(buffer);

    return 0;
}

