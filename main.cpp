
#include "spiAVR.h"
#include "timerISR.h"
#include "serialATmega.h"
/* work smarter */
#define uchar unsigned char
#define ushort unsigned short
#define uint unsigned int
/* colors */
#define red   0xF800
#define green 0x07E0
#define blue  0x001f
#define black 0x0000
/* dimensions */
#define min_x 0
#define min_y 0
#define max_x 239
#define max_y 319


//TODO: declare variables for cross-task communication

/* You have 5 tasks to implement for this lab */
#define NUM_TASKS 1


//Task struct for concurrent synchSMs implmentations
typedef struct _task{
	signed 	 char state; 		//Task's current state
	unsigned long period; 		//Task period
	unsigned long elapsedTime; 	//Time elapsed since last task tick
	int (*TickFct)(int); 		//Task tick function
} task;



const unsigned long GCD_PERIOD = /* TODO: Calulate GCD of tasks */ 1;

task tasks[NUM_TASKS]; // declared task array with 5 tasks

void TimerISR() {
	for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {                 
		if ( tasks[i].elapsedTime == tasks[i].period ) {           
			tasks[i].state = tasks[i].TickFct(tasks[i].state); 
			tasks[i].elapsedTime = 0;                         
		}
		tasks[i].elapsedTime += GCD_PERIOD;                       
	}
}

typedef enum dir {up,down};
typedef struct laser_struct{
    ushort x0, y0, y1;
    ushort dead = 1;
    uint color;
    dir ldir;
};

void sendcmd(uchar index);
void write(uchar data);
void senddata(uint data);
void setcol(uint start, uint end);
void setpage(uint start, uint end);
void fillscr(void);
void lcdinit(void);
uchar setpx(uint x, uint y, uint color);
int  tick_lcd(int state);
uchar move_laser(laser_struct* laser);
void mklaser(ushort x, ushort y, dir laser_dir, uint color);

typedef enum lcd_state{
    update
};

const uchar LSR_MAX = 20;
uchar LSR_CNT = 0;
laser_struct* lasers[LSR_MAX];

int main(void) {
  /*
  To initialize a pin as an output, you must set its DDR value as ‘1’ and then set its PORT value as a ‘0’. 
  To initialize a pin as an input, you do the opposite: you must set its DDR value as ‘0’ and its PORT value as a ‘1’.
  */
    serial_init(9600);
    DDRD = 0b11110000; PORTD = ~0b11110000;
    DDRB = 0b00101000; PORTB = ~0b00101000;
    lcdinit();

    // mklaser(110, 10,down,green);
    // mklaser(100,300,up,blue);
    // mklaser(80,300,up,red);



    tasks[0].period = 10;
    tasks[0].state = update;
    tasks[0].elapsedTime = 0;
    tasks[0].TickFct = &tick_lcd;

    TimerSet(GCD_PERIOD);
    TimerOn();
    uchar i = 0;
    while (1) {

        mklaser(110, 10,down,green);
        mklaser(100,300,up,blue);
        mklaser(200, 10,down,red);
        mklaser(180,300,up,green);
        mklaser(80,300,up,red);
        mklaser(20,300,up,blue);
        _delay_ms(500);
    }

    return 0;
}

/* function definitions */

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
    sendcmd(0x2A);                                                      /* Column Command address       */
    senddata(start);
    senddata(end);
}

void setpage(uint start,uint end)
{
    sendcmd(0x2B);                                                      /* Column Command address       */
    senddata(start);
    senddata(end);
}

void fillscr(void)
{
    setcol(0, 239);
    setpage(0, 319);
    sendcmd(0x2c);                                                  /* start to write to display ra */
                                                                        /* m                            */

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

void lcdinit(void)
{
    SPI_INIT();
    PORTD |=  0x20; // CS HIGH;
    PORTD |= 0x40; // DC HIGH;
    uchar i=0, TFTDriver=0;

	PORTD &= ~0x10; // RST ON;
	_delay_ms(10);
	PORTD |=  0x10; // RST OFF;

  _delay_ms(10);

	sendcmd(0xCB);  
	write(0x39); 
	write(0x2C); 
	write(0x00); 
	write(0x34); 
	write(0x02); 

	sendcmd(0xCF);  
	write(0x00); 
	write(0XC1); 
	write(0X30); 

	sendcmd(0xE8);  
	write(0x85); 
	write(0x00); 
	write(0x78); 

	sendcmd(0xEA);  
	write(0x00); 
	write(0x00); 

	sendcmd(0xED);  
	write(0x64); 
	write(0x03); 
	write(0X12); 
	write(0X81); 

	sendcmd(0x36);    	// Memory Access Control 
	write(0x48);  	//C8	   //48 68绔栧睆//28 E8 妯睆

	sendcmd(0x3A);    
	write(0x55); 

	sendcmd(0xB1);    
	write(0x00);  
	write(0x18); 

	sendcmd(0x11);    	//Exit Sleep 
	_delay_ms(120); 

	sendcmd(0x29);    //Display on 
	sendcmd(0x2c);   
	fillscr();
}

int tick_lcd(int state){
    // update laser positions
    for (uchar i = 0; i < LSR_CNT; i++){
        if (lasers[i]->dead != 1){ //laser is not dead
            move_laser(lasers[i]);
        }
    }
    return state;
}

uchar setpx(uint x, uint y, uint color){
    if (x > 0 && x <= max_x && y > 0 && y <= max_y) {
        setcol(x,x);
        setpage(y,y);
        sendcmd(0x2C);
        senddata(color);
        return 0;
    }
    return 1;
}


void mklaser(ushort x, ushort y, dir laser_dir, uint color){
    laser_struct* laser = new laser_struct;
    laser->ldir = laser_dir;
    laser->y0 = y;
    laser->y1 = y + 10;
    laser->x0 = x;
    laser->color = color;
    laser->dead = 0;
    for (ushort i = laser->y0; i <= laser->y1; i++){
        setpx(x, i, color);
    }

    for (uchar i = 0; i <= LSR_CNT; i++){
        if (lasers[i] == nullptr) {
            lasers[i] = laser;
            if (LSR_CNT < LSR_MAX) LSR_CNT+= 1;
            break;
        }
        if (lasers[i]->dead == 1){
            delete lasers[i];
            lasers[i] = laser;
            if (LSR_CNT < LSR_MAX) LSR_CNT+= 1;
            break;
        }

    }
}

uchar move_laser(laser_struct* laser){
    switch (laser->ldir){
        case down:
            if (setpx(laser->x0, laser->y0, black) == 1){
                laser->dead = 1;
                return 0;
            }
            laser->y0 += 1;
            laser->y1 += 1; 
            setpx(laser->x0, laser->y1, laser->color);

            break;
        case up:

            if (setpx(laser->x0, laser->y1, black) == 1){
                laser->dead = 1;
                return 0;
            }
            laser->y1 -= 1;
            laser->y0 -= 1;
            setpx(laser->x0, laser->y0, laser->color);

            break;
    }
    return 0;
}
