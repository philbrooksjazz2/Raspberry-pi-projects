/* 
 * hx711.c - code for load cell chip hx711
 * Copyright (C) Phil Brooks 2020.
 *
 * Note - keep the I2C wire length from the RPI board to the hx711 as short as possible.
 *
 */


#include <stdio.h>
#include "rpi_io.h"
#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define CLOCK_PIN	24
#define DATA_PIN	23
#define N_SAMPLES       200	
#define SPREAD		10
#define AVG_W           60 
//#define AVG_W           5 
#define CLK_DLY         20

#define SCK_ON  (GPIO_SET0 = (1 << CLOCK_PIN))
#define SCK_OFF (GPIO_CLR0 = (1 << CLOCK_PIN))
#define DT_R    (GPIO_IN0  & (1 << DATA_PIN))

void           reset_converter(void);
int            read_cnt(int offset, int argc);
void           set_gain(int r);
void           setHighPri (void);
int            log_print(char *pLogData);
int debug_print = 0;
FILE *fp;


void setHighPri (void)
{
    struct sched_param sched ;
    memset (&sched, 0, sizeof(sched)) ;
    sched.sched_priority = 10 ;

    if (sched_setscheduler (0, SCHED_FIFO, &sched))
    {
        printf ("Warning: Unable to set high priority\n");
    }
}

int log_print(char *sData)
{
    char sName[64]; 
    char sTimeStr[128];
    char pFileBuf[256];
    int nRet;

    time_t cur_time;
    cur_time = time(NULL);

    
    //(char *)sTimeStr = ctime(&cur_time);
    sprintf(pFileBuf,"%s %s\n",ctime(&cur_time),sData);
    nRet = fputs(pFileBuf,fp);
    fflush(fp);
    return nRet;

}

void setup_gpio()
{

    INP_GPIO(DATA_PIN);
    INP_GPIO(CLOCK_PIN);  
    OUT_GPIO(CLOCK_PIN);
    SCK_OFF;

}

void unpull_pins()
{
     GPIO_PULL = 0;
     GPIO_PULLCLK0 = 1 << DATA_PIN;
     GPIO_PULL = 0;
     GPIO_PULLCLK0 = 0;
} 
int pump_run(int nSec)
{
    system("/bin/g40");
    sleep(nSec);
    system("/bin/g41");

}


int main(int argc, char **argv)
{
    int i, j;
    int tmp=0;
    int tmp_avg=0;
    int tmp_avg2;
    int offset=0;
    float filter_low, filter_high;
    float spread_percent = SPREAD / 100.0 /2.0;
    int b;
    int nAvg = AVG_W;
    int nsamples=N_SAMPLES;
    int samples[nAvg];
    int nTrip = 0;
    int water_on = 0;
    char sLog[64];
    int nStatus = 0;
    fp = fopen("hx711.log","a");

    if ( fp == NULL )
    {
        printf("log file open error!!\n");
        exit(1);
    }

    if (argc >= 2) 
    {
        offset = atol(argv[1]);
    }
    else
    {
        printf("Using default offset\n");
        offset = 0;
    }
    if (argc >= 3) 
    {
        nTrip = atol(argv[2]);
    }
    else
    {
        printf("Using default trip\n");
        nTrip = 7000;
    }
    if (argc >= 4) 
    {
        water_on = atoi(argv[3]);
    }
    else
    {
        printf("Using default on time\n");
        water_on = 10;
    }
    if (argc >= 5) 
    {
       debug_print = 1;
    }

    setHighPri();
    setup_io();
    setup_gpio();
    reset_converter();

    j=0;

    int nFirst = read_cnt(offset, argc);
    printf("%d \n",nFirst);

    // get the dirty samples and average them
    while(1)
    {
        for(i=0;i<nAvg;i++) 
        {
            reset_converter();
            tmp_avg += read_cnt(offset, argc);
            usleep(5000);
        }

        tmp_avg = tmp_avg / nAvg;
        sprintf(sLog,"Avg = %d", tmp_avg);
        nStatus = log_print(sLog);
        if ( tmp_avg < nTrip )
        {
            nStatus =  log_print("pump start");
            pump_run(water_on);
        }
        tmp_avg = 0;
    }

    tmp_avg2 = 0;
    j=0;

    //filter_low =  (float) tmp_avg * (1.0 - spread_percent);
    //filter_high = (float) tmp_avg * (1.0 + spread_percent);

    // printf("%d %d\n", (int) filter_low, (int) filter_high);


    if (j == 0) 
    {
        printf("No data to consider\n");
        exit(255);

    }
    printf("%d\n", (tmp_avg2 / j) - offset);

    unpull_pins();
    restore_io();
}


void reset_converter(void) {
	SCK_ON;
	usleep(60);
	SCK_OFF;
	usleep(60);
}

void set_gain(int r) {
	int i;

// r = 0 - 128 gain ch a
// r = 1 - 32  gain ch b
// r = 2 - 63  gain ch a

	while( DT_R ); 

	for (i=0;i<24+r;i++) {
		SCK_ON;
		SCK_OFF;
	}
}


int read_cnt(int offset, int argc) 
{
    int count;
    int i;
    int b;


    count = 0;


    while( DT_R ); 

    usleep(8);
    for(i=0; i<24; i++) 
    {
	SCK_ON;
        count = count << 1;
	b++;
	usleep(CLK_DLY);
        SCK_OFF;
	b++;
	usleep(CLK_DLY);
        if (DT_R > 0 ) 
        { 
	    usleep(CLK_DLY);
            count++; 
        }
	usleep(CLK_DLY);
    }


    SCK_ON;
    b++;
    usleep(CLK_DLY);
    SCK_OFF;
    b++;
    usleep(CLK_DLY);
//  count = ~0x1800000 & count;
    count ^= 0x800000;


    // coarse offset
    count = count - 8334004;

    if (count & 0x800000) 
    {
	    //count |= (long) ~0xffffff;
    }

    // if things are broken this will show actual data


    if ( debug_print )
    {
        for (i=31;i>=0;i--) 
        {
            printf("%d ", ((count-offset) & ( 1 << i )) != 0 );
        }

        printf("n: %10d     -  ", count - offset);
        printf("\n"); 

    }
    return (count - offset);

}


