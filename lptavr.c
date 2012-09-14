/* Using LPT port to program AVR. Using SPI serial programing mode. 
 *
 * Connection:
 *
 *     LPT              AVR
 *     Bit0             MOSI
 *     Bit1             SCK
 *     Bit2             RST
 *     _BUSY            MISO
 *
 */

#include <sys/io.h>
#include <unistd.h>

#define LPT_BASE    0x378

inline void MOSI_hi()
{
    outb(inb(LPT_BASE) | 0x01, LPT_BASE);
}

inline void MOSI_lo()
{
    outb(inb(LPT_BASE) & ~0x01, LPT_BASE);
}

inline void SCK_hi()
{
    outb(inb(LPT_BASE) | 0x02, LPT_BASE);
}

inline void SCK_lo()
{
    outb(inb(LPT_BASE) & ~0x02, LPT_BASE);
}

inline void RST_hi()
{
    outb(inb(LPT_BASE) | 0x04, LPT_BASE);
}

inline void RST_lo()
{
    outb(inb(LPT_BASE) & ~0x4, LPT_BASE);
}

inline int  MISO()
{
    return !(inb(LPT_BASE+1) & 0x80);
}


int spi_transfer(unsigned char *out, unsigned char *in, int len)
{
    int i, b;
    unsigned c;
    for(i=0;i<len;i++)
    {
        c = out[i];
        unsigned char r=0;
        for(b=0;b<8;b++)
        {
            r=r<<1;
            if(MISO())
                r=r|0x01;

            if(c&0x80)
                MOSI_hi();
            else
                MOSI_lo();
            c=c<<1;

            SCK_hi();
            SCK_lo();
        }

        in[i] = r;
    }
    return 0;
}

int main()
{
    iopl(3);


    while(1)
    {
    SCK_lo();
    usleep(1000);
    RST_hi();
    usleep(1000);
    RST_lo();
    usleep(50000);

        unsigned char out[] = {0xAC, 0x53, 0x00, 0x00};
        unsigned char in[4] = {0};
        spi_transfer(out, in, 4);
    printf("%02X %02X %02X %02X \n", in[0], in[1], in[2], in[3]);

    out[0] = 0x30;
    out[1] = 0x00;
    out[2] = 0x02;
        spi_transfer(out, in, 4);
    printf("%02X %02X %02X %02X \n", in[0], in[1], in[2], in[3]);
    sleep(1);    
    }

    return 0;
}
