
#include <s3c44b0x.h>
#include <s3cev40.h>
#include <common_types.h>
#include <system.h>
#include <timers.h>
#include <uart.h>
#include <lcd.h>
#include <keypad.h>
#include <uda1341ts.h>
#include <iis.h>
#include <pbs.h>

#define TICKS_PER_SEC (100)

/* Declaración de graficos */

#define LANDSCAPE  ((uint8 *)0x0c250000)
#define FIREMEN    ((uint8 *)0x0c260000)
#define CRASH      ((uint8 *)0x0c260800)
#define DUMMY_0    ((uint8 *)0x0c270000)
#define DUMMY_90   ((uint8 *)0x0c270400)
#define DUMMY_180  ((uint8 *)0x0c270800)
#define DUMMY_270  ((uint8 *)0x0c270C00)
#define LIFE       ((uint8 *)0x0c271000)
#define MUSIC 	   ((int16 *)0x0c47a000)

#define MUSIC_SIZE 	   (512000)

typedef struct plots {
    uint16 x;               // Posición x en donde se pinta el gráfico
    uint16 y;               // Posición y en donde se pinta el gráfico
    uint8 *plot;            // Puntero al BMP que contiene el gráfico
} plots_t;

typedef struct sprite {
    uint16 width;           // Anchura del gráfico en pixeles
    uint16 height;          // Altura del gráfico en pixeles
    uint16 num_plots;       // Número de posiciones diferentes en donde pintar el gráfico
    plots_t plots[];        // Array de posiciones en donde pintar el gráfico
} sprite_t;

const sprite_t firemen = 
{
    64, 32, 3,                      // Los bomberos de tamaño 64x32 se pintan en 3 posiciones distintas
    {
        {  32, 176, FIREMEN },
        { 128, 176, FIREMEN },
        { 224, 176, FIREMEN }
    }
};

const sprite_t dummy = 
{
    32, 32, 19,                    // Los dummies de tamaño 32x32 se pintan en 19 posiciones distintas con 4 formas diferentes que se alternan
    {
        {   0,  64, DUMMY_0   },
        {  16,  96, DUMMY_90  },
        {  32, 128, DUMMY_180 },// pos 2
        {  48, 160, DUMMY_270 },
        {  64, 128, DUMMY_0   },
        {  80,  96, DUMMY_90  },
        {  96,  64, DUMMY_180 },
        { 112,  96, DUMMY_270 },
        { 128, 128, DUMMY_0   },// pos 8
        { 144, 160, DUMMY_90  },
        { 160, 128, DUMMY_180 },
        { 176,  96, DUMMY_270 },
        { 192,  64, DUMMY_0   },
        { 208,  96, DUMMY_90  },
        { 224, 128, DUMMY_180 },// pos 14
        { 240, 160, DUMMY_270 },
        { 256, 128, DUMMY_0   },
        { 272, 96,  DUMMY_90  },
        { 288, 64,  DUMMY_180 }
    }
};

const sprite_t crash = 
{
    64, 32, 3,                     // Los dummies estrellados de tamaño 64x32 se pintan en 3 posiciones distintas
    {
        {   32, 208, CRASH },
        {  128, 208, CRASH },
        {  224, 208, CRASH }
    }
};

const sprite_t life =
{
    16, 16, 3,                    // Los corazones estrellados de tamaño 16x16 se pintan en 3 posiciones distintas
    {
        {   8, 8, LIFE },
        {  24, 8, LIFE },
        {  40, 8, LIFE }
    }
};

/* Declaración de fifo de punteros a funciones */

#define BUFFER_LEN   (512)

typedef void (*pf_t)(void);

typedef struct fifo {
    uint16 head;
    uint16 tail;
    uint16 size;
    pf_t buffer[BUFFER_LEN];
} fifo_t;

void fifo_init( void );
void fifo_enqueue( pf_t pf );
pf_t fifo_dequeue( void );
boolean fifo_is_empty( void );
boolean fifo_is_full( void );

