
#include "spiAVR.h"
#include "timerISR.h"
#include "serialATmega.h"
#include "periph.h"
/* work smarter */
#define uchar unsigned char
#define ushort unsigned short
#define uint unsigned int
#define player_w 20
#define player_h 10
#define player_step 3
/* colors */
#define red   0xF800
#define green 0x07E0
#define blue  0x001f
#define cyan 0x07ff
#define black 0x0000
#define white 0xffff
/* dimensions */
#define min_x 0
#define min_y 0
#define max_x 239
#define max_y 319


#define NUM_TASKS 4


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

/* i do not like capital letters */

/* state enums */
enum lsr_state{
    update
};

enum btn_state{
    wait, press
};

enum dir {up,down,left,right};
struct laser_struct{
    ushort x0, y0, y1, dead;
    uint color;
    dir ldir;
};

struct mask{
    ushort x0, y0, x1, y1;
    uchar width;
    uint color;
};

struct entity{
    /* entity is centered at x0, y0 */
    dir fire_dir;
    ushort fire_pos;
    ushort gunx0, gunx1, guny0, guny1;
    ushort x0, x1, y0, y1;
    uint color, laser_color;
    ushort i = 0;
    mask render_mask[];
};

uchar mv_l = 0, mv_r = 0, fire_req = 0, fire_ack;

void sendcmd(uchar index);
void write(uchar data);
void senddata(uint data);
void setcol(uint start, uint end);
void setpage(uint start, uint end);
void fillscr(void);
void fillscr(ushort x0, ushort x1, ushort y0, ushort y1, uint color);
void lcdinit(void);
void mklaser(ushort x, ushort y, dir laser_dir, uint color);
void renderplayer(ushort x, ushort y, uint color, uint laser_color);
void mvplayer(uchar step_amt, dir direction);
// void mkentity(ushort x0, ushort x1, ushort y0, ushort y1, uint color);
uchar setpx(ushort x, ushort y, uint color);
uchar move_laser(laser_struct* laser);
int tick_lasers(int state);
int tick_mv(int state);
int tick_fire(int state);
int tick_player(int state);

entity player;
const uchar LSR_MAX = 20;
uchar LSR_CNT = 0;
laser_struct* lasers[LSR_MAX];

