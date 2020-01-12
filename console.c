#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* POSIX Header files */
#include <pthread.h>
#include <semaphore.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>
#ifdef CC32XX
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC32XX.h>
#endif

/* Driver configuration */
#include "ti_drivers_config.h"


/* Pong Section */

#define UP 72 /* up arrow key code */
#define ESC 27 /* escape key code */
#define DOWN 80 /* down arrow key code */

const char cleanDisplay[]     = "\f";
char testUART[]     = "\r\nTEST PRINT\r\n";

void reverse_direction (); /* reverses the direction of the ball */
void refresh (UART_Handle uart); /* prints game process */
int calculate (int c, UART_Handle uart); /* calculates next positions of objects (player1, player2, ball, etc.) */
void init (); /* initializes the player1, player2, ball structures */


struct
{
    int x, y; /* positions */
} player1, player2; /* left and right players */


struct
{
    int x, y; /* positions */
    struct
    {
        int x, y; /* how fast the ball moves */
    } speed;
    enum
    {
        LEFT_DOWN = 1,
        LEFT_UP,
        RIGHT_DOWN,
        RIGHT_UP
    } direction; /* contains information about what direction the ball should move in */
} ball;


void pongMain (UART_Handle uart)
{
    char testUART[] = "\r\nmain\r\n";
    UART_write(uart, testUART, sizeof(testUART));
    int c; /* stores code of a pressed key */
    init (); /* initializes the structures */
    refresh (uart); /* clears the screen and prints characters */
    while (c = getchar ())
        if (calculate (c, uart)) /* refresh () only if one if certain keys was pressed, if the escape key was pressed it calls exit (0) */
            refresh (uart);
}


void refresh (UART_Handle uart)
{
    int i = 0, j = 0;
    UART_write(uart, cleanDisplay, sizeof(cleanDisplay));
    char testUART[] = "\r\nrefresh\r\n";
    UART_write(uart, testUART, sizeof(testUART));
    while (i < 25)
    {
        j = 0;
        while (j < 80)
        {
            if ((j == player1.x && i == player1.y) || (j == player2.x && i == player2.y) || (j == ball.x && i == ball.y)) {
                const char ch[] = {219};
                UART_write(uart, ch, sizeof(ch));
            }
            else {
                const char dh[] = " ";
                UART_write(uart, dh, sizeof(dh));
            }
            ++j;
        }
        ++i;
    }
    return;
}


void init ()
{
    player1.x = 0;
    player1.y = 0;
    player2.x = 79;
    player2.y = 0;
    ball.x = 50;
    ball.y = 15;
    ball.speed.x = 1;
    ball.speed.y = 1;
    ball.direction = RIGHT_DOWN;
    return;
}


int calculate (int c, UART_Handle uart)
{
    if ((((ball.x == (player1.x + 1)) && (ball.y == player1.y)) || (ball.x == (player2.x - 1) && (ball.y == player2.y))) || (ball.y == 0 || ball.y == 24))
        reverse_direction ();
    if (ball.direction == LEFT_DOWN)
    {
        ball.x -= ball.speed.x;
        ball.y += ball.speed.y;
    }
    else if (ball.direction == LEFT_UP)
    {
        ball.x -= ball.speed.x;
        ball.y -= ball.speed.y;
    }
    else if (ball.direction == RIGHT_UP)
    {
        ball.x += ball.speed.x;
        ball.y -= ball.speed.y;
    }
    else if (ball.direction == RIGHT_DOWN)
    {
        ball.x += ball.speed.x;
        ball.y += ball.speed.y;
    }
    if (c == UP)
        player1.y--;
    else if (c == DOWN)
        player1.y++;
    else if (c == 'q')
        player2.y--;
    else if (c == 'a')
        player2.y++;
    else if (c == ESC)
    {
        UART_write(uart, cleanDisplay, sizeof(cleanDisplay));
        char testUART[] = "\r\ncalculate\r\n";
        UART_write(uart, testUART, sizeof(testUART));
    }
    else
        return 0;
    return 1;
}


void reverse_direction (void)
{
    if (ball.direction == RIGHT_UP)
        ball.direction = RIGHT_DOWN;
    if (ball.direction == RIGHT_DOWN)
        ball.direction = RIGHT_UP;
    if (ball.direction == LEFT_DOWN)
        ball.direction = LEFT_UP;
    if (ball.direction == LEFT_UP)
        ball.direction = LEFT_DOWN;
    return;
}

/* Console display strings */
const char consoleDisplay[]   = "\fConsole (h for help)\r\n";
const char helpPrompt[]       = "Valid Commands\r\n"                  \
                                "--------------\r\n"                  \
                                "h: help\r\n"                         \
                                "q: quit and shutdown UART\r\n"       \
                                "c: clear the screen\r\n"             \
                                "p: play pong\r\n";                   \
const char byeDisplay[]       = "Bye! Hit button1 to start UART again\r\n";
const char userPrompt[]       = "> ";
const char readErrDisplay[]   = "Problem read UART.\r\n";
const char pongSplash[]       = "Pong starting...\r\n\r\nPress Enter to Continue";

/* Used to determine whether to have the thread block */
volatile bool uartEnabled = true;
sem_t semConsole;