/* Declaración de recursos */

volatile fifo_t fifo;       // Cola de tareas
boolean gameOver;           // Flag de señalización del fin del juego

/* Declaración de variables */

uint8 dummyPos;     // Posición del dummy 
uint16 count;       // Número de dummies salvados

/* Declaración de funciones */

void dummy_init( void );                                    // Inicializa la posición del dummy y lo dibuja
void count_init( void );                                    // Inicializa el contador de dummies salvados y lo dibuja
void sprite_plot( sprite_t const *sprite, uint16 pos );     // Dibuja el gráfico en la posición indicada
void sprite_clear( sprite_t const *sprite, uint16 pos );    // Borra el gráfico pintado en la posición indicada

/* Declaración de tareas */

void dummy_move( void );    // Mueve el dummy
void count_inc( void );     // Incrementa el contador de dummies salvados

/* Declaración de RTI */

void isr_tick( void ) __attribute__ ((interrupt ("IRQ")));


/* variables añadidas */

uint32 scancode;
uint8 firemanPos;
uint8 hearts = 0;
boolean win_game;
boolean lose_game;
boolean finalizar;
uint8 caida;
boolean music;
volatile boolean flagPbs;


/* funciones añadidas */

void start_game( void );
void teclaMoveFireman ( void );
void lose_life( void );
void init_fireman( void );
void exit_game( void );
void win( void );
void lose( void );
void start_play( void );
void initMusic( void );

/*******************************************************************/

void main( void )
{
    pf_t pf;
    
    sys_init();
    timers_init();
    uart0_init();
    lcd_init();
    keypad_init();

    uda1341ts_init();
    
    iis_init(IIS_DMA);

    lcd_on();
    lcd_clear();

    lose_game = FALSE;
    lcd_draw_box(10, 10, 300, 200, BLACK, 4);//menu
	lcd_draw_box(10, 60, 300, 60, BLACK, 3);//barra

	lcd_puts_x2(20, 20, BLACK, "MENU. OPCIONES:");
	lcd_puts_x2(50, 80, BLACK, "0. JUGAR");
	lcd_puts_x2(50, 110, BLACK, "4. SALIR");
	uart0_puts( "Teclas\n" );
	uart0_puts( "Tecla 0 para jugar\n" );
	uart0_puts( "Tecla 4 para salir\n" );
	uart0_puts( "Tecla 2 para mover izquierda\n" );
	uart0_puts( "Tecla 3 para mover derecha\n" );
	uart0_puts("boton izquierdo pausa\n");
	uart0_puts("boton derecho play\n");

	music = FALSE;

	scancode = keypad_getchar();
	uart0_puts( "has pulsado \n" );
	uart0_puts( scancode );

	lcd_clear();

	if (scancode == KEYPAD_FAILURE){
		exit_game();
	}
	if(scancode == KEYPAD_KEY4){
		exit_game();

	}else if (scancode == KEYPAD_KEY0){

		initMusic();
		start_game();

		gameOver = FALSE;
		finalizar = FALSE;
		win_game = FALSE;
		lose_game = FALSE;
		hearts = 3;

		fifo_init();                                  // Inicializa cola de funciones
		timer0_open_tick( isr_tick, TICKS_PER_SEC );  // Instala isr_tick como RTI del timer0


		while( !gameOver ){
		//      sleep();                        // Entra en estado IDLE, sale por interrupción
			while( !fifo_is_empty() )
			{
				pf = fifo_dequeue();
				(*pf)();                    // Las tareas encoladas se ejecutan en esta hebra (background) en orden de encolado
			}


		}

		lcd_clear();
		lcd_draw_box(10, 10, 300, 200, BLACK, 4);

		if(gameOver){

			if(lose_game){
				lose();
			}else{
				win();
			}
		}
		timer0_close();
		while(1);
	}
}

