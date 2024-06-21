#ifndef LCD_H
#define LCD_H

#include "spiAVR.h"
#define uint unsigned int
#define uchar unsigned char
#define ushort unsigned short

void sendcmd(uchar index);
void write(uchar data);
void senddata(uint data);
void setcol(uint start, uint end);
void setpage(uint start, uint end);
void fillscr(void);
void fillscr(ushort x0, ushort x1, ushort y0, ushort y1, uint color);
void drawcirc(ushort x, ushort y, ushort r, uint color);
void drawline(ushort x0, ushort x1, ushort y0, ushort y1, uint color);
void drawline(ushort x, ushort y, ushort len, uint color);
void initlcd(void);
uchar setpx(ushort x, ushort y, uint color);


void sendcmd(uchar index)
{
    PORTD &= ~0x40; // DC LOW;
    PORTD &= ~0x20; // CS LOW;
    SPI_SEND(index);
    PORTD |=  0x20; // CS HIGH;
}

void write(uchar data)
{
    PORTD |= 0x40; // DC HIGH;
    PORTD &= ~0x20; // CS LOW;
    SPI_SEND(data);
    PORTD |=  0x20; // CS HIGH;
}

void senddata(uint data)
{
    uchar data1 = data>>8;
    uchar data2 = data&0xff;
    PORTD |= 0x40; // DC HIGH;
    PORTD &= ~0x20; // CS LOW;
    SPI_SEND(data1);
    SPI_SEND(data2);
    PORTD |=  0x20; // CS HIGH;
}

void setcol(uint start,uint end)
{
    sendcmd(0x2A);
    senddata(start);
    senddata(end);
}

void setpage(uint start,uint end)
{
    sendcmd(0x2B);                                    
    senddata(start);
    senddata(end);
}

void fillscr(void)
{
    setcol(0, 239);
    setpage(0, 319);
    sendcmd(0x2c);                                                  
                                                                      

    PORTD |= 0x40; // DC HIGH;
    PORTD &= ~0x20; // CS LOW;
    for(uint i=0; i<38400; i++)
    {
        SPI_SEND(0);
        SPI_SEND(0);
        SPI_SEND(0);
        SPI_SEND(0);
    }
    PORTD |=  0x20; // CS HIGH;
}

void fillscr(ushort x0, ushort x1, ushort y0, ushort y1, uint color){
    setcol(x0, x1);
    setpage(y0, y1);
    sendcmd(0x2C);

    PORTD |= 0x40; // DC HIGH;
    PORTD &= ~0x20; // CS LOW;

    uint area = (x1 - x0 + 1) * (y1 - y0 + 1);
    uchar colorh = color >> 8;
    uchar colorl = color & 0xFF;
    for (uint i = 0; i < area; i++) {
        SPI_SEND(colorh);
        SPI_SEND(colorl);
    }
    PORTD |=  0x20; // CS HIGH;
    
}

void drawcirc(ushort x0, ushort y0, ushort r, uint color)
{
    int x = -r, y = 0, err = 2-2*r, e2;
    do {

        drawline(x0-x, y0-y, 2*y, color);
        drawline(x0+x, y0-y, 2*y, color);

        e2 = err;
        if (e2 <= y) {
            err += ++y*2+1;
            if (-x == y && e2 <= x) e2 = 0;
        }
        if (e2 > x) err += ++x*2+1;
    } while (x <= 0);

}

void drawline(ushort x0, ushort y0, ushort len, uint color){
    setcol(x0,x0);
    setpage(y0,y0 + len);
    sendcmd(0x2C);
    for(int i = 0; i < len; i++) senddata(color);
}

int abs(int x){
    x = (x < 0) ? -x : x;
    return x;
}

void drawline(ushort x0, ushort x1, ushort y0, ushort y1, uint color){
    short y = y1-y0;
    short x = x1-x0;
    short dx = abs(x), sx = x0<x1 ? 1 : -1;
    short dy = -abs(y), sy = y0<y1 ? 1 : -1;
    short err = dx+dy, e2;                                                /* error value e_xy             */
    while (1){                                                           /* loop                         */
        setpx(x0,y0,color);
        e2 = 2*err;
        if (e2 >= dy) {                                                 /* e_xy+e_x > 0                 */
            if (x0 == x1) break;
            err += dy; x0 += sx;
        }
        if (e2 <= dx) {                                                 /* e_xy+e_y < 0                 */
            if (y0 == y1) break;
            err += dx; y0 += sy;
        }
    }
}

#endif