/* Used itoa instead of sprintf to help minimize the size of the stack */
static void itoa(int n, char s[]);

/*
 *  ======== gpioButtonFxn ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_1.
 *  There is no debounce logic here since we are just looking for
 *  a button push. The uartEnabled variable protects use against any
 *  additional interrupts cased by the bouncing of the button.
 */
void gpioButtonFxn(uint_least8_t index)
{

    GPIO_write(CONFIG_GPIO_GLED, 1);
    /* If disabled, enable and post the semaphore */
    if (uartEnabled == false) {
        uartEnabled = true;
        sem_post(&semConsole);
    }
}

void gpioTrigFxn(uint_least8_t index)
{

    /* If disabled, enable and post the semaphore */
    if (uartEnabled == false) {
        uartEnabled = true;
        sem_post(&semConsole);
    }
}

void gpioEchoFxn(uint_least8_t index)
{

    /* If disabled, enable and post the semaphore */
    if (uartEnabled == false) {
        uartEnabled = true;
        sem_post(&semConsole);
    }
}

/*
 *  ======== simpleConsole ========
 *  Handle the user input. Currently this console does not handle
 *  user back-spaces or other "hard" characters.
 */
void simpleConsole(UART_Handle uart)
{

    char cmd;
    int status;
    char oop;


    UART_write(uart, consoleDisplay, sizeof(consoleDisplay));

    /* Loop until read fails or user quits */
    while (1) {
        UART_write(uart, userPrompt, sizeof(userPrompt));
        status = UART_read(uart, &cmd, sizeof(cmd));
        if (status == 0) {
            UART_write(uart, readErrDisplay, sizeof(readErrDisplay));
            cmd = 'q';
        }


        switch (cmd) {
            case 'c':
                UART_write(uart, cleanDisplay, sizeof(cleanDisplay));
                break;
            case 'p':
                UART_write(uart, cleanDisplay, sizeof(cleanDisplay));
                UART_write(uart, pongSplash, sizeof(pongSplash));
                status = UART_read(uart, &oop, sizeof(oop));
                pongMain(uart);
                break;
            case 'q':
                UART_write(uart, byeDisplay, sizeof(byeDisplay));
                return;
            default:
                UART_write(uart, helpPrompt, sizeof(helpPrompt));
                break;
        }
    }
}

/*
 *  ======== consoleThread ========
 */
void *consoleThread(void *arg0)
{
    UART_Params uartParams;
    UART_Handle uart;
    int retc;

#ifdef CC32XX
    /*
     *  The CC3220 examples by default do not have power management enabled.
     *  This allows a better debug experience. With the power management
     *  enabled, if the device goes into a low power mode the emulation
     *  session is lost.
     *  Let's enable it and also configure the button to wake us up.
     */
    PowerCC32XX_Wakeup wakeup;

    PowerCC32XX_getWakeup(&wakeup);
    wakeup.wakeupGPIOFxnLPDS = gpioButtonFxn;
    PowerCC32XX_configureWakeup(&wakeup);
    Power_enablePolicy();
#endif

    /* Configure the button pin */
    GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* install Button callback and enable it */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn);
    GPIO_enableInt(CONFIG_GPIO_BUTTON_1);

    /* Configure the echo pin */
    GPIO_setConfig(CONFIG_GPIO_ECHO, GPIO_CFG_INPUT | GPIO_CFG_IN_INT_NONE);

    /* install Button callback and enable it */
    GPIO_setCallback(CONFIG_GPIO_ECHO, gpioEchoFxn);
    GPIO_enableInt(CONFIG_GPIO_ECHO);



    /* Configure the trig pin */
    GPIO_setConfig(CONFIG_GPIO_TRIG, GPIO_CFG_OUT_LOW | GPIO_CFG_IN_INT_NONE);

    /* install Button callback and enable it */
    GPIO_setCallback(CONFIG_GPIO_TRIG, gpioTrigFxn);
    GPIO_enableInt(CONFIG_GPIO_TRIG);

    retc = sem_init(&semConsole, 0, 0);
    if (retc == -1) {
        while (1);
    }

    UART_init();

    /*
     *  Initialize the UART parameters outside the loop. Let's keep
     *  most of the defaults (e.g. baudrate = 115200) and only change the
     *  following.
     */
    UART_Params_init(&uartParams);
    uartParams.writeDataMode  = UART_DATA_BINARY;
    uartParams.readDataMode   = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;

    /* Loop forever to start the console */
    while (1) {
        if (uartEnabled == false) {
            retc = sem_wait(&semConsole);
            if (retc == -1) {
                while (1);
            }
        }

        /* Create a UART for the console */
        uart = UART_open(CONFIG_UART_0, &uartParams);
        if (uart == NULL) {
            while (1);
        }

        simpleConsole(uart);

        /*
         * Since we returned from the console, we need to close the UART.
         * The Power Manager will go into a lower power mode when the UART
         * is closed.
         */
        UART_close(uart);
        uartEnabled = false;
    }
}

/*
 * The following function is from good old K & R.
 */
static void reverse(char s[])
{
    int i, j;
    char c;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/*
 * The following function is from good old K & R.
 */
static void itoa(int n, char s[])
{
    int i, sign;

    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
         s[i++] = '-';
    s[i] = '\0';
    reverse(s);
}