/*******************************************************************/

void dummy_init( void )
{
    dummyPos = 0;                           // Inicializa la posición del dummy...
    sprite_plot( &dummy, 0 );               // ... y lo dibuja
}

void dummy_move( void )
{
    sprite_clear( &dummy, dummyPos );       // Borra el dummy de su posición actual
    if( dummyPos == dummy.num_plots-1 ){     // Si el dummy ha alcanzado la última posición...

        dummyPos = 0;                       // ... lo coloca en la posición de salida
        fifo_enqueue( count_inc );          // ... incremeta el contador de dummies rescatados 
    }
    else if( dummyPos == 2 && firemanPos != 0){ // primera pos donde se caeria

    	dummyPos = 0;
    	caida = 0;
    	//sprite_plot(&crash, 0);
    	fifo_enqueue( start_play );
    	fifo_enqueue(lose_life);
    }
    else if(dummyPos == 8 && firemanPos != 1){ // segunda pos donde se caeria

    	dummyPos = 0;
    	caida = 1;
    	//sprite_plot(&crash, 1);
    	fifo_enqueue( start_play );
    	fifo_enqueue(lose_life);
    }
    else if(dummyPos == 14 && firemanPos != 2){ // tercera pos donde se caeria

    	dummyPos = 0;
    	caida = 2;
    	//sprite_plot(&crash, 2);
    	fifo_enqueue( start_play );
    	fifo_enqueue(lose_life);
    }
    else{
        dummyPos++;                         // En caso contrario, avanza su posición´
    }

    sprite_plot( &dummy, dummyPos );        // Dibuja el dummy en la nueva posición   
}

/*******************************************************************/

void count_init( void )
{
    count = 0;                              // Inicializa el contador de dummies salvados...
    lcd_putint_x2( 287, 0, BLACK, count );  // ... y lo dibuja
}

void count_inc( void )
{
    count++;                                // Incrementa el contador de dummies salvados
    lcd_putint_x2( 287, 0, BLACK, count );
    if( count == 9 )                        // Si se han salvado 9 dummies...
        gameOver = TRUE;                    // ... señaliza fin del juego
}

/*******************************************************************/

void isr_tick( void )
{   
    static uint16 cont40ticks = 40;
    static uint16 cont5ticks = 5;

	if( !(--cont5ticks) ){
		cont5ticks = 5;
		fifo_enqueue( teclaMoveFireman );
    }

    if( !(--cont40ticks) )
    {
        cont40ticks = 40;
        fifo_enqueue( dummy_move );
    }


    I_ISPC = BIT_TIMER0;
};

/*******************************************************************/

extern uint8 lcd_buffer[];

void lcd_putBmp( uint8 *bmp, uint16 x, uint16 y, uint16 xsize, uint16 ysize );
void lcd_clearWindow( uint16 x, uint16 y, uint16 xsize, uint16 ysize );

void sprite_plot( sprite_t const *sprite, uint16 num )
{
    lcd_putBmp( sprite->plots[num].plot, sprite->plots[num].x, sprite->plots[num].y, sprite->width, sprite->height );
}

void sprite_clear( sprite_t const *sprite, uint16 num )
{
    lcd_clearWindow( sprite->plots[num].x, sprite->plots[num].y, sprite->width, sprite->height );
}

/*
** Muestra un BMP de tamaño (xsize, ysize) píxeles en la posición (x,y)
** Esta función es una generalización de lcd_putWallpaper
*/
void lcd_putBmp( uint8 *bmp, uint16 x, uint16 y, uint16 xsize, uint16 ysize )
{
	uint32 headerSize;

	uint16 xSrc, ySrc, yDst;
	uint16 offsetSrc, offsetDst;

	headerSize = bmp[10] + (bmp[11] << 8) + (bmp[12] << 16) + (bmp[13] << 24);

	bmp = bmp + headerSize; 

	for( ySrc=0, yDst=ysize-1; ySrc<ysize; ySrc++, yDst-- )
	{
		offsetDst = (yDst+y)*LCD_WIDTH/2+x/2;
		offsetSrc = ySrc*xsize/2;
		for( xSrc=0; xSrc<xsize/2; xSrc++ )
			lcd_buffer[offsetDst+xSrc] = ~bmp[offsetSrc+xSrc];
	}
}