int main(void) {
  /*
  To initialize a pin as an output, you must set its DDR value as ‘1’ and then set its PORT value as a ‘0’. 
  To initialize a pin as an input, you do the opposite: you must set its DDR value as ‘0’ and its PORT value as a ‘1’.
  */
    serial_init(9600);
    ADC_init();
    DDRD = 0b11110000; PORTD = ~0b11110000;
    DDRB = 0b00101000; PORTB = ~0b00101111;
    lcdinit();
    // fillscr(100, 120, 300, 310, white);
    renderplayer(105, 300, white, cyan);

    tasks[0].period = 5;
    tasks[0].state = update;
    tasks[0].elapsedTime = 0;
    tasks[0].TickFct = &tick_lasers;

    tasks[1].period = 20;
    tasks[1].state = wait;
    tasks[1].elapsedTime = 1;
    tasks[1].TickFct = &tick_mv;

    tasks[2].period = 20;
    tasks[2].state = wait;
    tasks[2].elapsedTime = 1;
    tasks[2].TickFct = &tick_fire;

    tasks[3].period = 20;
    tasks[3].state = wait;
    tasks[3].elapsedTime = 1;
    tasks[3].TickFct = &tick_player;
    
    TimerSet(GCD_PERIOD);
    TimerOn();


    while (1) {
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

	sendcmd(0x36);    
	write(0x48);  

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

int tick_lasers(int state){
    // update laser positions
    for (uchar i = 0; i < LSR_CNT; i++){
        if (lasers[i]->dead != 1){ //laser is not dead
            move_laser(lasers[i]);
        }
    }
    return state;
}

uchar setpx(ushort x, ushort y, uint color){
    if (x >= 0 && x <= max_x && y >= 0 && y <= max_y) {
        setcol(x,x);
        setpage(y,y);
        sendcmd(0x2C);
        senddata(color);
        return 0;
    }
    return 1;
}

void mklaser(ushort x, ushort y, dir laser_dir, uint color){
    y = (y + 10 > max_y) ? max_y - 10 : y;
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
                // mklaser(laser->x0, max_y - 10, up, laser->color);
                return 1;
            }
            laser->y0 += 1;
            laser->y1 += 1; 
            setpx(laser->x0, laser->y1, laser->color);

            break;
        case up:

            if (setpx(laser->x0, laser->y1, black) == 1){
                laser->dead = 1;
                // mklaser(laser->x0, 0, down, laser->color);
                return 1;
            }
            laser->y1 -= 1;
            laser->y0 -= 1;
            setpx(laser->x0, laser->y0, laser->color);

            break;
    }
    return 0;
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

int tick_mv(int state){
    ushort leftbtn = ADC_read(0), rightbtn = ADC_read(1);

    switch(state){
        case wait:
            if ((leftbtn < 128 && rightbtn < 128) || (leftbtn > 128 && rightbtn > 128)){
                mv_l = 0; mv_r = 0;
                return wait;
            }
            return press;
        break;
        case press:
            if ((leftbtn < 128 && rightbtn < 128) || (leftbtn > 128 && rightbtn > 128)){
                mv_l = 0; mv_r = 0;
                return wait;
            }
            if (leftbtn < 128) {
                mv_l = 1;
            } else if (rightbtn < 128){
                mv_r = 1;
            }
        break;
    }
    return state;
}

int tick_fire(int state){
    ushort firebtn = ADC_read(2);
    fire_req = (fire_ack == 1) ? 0 : fire_req;
    switch(state){
        case wait:
            if (firebtn < 128){
                fire_req = 1;
                return press;
            }
            return wait;
        break;
        case press:
            if (firebtn > 128){
                return wait;
            }
            return press;
        break;
    }
}

int tick_player(int state){
    if (fire_req == 1){
        fire_ack = 1;
        mklaser(player.fire_pos, player.y0 - 14, up, player.laser_color);
    } else {
        fire_ack = 0;
    }
    if (mv_l == 1) {
        if (player.x0 - player_step > 0){
            mvplayer(player_step, left);
        }
    }
    if (mv_r == 1){
        if (player.x1 + player_step < max_x){
            mvplayer(player_step, right); 
        }
    }

    
}

void renderplayer(ushort x0, ushort y0, uint color, uint laser_color){
    player.x0 = x0; 
    player.x1 = x0 + player_w;
    player.y0 = y0; 
    player.y1 = y0 + player_h; 
    player.fire_pos = x0 + (player_w / 2);
    player.laser_color = laser_color;
    // for his lil gun
    player.gunx0 = x0 + (player_w / 2) - 2;
    player.gunx1 = player.gunx0 + 4;
    player.guny0 = y0 - 3;
    player.guny1 = y0;
    fillscr(player.gunx0, player.gunx1, player.guny0, player.guny1, color);
    fillscr(x0, x0 + player_w, y0, y0 + player_h, color);
}

void mvplayer(uchar step_amt, dir direction){
    switch(direction){
        case left:
            /* erase rightmost columns */
            fillscr((player.x1 - step_amt), player.x1, player.y0, player.y1, black);
            fillscr((player.gunx1 - step_amt), player.gunx1, player.guny0, player.guny1, black);
            player.x1 -= step_amt;
            player.gunx1 -= step_amt;
            /* write leftmost columns */
            fillscr(player.x0 - step_amt, player.x0, player.y0 + 1, player.y1, white);
            fillscr(player.gunx0 - step_amt, player.gunx0, player.guny0, player.guny1, white);
            player.x0 -= step_amt;
            player.gunx0 -= step_amt;
            player.fire_pos -= step_amt;
        break;
        case right:
            /*write rightmost columns*/
            fillscr(player.x1, (player.x1 + step_amt), player.y0 + 1, player.y1, white);
            fillscr(player.gunx1, (player.gunx1 + step_amt), player.guny0, player.guny1, white);
            player.x1 += step_amt;
            player.gunx1 += step_amt;
            /*erase leftmost column*/
            fillscr(player.x0, (player.x0 + step_amt), player.y0, player.y1, black);
            fillscr(player.gunx0, (player.gunx0 + step_amt), player.guny0, player.guny1, black);
            player.x0 += step_amt;
            player.gunx0 += step_amt;
            player.fire_pos += step_amt;
        break;

        default:
        /*you're not supposed to be in here*/
        break;
    }
}
