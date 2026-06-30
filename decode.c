#include "stdio.h"
#include <endian.h>
#include <math.h>
#include <stddef.h>
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

typedef struct{
    int v[16][16][3];
}MCU;

HuffmannTable dc_table[4];
HuffmannTable ac_table[4];
uint8_t bit_buffer = 0;
uint8_t bits_left = 0;
HuffmannTable* use_dc[4];
HuffmannTable* use_ac[4];
uint8_t component_qt[4];
int last_dc[4] = {0,0,0,0};


int zigzag[64] = {
     0, 1, 8,16, 9, 2, 3,10,17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34,27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};

void init_buffer(int width, int height){
    buffer = malloc(width*height*3);
    memset(buffer, 0, width*height*3);
}


uint8_t read_byte(){
    int b = fgetc(f);
    if(b == EOF){ printf("unexpected EOF\n"); exit(1); }
    return (uint8_t)b;
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


    for(int i = 0;i < 64; i++)
        q_tables[tq][zigzag[i]] = read_byte();
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
    component_qt[1] = read_byte();
    assert(read_byte() == 0x02,"0:2");
    assert(read_byte() == 0x11,"1:1");
    component_qt[2] = read_byte();
    assert(read_byte() == 0x03,"0:3");
    assert(read_byte() == 0x11,"1:1");
    component_qt[3] = read_byte();

    printf("Read SOF0 marker : image is %d x %d pixel\n ",width,height);
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
uint8_t next_entropy_byte(){
    uint8_t b = read_byte();
    if(b == 0xFF){
        uint8_t next = read_byte();
        if(next != 0x00){
            // real marker mid-scan (EOI / RSTn) — baseline w/o restart shouldn't hit this
            printf("marker FF%02X in scan\n", next);
        }
        // else: stuffed 00, the real byte is 0xFF
    }
    return b;
}
int read_huffman(){
    if (bits_left == 0){
        bit_buffer = next_entropy_byte();
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
    if(n == 0) return 0;
    int cal_value = 0;
    for(int i = 0; i < n; i++)
        cal_value = (cal_value << 1) | read_huffman();

    if(!(cal_value >> (n - 1)))      // top bit 0 -> negative value
        cal_value -= (1 << n) - 1;
    return cal_value;                // top bit 1 -> positive, return as-is
}

int read_dc_diff(HuffmannTable* ht){
    int symbol = read_huffman_symbols(ht);
    int dc_diff = read_n_bits_sign_extend(symbol);
    return dc_diff;

}
int clamp_to_byte(int value){
    if(value < 0) return 0;
    if(value > 255 ) return 255;
    return value;
}

void  inverse_dct(Block* block){
    int input[64];

    for(int i = 0; i < 64; i++)
        input[i] = block->coefficient[i];

    for(int y = 0; y < 8;y++){
        for(int x =0;x < 8; x++){
            double sum = 0.0;

            for(int v = 0; v < 8; v++){
                for(int u = 0; u <8;u++){double cu=(u==0)?1.0/sqrt(2.0):1.0;
                double cv=(v==0)?1.0/sqrt(2.0):1.0;

                double bx = cos(((2* x + 1)*u*M_PI)/16.0);
                double by = cos(((2*y+1)*v*M_PI)/16.0);

                sum += cu * cv * input[v * 8 + u] * bx * by;}
            }
            int value = round(0.25 * sum) + 128;
            block->coefficient[y*8+x] = clamp_to_byte(value);
        }

    }
}

void read_block(Block* block,int channel){
    // zero black
    memset(block->coefficient, 0, sizeof(block->coefficient));

    // Read DC
    int dc_diff = read_dc_diff(use_dc[channel]);
    int dc = last_dc[channel] + dc_diff;
    last_dc[channel] = dc;
    block->coefficient[0] = dc;

    // Read AC
   int offset = 1;
   while (offset<64) {
        uint8_t symbol = read_huffman_symbols(use_ac[channel]);
        // printf("AC symbol:%02x\n",symbol);

        if(symbol == 0x00){
            offset = 64;
        }else if (symbol == 0xF0){
            offset += 16;
        } else {
            uint8_t leading_zeros =  symbol >> 4;
            uint8_t bits = symbol & 15;
            offset += leading_zeros;
            int ac = read_n_bits_sign_extend(bits);

            // printf("AC coefficent %d\n",ac);
            //block->coefficient[offset] = ac;
            block->coefficient[zigzag[offset]] = ac;
            offset++;
        }
   }

   // de-quantize block
   int quant_table_index = component_qt[channel];
   for(int i = 0; i < 64;i++)block->coefficient[i] *= q_tables[quant_table_index][i];

   // perform iDCT
   // nah we fill the entire block
   // with average value
   // EHEHE

   //int avg = block->coefficient[0] / 8 + 128;
   //if (avg < 0) avg = 0;
   //if(avg>255) avg = 255;
   //for(int i = 0; i < 64; i++) block->coefficient[i] = avg;
   inverse_dct(block);
}

void ycbcr_to_rgb(int y, int cb , int cr , int* r, int* g, int* b){
    *r = round(y +1.402 * (cr - 128));
    *g = round(y - 0.344136 * (cb - 128)-0.714136*(cr-128));
    *b = round(y + 1.772 * (cb - 128));

    if(*r < 0x00) *r = 0x00;
    if(*r > 0xFF) *r = 0XFF;
    if(*g < 0x00) *g = 0x00;
    if(*g > 0xFF) *g = 0XFF;
    if(*b < 0x00) *b = 0x00;
    if(*b > 0xFF) *b = 0XFF;
};

void parse_sos(){
    uint16_t lenght = read_word();
    assert(lenght==12, "SOS : segment lenght 12 it is ");
    uint8_t ns = read_byte();
    assert(ns==3, "SOS : 3 image Componentents it has");
    
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

    //int dc_diff = read_dc_diff(use_dc[1]);
    //int dc = last_dc[1]+dc_diff;
    //last_dc[1]=dc;
    //printf("DC coefficient is%d\n",dc);

    int mcu_y = 0,mcu_x = 0;
    
    while ((mcu_y < height)) {
        printf("%d - %d\n", mcu_x, mcu_y);
        Block y00,y10,y01,y11,cb,cr;
        read_block(&y00,1);
        read_block(&y10,1);
        read_block(&y01,1);
        read_block(&y11,1);
        read_block(&cb,2);
        read_block(&cr,3);

        MCU mcu;
        memset(mcu.v,0,sizeof(mcu.v));

        for (int y = 0; y < 8; y++){
            for(int x = 0;x < 8;x++){
                mcu.v[y][x][0] = y00.coefficient[y * 8 + x];
                mcu.v[y][x+8][0] = y10.coefficient[y * 8 + x];
                mcu.v[y+8][x][0] = y01.coefficient[y * 8 + x];
                mcu.v[y+8][x+8][0] = y11.coefficient[y * 8 + x];

                for(int dx = 0;dx<2;dx++){
                    for (int dy = 0; dy < 2; dy++) {
                        mcu.v[y*2 + dy][x * 2 + dx][1] =  cb.coefficient[y * 8 + x];
                        mcu.v[y*2 + dy][x * 2 + dx][2] =  cr.coefficient[y * 8 + x];

                    }
                }
            }
        }
        for (int y = 0; y < 16; y++){
            for(int x = 0; x < 16; x++){
                if(mcu_x + x > width || mcu_y + y >= height) continue;
                int r,g,b;
                ycbcr_to_rgb(mcu.v[y][x][0],mcu.v[y][x][1],mcu.v[y][x][2],&r,&g,&b);
                set_pixel(mcu_x + x , mcu_y + y,r,g,b);
            }
        }
        mcu_x += 16;
        if(mcu_x >= width){
            mcu_x = 0;
            mcu_y += 16;
        }
    }


    

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

    int finished = 0;
    while (!finished){
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
                finished = 1;
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