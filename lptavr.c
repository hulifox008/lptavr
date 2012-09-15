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
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define LPT_BASE    0x378

#define PAGE_SIZE   32      /* 32bytes (16 words) */

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

int avr_sync()
{
    int i;

    unsigned char out[] = {0xAC, 0x53, 0x00, 0x00};
    unsigned char in[4] = {0};

    /* Try 3 times. */
    for(i=0;i<3;i++)
    {
        SCK_lo();
        usleep(1000);
        RST_hi();
        usleep(1000);
        RST_lo();
        usleep(50000);

        spi_transfer(out, in, 4);
        if(0x53 == in[2])
        {
            return 0;
        }
    }

    return -1;
}

unsigned char avr_read_sig(unsigned char reg)
{
    unsigned char out[] = {0x30, 0x00, 0x00, 0x00};
    unsigned char in[4] = {0};

    out[2] = reg&0x03;

    spi_transfer(out, in, 4);
    return in[3];
}

unsigned char avr_busy()
{
    unsigned char out[] = {0xF0, 0x00, 0x00, 0x00};
    unsigned char in[4] = {0};

    spi_transfer(out, in, 4);
    return in[3]&0x01;
}

/* Program one page at page_addr.
 * data must be 32 bytes. 
 */
int avr_program_page(unsigned char* data, unsigned char page_addr)
{
    assert(NULL!=data);

    unsigned char out[4] ={0} ;
    unsigned char in[4] = {0};

    int b;
    for(b=0;b<PAGE_SIZE;b++)
    {
        if(b&0x01)
            out[0] = 0x48;
        else
            out[0] = 0x40;

        out[2] = (b>>1)&0x0F;
        out[3] = data[b];
        spi_transfer(out, in, 4);
    }

    out[0] = 0x4C;
    out[1] = (page_addr>>4)&0x03; 
    out[2] = (page_addr<<4)&0xF0;
    out[3] = 0;
    spi_transfer(out, in, 4);
    while(avr_busy());
    return 0;
}

int avr_erase()
{
    unsigned char out[] = {0xAC, 0x80, 0x00, 0x00};
    unsigned char in[4] = {0};
    spi_transfer(out, in, 4);
    while(avr_busy());
    return 0;
}

unsigned char avr_read(unsigned long addr)
{
    unsigned char out[4] = {0x20, 0x00, 0x00, 0x00};
    unsigned char in[4] = {0};

    out[0] |= (addr&0x01)<<3;
    addr=addr>>1;
    out[1] = (addr>>8)&0x03;
    out[2] = addr&0xFF;

    spi_transfer(out, in, 4);

/*  printf("---> %02X %02X %02X %02X       [%02X]\n", out[0], out[1], out[2], out[3], in[3]); */

    return in[3];
}

int avr_program(const char* filename)
{
    assert(NULL!=filename);

    struct stat stat = {0};
    unsigned char buf[32] = {0};

    int fd = open(filename, O_RDONLY);
    if(-1==fd)
    {
        perror("Cannot open file");
        return -1;
    }

    if(fstat(fd, &stat))
    {
        perror("Cannot access file");
        return -1;
    }

    printf("File size: %lu\n", stat.st_size);

    /* FIXME: Check the chip signature bytes to get the program memory size. */
    if(stat.st_size>2048)
    {
        fprintf(stderr, "File size exceeds 2048 bytes!");
        return -1;
    }

    printf("Erasing the chip ...\n");
    avr_erase();

    unsigned int programed = 0;
    unsigned int readed;
    unsigned char page = 0;
    while((readed=read(fd, buf, PAGE_SIZE))>0)
    {
        int i;

        /* Pad the buffer. */
        for(i=readed;i<PAGE_SIZE;i++)
            buf[i] = 0xFF;

        avr_program_page(buf, page++);
        programed += readed;
    }

    printf("%d bytes programed! [%s]\n", programed, programed==stat.st_size?"Succeed":"Failed");
    close(fd);
}

int avr_verify(const char* filename)
{
    assert(NULL!=filename);

    struct stat stat = {0};

    printf("Verifying ...\n");

    int fd = open(filename, O_RDONLY);
    if(-1==fd)
    {
        perror("Cannot open file");
        return -1;
    }

    if(fstat(fd, &stat))
    {
        perror("Cannot access file");
        close(fd);
        return -1;
    }

    printf("File size: %lu\n", stat.st_size);

    /* FIXME: Check the chip signature bytes to get the program memory size. */
    if(stat.st_size>2048)
    {
        fprintf(stderr, "File size exceeds 2048 bytes!");
        close(fd);
        return -1;
    }

    unsigned char *data = malloc(stat.st_size);
    if(!data)
    {
        perror("malloc() failed");
        close(fd);
        return -1;
    }

    if(stat.st_size != read(fd, data, stat.st_size))
    {
        perror("Cannot read file!");
        close(fd);
        return -1;
    }

    int i;
    int failed=0;
    for(i=0;i<stat.st_size;i++)
    {
        unsigned char c = avr_read(i);
        if(data[i] != c)
        {
            printf("Verify failed at address %08X!   Flash content: %02X  File content: %02X\n", i, c, data[i]);
            failed=1;
        }
    }
    close(fd);
    return failed;
}

int main(int argc, char* argv[])
{
    if(argc!=2)
    {   
        fprintf(stderr, "Usage: lptavr <filename>\n");
        return 1;
    }

    iopl(3);

    if(avr_sync())
    {
        fprintf(stderr, "Cannot sync to the devices. Check connection.\n");
    }

    printf("Device signature: %02X %02X %02X\n", avr_read_sig(0), avr_read_sig(1), avr_read_sig(2));

    avr_program(argv[1]);
    avr_verify(argv[1]);

    /* release RST */
    RST_hi();
    return 0;
}