/*
** Borra una porción de la pantalla de tamaño (xsize, ysize) píxeles desde la posición (x,y)
** Esta función es una generalización de lcd_clear
*/
void lcd_clearWindow( uint16 x, uint16 y, uint16 xsize, uint16 ysize )
{
	uint16 xi, yi;

	for( yi=y; yi<y+ysize; yi++ )
		for( xi=x; xi<x+xsize; xi++ )
			lcd_putpixel( xi, yi, WHITE );
}

/*******************************************************************/

void fifo_init( void )
{
    fifo.head = 0;
    fifo.tail = 0;
    fifo.size = 0;
}

void fifo_enqueue( pf_t pf )
{
    fifo.buffer[fifo.tail++] = pf;
    if( fifo.tail == BUFFER_LEN )
        fifo.tail = 0;
    INT_DISABLE;
    fifo.size++;
    INT_ENABLE;
}

pf_t fifo_dequeue( void )
{
    pf_t pf;
    
    pf = fifo.buffer[fifo.head++];
    if( fifo.head == BUFFER_LEN )
        fifo.head = 0;
    INT_DISABLE;
    fifo.size--;
    INT_ENABLE;
    return pf;
}

boolean fifo_is_empty( void )
{
    return (fifo.size == 0);
}

boolean fifo_is_full( void )
{
    return (fifo.size == BUFFER_LEN-1);
}

/*******************************************************************/


void init_fireman( void ){
	firemanPos = 0;
	sprite_plot( &firemen, firemanPos);
}

// gestion movimiento bomberos
void teclaMoveFireman( void ){

	static boolean init = TRUE;

	if( init ){
		init  = FALSE;
	    uart0_puts( " Entra en teclaMoveFireman\n" );  /* Muestra un mensaje inicial en la UART0 (no es necesario semáforo) */

	}else{

		if (keypad_pressed()){
			scancode = keypad_getchar();

					if(scancode == KEYPAD_KEY2){
						uart0_puts("nos movemos a la izquierda\n");
						sprite_clear(&firemen, firemanPos);
						firemanPos--;
						if(firemanPos < 0 || firemanPos > 3){
							firemanPos = 0;
							sprite_plot( &firemen, firemanPos);
						}else{
							sprite_plot( &firemen, firemanPos);
						}

					} else if(scancode == KEYPAD_KEY3){
						uart0_puts("nos movemos a la derecha\n");
						sprite_clear(&firemen, firemanPos);
						firemanPos++;
						if(firemanPos >= 3){
							firemanPos = 0;
							sprite_plot( &firemen, firemanPos);
						}else{
							sprite_plot( &firemen, firemanPos);
						}
					}
		}

	}
}

void draw_life( void ){
	uint8 i;
	for(i = 0; i<life.num_plots; i++ )           // Dibuja los corazones en todas sus posiciones posibles
		sprite_plot( &life, i );
}

void lose_life( void ){
	hearts--;
	uint8 i;

	for(i = 0; i<life.num_plots; i++ )           // borra los corazones en todas sus posiciones posibles
			sprite_clear( &life, i );

	if(hearts > 0){
		for( i=0; i<hearts; i++ )           // Dibuja los corazones en sus posiciones
			sprite_plot( &life, i );

	}else {
		gameOver = TRUE;
		lose_game = TRUE;
		sprite_clear(&dummy, dummyPos);
		uint8 i;
		boolean encontrado = FALSE;
		while(!encontrado){
			if(dummy.plots[2].x == crash.plots[i].x){
				sprite_clear(&crash, i);
				encontrado = TRUE;
			}else{
				i++;
			}
		}
	}
}

