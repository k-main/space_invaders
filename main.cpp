#include "spiAVR.h"
#include "timerISR.h"
#include "serialATmega.h"
#include "periph.h"
#include "helper.h"
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

#define hitb_x 6
#define hitb_y 8
#define shft_max_y 4
#define enemies_per_row 11
#define enemy_rows 4

#define dmg_rad 7

#define NUM_TASKS 5


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
enum enemy_type {lvl_1, lvl_2, lvl_3};
struct laser_struct{
    uchar barrier_num, hitbox_num;
    uchar x0, dead, pending_coll;
    ushort y0, y1;
    uint color;
    dir ldir;
    void erase(void);
    void mvlaser(void);
    uchar collision(void);
};

struct entity {
    dir fire_dir;
    ushort fire_pos;
    uchar x0, x1;
    ushort y0, y1;
    uint color, laser_color;
};

struct player_entity : entity { 
    ushort gunx0, gunx1, guny0, guny1;
};

struct enemy : entity {
    void (*render_s0)(ushort, ushort, uint);
    void (*render_s1)(ushort, ushort, uint);
};

struct enemy_row {
    enemy enemies[enemies_per_row];
    dir shift_dir;
    uchar shift_i;
    uchar shift_enemy_num;
    uchar shift_y_c;
    enemy_row(ushort x0, ushort y0, enemy_type type);
    void x_shift(void);
    void y_shift(void);
};


struct hitbox_ss {
    uchar x0, x1;
    ushort y0, y1;
    uchar hit;
};
struct static_structure { 
    ushort x0, y0, x1, y1;
    hitbox_ss hitbox[15];
};

uchar mv_l = 0, mv_r = 0, fire_req = 0, fire_ack;

void sendcmd(uchar index);
void write(uchar data);
void senddata(uint data);
void setcol(uint start, uint end);
void setpage(uint start, uint end);
void fillscr(void);
void fillscr(ushort x0, ushort x1, ushort y0, ushort y1, uint color);
void drawcirc(ushort x, ushort y, ushort r, uint color);
void initlcd(void);
void mklaser(ushort x, ushort y, dir laser_dir, uint color);
void renderplayer(ushort x, ushort y, uint color, uint laser_color);
void mvplayer(uchar step_amt, dir direction);
void drawline(ushort x, ushort y, ushort len, uint color);
void render_s0_l1(ushort x0, ushort y0);
void render_s1_l1(ushort x0, ushort y0);
uchar setpx(ushort x, ushort y, uint color);
int tick_lasers(int state);
int tick_mv(int state);
int tick_fire(int state);
int tick_player(int state);
int tick_enemies(int state);
static_structure mkstruct(ushort x0, ushort x1, ushort y0, ushort y1);



player_entity player;
const uchar LSR_MAX = 10;
uchar LSR_CNT = 0;
laser_struct* lasers[LSR_MAX];

static_structure barriers[4];
enemy_row* rows[4];

// uchar barrier_num, hitbox_num;
uchar row_num;

