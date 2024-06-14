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
#define b_red 0xE691
#define yellow 0xffe0
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
#define enemy_start_y 5*hitb_y
#define enemy_rows 0
#define row_offst 21


#define dmg_rad 7

#define NUM_TASKS 6


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

enum program_state {
    menu,live,reset,halt
};

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

struct player : entity { 
    ushort gunx0, gunx1, guny0, guny1;
    ushort lives;
    // player(ushort x0, ushort y0);
    // void render(ushort x0, ushort y0, uint player_color, uint laser_color);
};

struct enemy : entity {
    uchar dead;
    void (*render_s0)(ushort, ushort, uint);
    void (*render_s1)(ushort, ushort, uint);
    void perish(void);
    void render0(uint c);
    void render1(uint c);
};

struct enemy_row {
    enemy enemies[enemies_per_row];
    dir shift_dir;
    uchar shift_i;
    uchar shift_i_y;
    uchar l_most, r_most, r_synch;
    uchar hp, shoot_f;
    enemy_row(ushort y0, enemy_type type, dir);
    void erase(uchar enemy_num);
    void set_rmost(void);
    void set_lmost(void);
    uchar x_shift(void);
    uchar y_shift(void);
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

struct game_state {
    uint score;
    uint enemy_c;
    int lives;
    program_state state;
    game_state();
    void reset(void);
    void i_score(void);
    void render_score(void);

    void i_lives(void);
    void d_lives(void);
    void render_lives(void);
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
void render_s0_l1(ushort x0, ushort y0, uint color);
void render_s1_l1(ushort x0, ushort y0, uint color);
void render_s0_l2(ushort x0, ushort y0, uint color);
void render_s1_l2(ushort x0, ushort y0, uint color);
void render_s0_l3(ushort x0, ushort y0, uint color);
void render_s1_l3(ushort x0, ushort y0, uint color);
uchar setpx(ushort x, ushort y, uint color);
uint randint(void);

int tick_lasers(int state);
int tick_mv(int state);
int tick_fire(int state);
int tick_player(int state);
int tick_enemies(int state);
int tick_gamestate(int state);

ushort tick_enemies_c = 0;
ushort tick_halt_c = 0;

static_structure mkstruct(ushort x0, ushort x1, ushort y0, ushort y1);

player player1;


const uchar LSR_MAX = 10;
uchar LSR_CNT = 0;
uint shoot_i = 5;
laser_struct* lasers[LSR_MAX];
static_structure barriers[4];
enemy_row* rows[enemy_rows];

// MASTER GAMESTATE
game_state game = game_state();

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
    game.reset();

    tasks[0].period = 100;
    tasks[0].state = live;
    tasks[0].elapsedTime = 1;
    tasks[0].TickFct = &tick_gamestate;

    tasks[1].period = 5;
    tasks[1].state = update;
    tasks[1].elapsedTime = 1;
    tasks[1].TickFct = &tick_lasers;

    tasks[2].period = 20;
    tasks[2].state = wait;
    tasks[2].elapsedTime = 1;
    tasks[2].TickFct = &tick_mv;

    tasks[3].period = 20;
    tasks[3].state = wait;
    tasks[3].elapsedTime = 1;
    tasks[3].TickFct = &tick_fire;

    tasks[4].period = 30;
    tasks[4].state = wait;
    tasks[4].elapsedTime = 1;
    tasks[4].TickFct = &tick_player;