void start_game ( void ){
	lcd_putWallpaper( LANDSCAPE );              // Dibuja el fondo de la pantalla
	init_fireman(); 							// inicializa el bombero
	draw_life();								// vidas
	dummy_init();                               // Inicializa las tareas
	count_init();								// contador de dummies
	teclaMoveFireman();							// gestion movimientos de bomberos
}

void start_play( void ){
	//sprite_clear( &dummy, dummyPos );
	sprite_plot(&crash, caida);
	sw_delay_ms(400);

	uint16 i;
	sprite_clear( &firemen, firemanPos );
	sprite_clear( &dummy, dummyPos );
	sprite_clear(&crash, caida);

	sw_delay_ms(40);
	lcd_putWallpaper(LANDSCAPE);
	dummyPos = 0;
	firemanPos = 0;
	draw_life();
	sprite_plot(&dummy, dummyPos);
	sprite_plot(&firemen, firemanPos);
	lcd_putint_x2( 287, 0, BLACK, count );
	for(i = 0; i<life.num_plots; i++ )           // Dibuja los corazones en todas sus posiciones posibles
			sprite_plot( &life, i );

	draw_life();

	if(count == 9 && hearts > 1){
			gameOver = TRUE;
			win_game = TRUE;
	}
}

void exit_game( void ){
	uart0_puts( "Has decidido salir del juego\n" );
	lcd_puts_x2( 20, 50, BLACK, "saliendo..");
	lcd_puts_x2( 40, 90, BLACK, "BYE BYE!");
}

void win( void ){
	uart0_puts( "Has ganado la partida\n" );
	lcd_puts_x2( 20, 50, BLACK, "YOU WIN!");
	lcd_puts_x2( 40, 80, BLACK, "^ ^");
	lcd_puts_x2( 50, 90, BLACK, ".");
}

void lose( void ){
	uart0_puts( "Has perdido la partida\n" );
	lcd_puts_x2( 20, 50, BLACK, "YOU LOSE..");
	lcd_puts_x2( 40, 75, BLACK, "x     x");
	lcd_puts_x2( 50, 100, BLACK, " ___");
}

void initMusic( void ){
	uda1341ts_setvol(VOL_MAX);
	iis_playWawFile(MUSIC, TRUE);
	music = TRUE;
}

void pulsador ( void ){
	uint16 ticks;
	uart0_puts( "Lectura de pulsadores sin espera máxima usando pb_getchar():\n" );
	    ticks = 0;
	    while( ticks != 5 )
	    {
	        switch( pb_getchar() )
	        {
	            case PB_FAILURE:
	                uart0_puts( "  - Error de lectura\n" );
	                break;
	            case PB_LEFT:
	                uart0_puts( "  - El pulsador izquierdo ha sido presionado\n" );
	                iis_pause();
	                break;
	            case PB_RIGHT:
	                uart0_puts( "  - El pulsador derecho ha sido presionado\n" );
	                iis_continue();
	                break;
	        }
	        ticks++;
	    }
}


void isr_pbs( void ){
	static boolean init = TRUE;

	if(init){
		init = FALSE;
	}else{
		switch( pb_scan() )
		{
		case PB_FAILURE:
			EXTINTPND = BIT_LEFTPB | BIT_RIGHTPB;
			break;
		case PB_LEFT:
			EXTINTPND = BIT_LEFTPB;
			uart0_puts("boton izquierdo pausa\n");
			music = FALSE;
			iis_pause();
			break;
		case PB_RIGHT:
			uart0_puts("boton derecho play\n");
			music = TRUE;
			iis_continue();
			EXTINTPND = BIT_RIGHTPB;
			break;
		}
	}


    flagPbs = TRUE;
    I_ISPC = BIT_PB;
}