int main(void) {
  /*
  To initialize a pin as an output, you must set its DDR value as ‘1’ and then set its PORT value as a ‘0’. 
  To initialize a pin as an input, you do the opposite: you must set its DDR value as ‘0’ and its PORT value as a ‘1’.
  */
    ADC_init();
    DDRD = 0b11110000; PORTD = ~0b11110000;
    DDRB = 0b00101000; PORTB = ~0b00101111;
    serial_init(9600);
    /* initialization */
    initlcd();
    renderplayer(105, 300, white, cyan);
    barriers[0] = mkstruct(15, 45, 260, 284);
    barriers[1] = mkstruct(75, 105, 260, 284);
    barriers[2] = mkstruct(135, 165, 260, 284);
    barriers[3] = mkstruct(195, 225, 260, 284);


    // enemy_row row_1 = enemy_row(max_x - 2*hitb_x,76, lvl_1);
    // enemy_row row_2 = enemy_row(max_x - 2*hitb_x,100, lvl_1);
    // enemy_row row_3 = enemy_row(max_x - 2*hitb_x,124, lvl_1);
    // enemy_row row_4 = enemy_row(max_x - 2*hitb_x,148, lvl_1);
    // rows[0] = &row_1;
    // rows[1] = &row_2;
    // rows[2] = &row_3;
    // rows[3] = &row_4;
    serial_print("size of enemy row: ");
    serial_println(sizeof(enemy_row));
    for (uchar i = 0; i < enemy_rows; i++){
        
        rows[i] = new enemy_row(max_x - 2*hitb_x, 48 + 24*i, lvl_1);
    }

    // enemy_row row_2 = enemy_row(max_x - 2*hitb_x,124, lvl_1);

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

    tasks[3].period = 30;
    tasks[3].state = wait;
    tasks[3].elapsedTime = 1;
    tasks[3].TickFct = &tick_player;

    tasks[4].period = 50;
    tasks[4].state = update;
    tasks[4].elapsedTime = 1;
    tasks[4].TickFct = &tick_enemies;
    
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

void initlcd(void)
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
            lasers[i]->mvlaser();
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
    laser->pending_coll = 0;
    for (int i = 0; i < 4; i++){
        if (x >= barriers[i].x0 && x <= barriers[i].x1){
            laser->pending_coll = 1;
            laser->barrier_num = (map_value(0,barriers[3].x1 +15,0,4,x));
            laser->hitbox_num = (map_value(barriers[laser->barrier_num].x0,barriers[laser->barrier_num].x1,0,5,x));
            continue;
        }
    }

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

uchar laser_struct::collision(){
    ushort leading_px = (ldir == down) ? y1 : y0;
    uchar arr_offst = (ldir == down) ? 1 : 0;
    char row_coeff = (ldir == down) ? - 1 : 1;
    // barrier collision
    if (pending_coll == 1){
        if (leading_px >= barriers[0].y0 && leading_px <= barriers[0].y1){
            row_num = map_value(barriers[0].y0, barriers[0].y1 + 1,0,3,leading_px);
            if (barriers[barrier_num].hitbox[hitbox_num + 10*arr_offst + row_coeff*5*row_num].hit == 0){
                barriers[barrier_num].hitbox[hitbox_num + 10*arr_offst + row_coeff*5*row_num].hit = 1;
                drawcirc(x0,leading_px,dmg_rad,black);
                erase();
                dead = 1;
            } else if (row_num == 0) drawcirc(x0,y0,dmg_rad,black);
        }
    }
    return 0;
}

void laser_struct::mvlaser(){
    ushort leadingpx = (ldir == down) ? y1 : y0;
    ushort laggingpx = (leadingpx == y1) ? y0 : y1;
    switch(ldir){
        case down:
            dead = (setpx(x0,laggingpx,black) == 1) ? 1 : 0;
            if (dead) break;
            y0 += 1;
            y1 += 1;
            setpx(x0,leadingpx,color);
        break;
        case up:
            dead = (setpx(x0,laggingpx,black) == 1) ? 1 : 0;
            if (dead) break;
            y0 -= 1;
            y1 -= 1;
            setpx(x0,leadingpx,color);
        break;
    }
    if (leadingpx % 8 == 0) collision();
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
    player.color = color;
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

void drawline(ushort x0, ushort y0, ushort len, uint color){
    setcol(x0,x0);
    setpage(y0,y0 + len);
    sendcmd(0x2C);
    for(int i = 0; i < len; i++) senddata(color);
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

void laser_struct::erase(void){
    for (ushort i = y0; i <= y1; i++){
        setpx(x0,i,black);
    }
}

static_structure mkstruct(ushort x0, ushort x1, ushort y0, ushort y1){
    static_structure structure;
    structure.x0 = x0;
    structure.x1 = x1;
    structure.y0 = y0;
    structure.y1 = y1;
    fillscr(x0, x1, y0, y1, white);

    fillscr(x0 + 5, x1 - 5, y1 - 3, y1 + 3, black);
    fillscr(x0, x0 + 2, y0, y0 + 2, black);
    fillscr(x1 - 2, x1, y0, y0 + 2, black);

    hitbox_ss hitb;
    hitb.hit = 0;
    uchar hitb_i = 0;
    uchar hb_dx = (x1 - x0) / 5;
    uchar hb_dy = (y1 - y0) / 3;
    for (uchar y = 0; y < 3; y++){
        hitb.y0 = y0 + y*hb_dy;
        hitb.y1 = y0 + (y+1)*hb_dy;
        for (uchar x = 0; x < 5; x++){
            hitb.x0 = x0 + x*hb_dx;
            hitb.x1 = x0 + (x+1)*hb_dx;
            structure.hitbox[hitb_i] = hitb;
            hitb_i += 1;
        }
    }

    return structure;
}

void render_s0_l1(ushort x0, ushort y0, uint color){
    fillscr(x0, x0 + 2*hitb_x, y0, y0+2*hitb_y, color);
}

void render_s1_l1(ushort x0, ushort y0, uint color){
    fillscr(x0, x0 + 2*hitb_x, y0, y0+2*hitb_y, color);
}

enemy_row::enemy_row(ushort x0, ushort y0, enemy_type enemy){
    shift_i = 0;
    shift_y_c = 0;
    shift_enemy_num = 0;
    shift_dir = (x0 == 2*hitb_x) ? right : left;
    // shift_dir = (x0 == 2*hitb_x) ? right : left;
    // uchar x0 = (shift_dir == right) ? 2*hitb_x : max_x - 2*hitb_x;

    for (uchar i = 0; i < enemies_per_row; i++){
        enemies[i].x0 = x0;
        enemies[i].y0 = y0;
        switch (enemy){
            case lvl_1:
            enemies[i].render_s0 = &render_s0_l1;
            enemies[i].render_s1 = &render_s1_l1;
            break;
        }
        (shift_dir == right) ? enemies[i].render_s0(x0,y0,blue) : enemies[i].render_s0(x0 - 2*hitb_x, y0,blue);
        x0 = (shift_dir == right) ? x0 + 3*hitb_x : x0 - 3*hitb_x;
    }
    
}




void enemy_row::x_shift(void){
    if (shift_i < 30){
        switch(shift_dir){

            case right:
                // for (uchar i = 0; i < enemies_per_row; i++){
                // }
                    switch(shift_i % 2){
                        case 0:
                            enemies[shift_enemy_num].render_s0(enemies[shift_enemy_num].x0,enemies[shift_enemy_num].y0,black);
                            enemies[shift_enemy_num].x0 += 1;
                            enemies[shift_enemy_num].render_s1(enemies[shift_enemy_num].x0,enemies[shift_enemy_num].y0,cyan);
                        break;
                        case 1:
                            enemies[shift_enemy_num].render_s1(enemies[shift_enemy_num].x0,enemies[shift_enemy_num].y0,black);
                            enemies[shift_enemy_num].x0 += 1;
                            enemies[shift_enemy_num].render_s0(enemies[shift_enemy_num].x0,enemies[shift_enemy_num].y0,blue);
                        break;
                    }
            break;

            case left:
                // for (uchar i = 0; i < enemies_per_row; i++){
                // }
                    switch(shift_i % 2){
                        case 0:
                            enemies[shift_enemy_num].render_s0(enemies[shift_enemy_num].x0 - 2*hitb_x, enemies[shift_enemy_num].y0, black);
                            enemies[shift_enemy_num].x0 -= 1;
                            enemies[shift_enemy_num].render_s1(enemies[shift_enemy_num].x0 - 2*hitb_x, enemies[shift_enemy_num].y0, cyan);
                        break;
                        case 1:
                            enemies[shift_enemy_num].render_s1(enemies[shift_enemy_num].x0 - 2*hitb_x, enemies[shift_enemy_num].y0, black);
                            enemies[shift_enemy_num].x0 -= 1;
                            enemies[shift_enemy_num].render_s0(enemies[shift_enemy_num].x0 - 2*hitb_x, enemies[shift_enemy_num].y0, blue);
                        break;
                    }
            break;
        }  
    shift_i = (shift_enemy_num == enemies_per_row - 1) ? shift_i + 1 : shift_i;
    shift_enemy_num = (shift_enemy_num < enemies_per_row - 1) ? shift_enemy_num + 1 : 0;
    // shift_i += 1;
    } 
}

void enemy_row::y_shift(){
    switch(shift_dir){
        case right:
            for (uchar i = 0; i < enemies_per_row; i++){
                enemies[i].render_s0(enemies[i].x0,enemies[i].y0,black);
                if (shift_y_c < shft_max_y) enemies[i].y0 += 3*hitb_y;
                shift_dir = left;
                enemies[i].render_s1(enemies[i].x0, enemies[i].y0, white);
                enemies[i].x0 += 2*hitb_x;
                shift_i = 0;
            }
        break;
        case left:
            for (uchar i = 0; i < enemies_per_row; i++){
                enemies[i].render_s0(enemies[i].x0 - 2*hitb_x, enemies[i].y0,black);
                if (shift_y_c < shft_max_y) enemies[i].y0 += 3*hitb_y;
                shift_dir = right;
                enemies[i].render_s1(enemies[i].x0 - 2*hitb_x, enemies[i].y0,white);
                enemies[i].x0 -= 2*hitb_x;
                shift_i = 0;
            }
        break;
    }
    shift_y_c = (shift_y_c < shft_max_y) ? shift_y_c + 1 : shift_y_c;

}

int tick_enemies(int state){
    for (uchar i = 0; i < enemy_rows; i++){
        rows[i]->x_shift();
    }

    if (rows[enemy_rows - 1]->shift_i == 30){
        for (uchar i = enemy_rows; i > 0; i--){
            rows[i - 1]->y_shift();
        }
    }
    return state;
}