    tasks[5].period = 50;
    tasks[5].state = update;
    tasks[5].elapsedTime = 1;
    tasks[5].TickFct = &tick_enemies;

    
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
                drawcirc(x0,leading_px,dmg_rad,black); // replace with dmg sprite
                erase();
            } else if (row_num == 0) drawcirc(x0,y0,dmg_rad,black);
        }
    }
    // player1 shooting at enemies

    short hit;
    if (ldir == up){
        for (short i = enemy_rows - 1; i >= 0; i--){
            if (!rows[i]->hp) continue;
            enemy_row row = *rows[i];
            enemy _enemy = row.enemies[row.r_most]; // doesnt matter, just need a valid one

            if (_enemy.y0 + 2*hitb_y - y0 < 2*hitb_y){
                hit = map_value(row.enemies[row.l_most].x0, row.enemies[row.r_most].x0+2*hitb_x,row.l_most,row.r_most + 1,x0);
                if (hit >= 11 || hit < 0){
                    continue;
                }
            } else {
                continue;
            }
            
            enemy enemy_hit = rows[i]->enemies[hit];
            if (!enemy_hit.dead){
                if (enemy_hit.x0 + 2*hitb_x - x0 <= 2*hitb_x) {
                    rows[i]->enemies[hit].perish();
                    erase();
                    if (hit == rows[i]->l_most){
                        rows[i]->set_lmost();
                    } else if (hit == rows[i]->r_most){
                        rows[i]->set_rmost();
                    }
                    rows[i]->hp = rows[i]->hp - 1;
                    return 0;
                }
            }
        }
    } else {
        
    }
    // todo: enemies shooting at player1
    if (player1.y1 - y1 < player_h) {
        if (x0 > player1.x0 && x0 < player1.x1) {
            //  serial_print(randint()); serial_print(" "); serial_println("player1 hit");
            erase();
            game.d_lives(); // decriment lives;
            serial_println(game.lives);
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
            if (leadingpx > max_y - 3*hitb_y - 4) erase();
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
    if (game.state == halt) return state;
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
    if (game.state == halt) return state;
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
    if (game.state == halt) return state;
    if (fire_req == 1){
        fire_ack = 1;
        mklaser(player1.fire_pos, player1.y0 - 14, up, player1.laser_color);
        // serial_print("Laser at : ");
        // serial_println(player1.fire_pos);
    } else {
        fire_ack = 0;
    }
    if (mv_l == 1) {
        if (player1.x0 - player_step > 0){
            mvplayer(player_step, left);
        }
    }
    if (mv_r == 1){
        if (player1.x1 + player_step < max_x){
            mvplayer(player_step, right); 
        }
    }

    
}

void renderplayer(ushort x0, ushort y0, uint color, uint laser_color){
    player1.x0 = x0; 
    player1.x1 = x0 + player_w;
    player1.y0 = y0; 
    player1.y1 = y0 + player_h; 
    player1.fire_pos = x0 + (player_w / 2);
    player1.laser_color = laser_color;
    player1.color = color;
    // for his lil gun
    player1.gunx0 = x0 + (player_w / 2) - 2;
    player1.gunx1 = player1.gunx0 + 4;
    player1.guny0 = y0 - 3;
    player1.guny1 = y0;
    fillscr(player1.gunx0, player1.gunx1, player1.guny0, player1.guny1, color);
    fillscr(x0, x0 + player_w, y0, y0 + player_h, color);
}

void mvplayer(uchar step_amt, dir direction){
    switch(direction){
        case left:
            /* erase rightmost columns */
            fillscr((player1.x1 - step_amt), player1.x1, player1.y0, player1.y1, black);
            fillscr((player1.gunx1 - step_amt), player1.gunx1, player1.guny0, player1.guny1, black);
            player1.x1 -= step_amt;
            player1.gunx1 -= step_amt;
            /* write leftmost columns */
            fillscr(player1.x0 - step_amt, player1.x0, player1.y0 + 1, player1.y1, white);
            fillscr(player1.gunx0 - step_amt, player1.gunx0, player1.guny0, player1.guny1, white);
            player1.x0 -= step_amt;
            player1.gunx0 -= step_amt;
            player1.fire_pos -= step_amt;
        break;
        case right:
            /*write rightmost columns*/
            fillscr(player1.x1, (player1.x1 + step_amt), player1.y0 + 1, player1.y1, white);
            fillscr(player1.gunx1, (player1.gunx1 + step_amt), player1.guny0, player1.guny1, white);
            player1.x1 += step_amt;
            player1.gunx1 += step_amt;
            /*erase leftmost column*/
            fillscr(player1.x0, (player1.x0 + step_amt), player1.y0, player1.y1, black);
            fillscr(player1.gunx0, (player1.gunx0 + step_amt), player1.guny0, player1.guny1, black);
            player1.x0 += step_amt;
            player1.gunx0 += step_amt;
            player1.fire_pos += step_amt;
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
    dead = 1;
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

void enemy::perish(void){
    // (x0 % 2 == 0) ? render0(black) : render1(black);
    fillscr(x0, x0+2*hitb_x,y0,y0+2*hitb_y + 2,black);
    dead = 1;
}

void enemy::render0(uint c){
    render_s0(x0,y0,c);
}

void enemy::render1(uint c){
    render_s1(x0,y0,c);
}

void render_s0_l2(ushort x0, ushort y0, uint color){
    // fillscr(x0, x0 + 2*hitb_x, y0, y0+2*hitb_y, color);
    ushort x0plus2hb = x0 + 2*hitb_x;
    ushort y0plus6 = y0 + 6;
    ushort hbdiv2 = hitb_x >> 1;
    fillscr(x0 + 1, x0plus2hb - 1, y0 + 3, y0plus6 + hitb_y, color); 

    // corners
    setpx(x0 + 1, y0 + 3, black); setpx(x0plus2hb - 1, y0 + 3, black);
    setpx(x0 + 1, y0plus6 + hitb_y, black); setpx(x0plus2hb - 1, y0plus6 + hitb_y, black);
    
    // eyes
    fillscr(x0 + 3, x0 + 5, y0 + 5, y0 + 7, black);
    fillscr(x0plus2hb - 5, x0plus2hb - 3, y0 + 5, y0 + 7, black);

    fillscr(x0 + 3, x0plus2hb - 3, y0 + 3 + hitb_y, y0 + 5 + hitb_y, black);
    fillscr(x0 + 6, x0plus2hb - 6, y0 + 5 + hitb_y, y0plus6 + hitb_y, black);
    
    setpx(x0 + hbdiv2, y0 + 2, color); setpx(x0 + hbdiv2 - 1, y0 + 1, color);
    setpx(x0plus2hb - 1 - hbdiv2, y0 + 2, color); setpx(x0plus2hb - 1 - hbdiv2 + 1, y0 + 1, color);
}

void render_s1_l2(ushort x0, ushort y0, uint color){
    ushort x0plus2hb = x0 + 2*hitb_x;
    ushort hbdiv2 = hitb_x >> 1;
    fillscr(x0 + 1, x0plus2hb - 1, y0 + 3, y0 + 3 + hitb_y, color); 
    
    // corners
    setpx(x0 + 1, y0 + 3, black); setpx(x0plus2hb - 1, y0 + 3, black);
    setpx(x0 + 1, y0 + 3 + hitb_y, black); setpx(x0plus2hb - 1, y0 + 3 + hitb_y, black);
    // antenna
    setpx(x0 + (hitb_x >> 2), y0 + 2, color); setpx(x0 + (hitb_x >> 2) - 1, y0 + 1, color);
    setpx(x0plus2hb - 1 - (hitb_x >> 2), y0 + 2, color); setpx(x0plus2hb - 1 - (hitb_x >> 2) + 1, y0 + 1, color);
    // eyes
    fillscr(x0 + 3, x0 + 5, y0 + 5, y0 + 7, black);
    fillscr(x0plus2hb - 5, x0plus2hb - 3, y0 + 5, y0 + 7, black);

    fillscr(x0 + hbdiv2 - 2, x0 + hbdiv2, y0 + 4 + hitb_y, y0 + 4 + hitb_y, color);
    fillscr(x0 + hbdiv2 - 3, x0 + hbdiv2 - 1, y0 + 5 + hitb_y, y0 + 5 + hitb_y, color);

    fillscr(x0plus2hb - hbdiv2, x0plus2hb - hbdiv2 + 2, y0 + 4 + hitb_y, y0 + 4 + hitb_y, color);
    fillscr(x0plus2hb - hbdiv2 + 1, x0plus2hb - hbdiv2 + 3, y0 + 5 + hitb_y, y0 + 5 + hitb_y, color);
}

void render_s0_l1(ushort x0, ushort y0, uint color){
    short start = x0, end = start + 12;
    fillscr(start,end,y0+hitb_x+2,y0+hitb_x+5,white);
    fillscr(start + 1, end - 1,y0 + hitb_x + 6, y0 + hitb_x + 7, white);
    fillscr(start,start,y0 + hitb_x + 6, y0 + hitb_x + 9, white); fillscr(start+1,start+1,y0 + hitb_x + 9, y0 + hitb_x + 12, white);
    fillscr(end,end,y0 + hitb_x + 6, y0 + hitb_x + 9, white); fillscr(end - 1, end - 1, y0 + hitb_x + 9, y0 + hitb_x + 12, white);
    fillscr(start + 4, start + 8, y0+hitb_x+6, y0+hitb_x+8, white);
    for (uchar i = 0; i < 6; i++){
        fillscr(start,end,y0 + hitb_y-i,y0 + hitb_y-i,white);
        start += 1;
        end -= 1;
    }
    fillscr(start + 2, start + 4, y0 + hitb_x + 3, y0 + hitb_x + 4, black);
    fillscr(end - 4, end -2, y0 + hitb_x + 3, y0 + hitb_x + 4, black);
}

void render_s1_l1(ushort x0, ushort y0, uint color){
    short start = x0, end = start + 12;
    fillscr(start,end,y0+hitb_x+2,y0+hitb_x+5,white);
    fillscr(start + 1, end - 1,y0 + hitb_x + 6, y0 + hitb_x + 7, white);

    fillscr(start + 3, start + 3, y0 + hitb_x + 6, y0 + hitb_x + 10, white); fillscr(start + 4, start + 4, y0 + hitb_x + 10, y0 + hitb_x + 12, white);
    fillscr(end - 3, end - 3, y0 + hitb_x + 6, y0 + hitb_x + 10, white); fillscr(end - 4, end - 4, y0 + hitb_x + 10, y0 + hitb_x + 12, white);

    
    fillscr(start + 4, start + 8, y0+hitb_x+6, y0+hitb_x+8, white);
    for (uchar i = 0; i < 6; i++){
        fillscr(start,end,y0 + hitb_y-i,y0 + hitb_y-i,white);
        start += 1;
        end -= 1;
    }
    fillscr(start + 2, start + 4, y0 + hitb_x + 3, y0 + hitb_x + 4, black);
    fillscr(end - 4, end -2, y0 + hitb_x + 3, y0 + hitb_x + 4, black);
}

void render_s0_l3(ushort x0, ushort y0, uint color){
    fillscr(x0 + 2, x0 + 2*hitb_x - 2, y0 +  hitb_y - 2, y0 + hitb_y, white);
    fillscr(x0, x0 + 2*hitb_x, y0 + hitb_y, y0 + hitb_y + 2, white);
    // setpx(x0 + 4, y0 + hitb_y + 1, black);
    fillscr(x0 + 4, x0 + 8, y0 + hitb_y + 1, y0 + hitb_y + 2, black);
    fillscr(x0 + 2, x0 + 2*hitb_x - 2, y0 + hitb_y + 3, y0 + hitb_y + 4, white);
    fillscr(x0 + 2, x0 + 4, y0 + hitb_y + 5, y0 + hitb_y + 6, white);
    fillscr(x0 + 3, x0 + 4, y0 + hitb_y + 6, y0 + hitb_y + 8, white);
    fillscr(x0 + 2*hitb_x - 4, x0 + 2*hitb_x - 2, y0 + hitb_y + 5, y0 + hitb_y + 6, white);
    fillscr(x0 + 2*hitb_x - 4, x0 + 2*hitb_x - 3, y0 + hitb_y + 6, y0 + hitb_y + 8, white);
    // fillscr(x0 + 2, )
}

void render_s1_l3(ushort x0, ushort y0, uint color){
    fillscr(x0 + 2, x0 + 2*hitb_x - 2, y0 +  hitb_y - 2, y0 + hitb_y, white);
    fillscr(x0, x0 + 2*hitb_x, y0 + hitb_y, y0 + hitb_y + 2, white);
    // setpx(x0 + 4, y0 + hitb_y + 1, black);
    fillscr(x0 + 4, x0 + 8, y0 + hitb_y + 1, y0 + hitb_y + 2, black);
    fillscr(x0 + 2, x0 + 2*hitb_x - 2, y0 + hitb_y + 3, y0 + hitb_y + 4, white);
    fillscr(x0 + 2, x0 + 4, y0 + hitb_y + 5, y0 + hitb_y + 6, white);
    fillscr(x0 + 1, x0 + 2, y0 + hitb_y + 6, y0 + hitb_y + 8, white);
    fillscr(x0 + 2*hitb_x - 4, x0 + 2*hitb_x - 2, y0 + hitb_y + 5, y0 + hitb_y + 6, white);
    fillscr(x0 + 2*hitb_x - 2, x0 + 2*hitb_x - 1, y0 + hitb_y + 6, y0 + hitb_y + 8, white);
    // fillscr(x0 + 2, )
}

enemy_row::enemy_row(ushort y0, enemy_type enemy, dir enemy_dir){
    void (*render_0)(ushort, ushort, uint);
    void (*render_1)(ushort, ushort, uint);
    uint lsr_color;
    switch(enemy){
        case lvl_1:
            render_0 = render_s0_l1;
            render_1 = render_s1_l1;
            lsr_color = red;
        break;
        case lvl_2:
            render_0 = render_s0_l2;
            render_1 = render_s1_l2;
            lsr_color = red;
        break;
        case lvl_3:
            render_0 = render_s0_l3;
            render_1 = render_s1_l3;
            lsr_color = red;
        break;
    }

    for (unsigned char i = 0; i < enemies_per_row; i++){
        unsigned char enemy_x0 = row_offst + 3*i*hitb_x;
        render_s0_l1(enemy_x0, y0, white);
        enemies[i].render_s0 = render_0;
        enemies[i].render_s1 = render_1;
        enemies[i].x0 = enemy_x0;
        enemies[i].x1 = enemy_x0 + 2*hitb_x;
        enemies[i].y0 = y0;
        enemies[i].y1 = y0 + hitb_y;
        enemies[i].dead = 0;
        enemies[i].laser_color = lsr_color;
    }
    r_synch = 1;
    l_most = 0;
    r_most = enemies_per_row - 1;
    hp = enemies_per_row;
    shift_dir = enemy_dir;
    shift_i = 0;
    shift_i_y = 0;
    shoot_f = 0;
}

void enemy_row::set_lmost(void){
    for (short i = l_most; i < enemies_per_row; i++){
        if (enemies[i].dead == 0){
            l_most = i;
            // serial_print("l_most is now : "); serial_println(l_most);
            break;
        }
    }
}

void enemy_row::set_rmost(void){
    for (short i = r_most; i >= 0; i--){
        if (enemies[i].dead == 0){
            r_most = i;
            // serial_print("r_most is now : "); serial_println(r_most);
            break;
        }
    }
}

uchar enemy_row::x_shift(void){

    enemy _enemy = enemies[shift_i];
    if (enemies[shift_i].dead == 1){
        shift_i = (shift_i >= enemies_per_row - 1) ? 0 : shift_i + 1;
        return 0;
    }
    
    fillscr(_enemy.x0,_enemy.x0 + 2*hitb_x, _enemy.y0, _enemy.y0 + 2*hitb_y + 2, black); // erase   
    _enemy.x0 = (shift_dir == right) ? _enemy.x0 + 1 : _enemy.x0 - 1;                // shift
    enemies[shift_i].x0 = _enemy.x0;                                                 // reassign
    (_enemy.x0 % 2 == 0) ? _enemy.render0(white) : _enemy.render1(white);              // re render
    r_synch = (shift_i == l_most) ? 0 : r_synch;
    r_synch = (shift_i == r_most) ? 1 : r_synch;

    
    shift_i = (shift_i >= r_most) ? l_most : shift_i + 1;
    if (shoot_f == 1) {
        mklaser(_enemy.x0 + hitb_x, _enemy.y0 + 2*hitb_y, down, _enemy.laser_color);
        shoot_f = 0;
    }
    return 0;
}

uchar enemy_row::y_shift(void){
    if (hp == 0) return 1;
    shift_i_y += 1;
    for (uchar i = l_most; i <= r_most; i++){
        if (enemies[i].dead == 1) continue;
        enemy _enemy = enemies[i];
        fillscr(enemies[i].x0, enemies[i].x0+2*hitb_x, enemies[i].y0, enemies[i].y0+2*hitb_y + 2, black);
        enemies[i].y0 += 3*hitb_y;
        if (_enemy.x0 % 2 == 0){
            // conditional rendering.
	        enemies[i].render0(white);
        } else {
            enemies[i].render1(white);
        }
    }
    return 0;
}

uint randint(void){
    uchar randint = (ADC_read(4) - 950) * (ADC_read(3) - 950);
    randint = map_value(0,256,0,4,randint);
    return randint;
}

int tick_enemies(int state){
    if (game.state == halt) return state;
    tick_enemies_c += 1;
    if (tick_enemies_c > 2){
        tick_enemies_c = 0;
        uint rint = randint();
        rint = (rint > enemy_rows - 1) ? enemy_rows  - 1 : rint;
        rows[rint]->shoot_f = 1;
        // serial_print("Shoot flag set at: "); serial_println(rint); s
    }

    
    for (short i = enemy_rows - 1; i >= 0; i--){
        /* shifting */
        enemy_row row = *rows[i];
        if (row.enemies[row.l_most].x0 <= hitb_x || row.enemies[row.r_most].x0 + 2*hitb_x >= max_x - hitb_x){ // bounds check
            if (row.r_synch == 1) {
                if (rows[i]->shift_i_y < shft_max_y) {
                    if (i == enemy_rows - 1) {
                        rows[i]->shift_dir = (rows[i]->shift_dir == right) ? left : right;
                        rows[i]->y_shift();
                    } else if (rows[i + 1]->shift_i_y > rows[i]->shift_i_y){
                         // second check to avoid invalid array access.
                        rows[i]->shift_dir = (rows[i]->shift_dir == right) ? left : right;
                        rows[i]->y_shift();
                    }
                } else {
                    rows[i]->shift_dir = (rows[i]->shift_dir == right) ? left : right;
                }
                // continue;
            }
        }
        rows[i]->x_shift();
        /* collision */

    }
    return state;
}

game_state::game_state(){
    score = 0;
    lives = 3;
    enemy_c = enemy_rows * enemies_per_row;
}

void game_state::reset(){
    score = 0;
    lives = 3;
    enemy_c = enemy_rows * enemies_per_row;
    state = live;
    // const unsigned short enemy_start_y = 5*hitb_y;
    dir enemy_dir = right;

    renderplayer(105, 280, white, blue);
    render_lives();

    barriers[0] = mkstruct(15, 45, 236, 260);
    barriers[1] = mkstruct(75, 105, 236, 260);
    barriers[2] = mkstruct(135, 165, 236, 260);
    barriers[3] = mkstruct(195, 225, 236, 260);

    enemy_type type;
    for (uchar i = 0; i < enemy_rows; i++){
        switch (i){
            case 0:
                type = lvl_1;
            break;
            case 1:
                type = lvl_2;
            break;
            default:
                type = lvl_3;
            break;
        }
        rows[i] = new enemy_row(enemy_start_y + 3*i*hitb_y,type,enemy_dir);
        enemy_dir = (enemy_dir == right) ? left : right;
    }
}


void game_state::d_lives(void){
    lives --;
    if (lives <= 0){
        state = halt;
        fillscr(player1.x0,player1.x0+player_w,player1.y0 - 4,player1.y0+player_h,black);
    }
    render_lives();
}

void game_state::render_lives(void){
    // clear previously displayed lives

    fillscr(2*hitb_x,4*hitb_x + 3*3*hitb_x, max_y - 2*hitb_y - 4, max_y - hitb_y, black);

    // display current lives
    for (int i = 0; i < lives; i++){
        fillscr(2*hitb_x + i*3*hitb_x, 4*hitb_x + i*3*hitb_x, max_y - 2*hitb_y, max_y - hitb_y, white);
        fillscr(2*hitb_x + 4 + i*3*hitb_x, 2*hitb_x + 8 + i*3*hitb_x, max_y - 2*hitb_y - 2, max_y - 2*hitb_y, white);
        fillscr(0,max_x, max_y - 3*hitb_y, max_y - 3*hitb_y, white);
    }
}

int tick_gamestate(int state){
    switch(state){
        case menu:

        break;
        case live:
        // check player lives
            
        break;
        case reset:
            game.reset();
            state = live;
        break;
        case halt:
            if (tick_halt_c > 10 && game.lives > 0) {
                tick_halt_c = 0;
                state = reset;
                break;
            } 
            tick_halt_c += 1;
        break;
    }
    return state;
}