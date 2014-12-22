/*
 * dlg2012.h
 *
 *  Created on: 19/03/2012
 *      Author: pablo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include "labjackusb.h"
#include "u3.h"
#include "sqlite3.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <wait.h>
#include <argp.h>
#include <stdarg.h>
#include <stddef.h>
#include <argp.h>
#include <syslog.h>
#include <signal.h>

#include <sys/ioctl.h>

#include <linux/usbdevice_fs.h>

#define VERSION (const char *)("2012 dlgLabjack V1.0 20130806a")

#define MAXFD 64

#define MAXLENPARAMETERS	64
#define DINCHBASE 			16  //din0..din3 -> CIO0..CIO3
#define TCPINOFFSET 		8   //T0/T1/C0/C1 -> EIO0..EIO3
#define EXTRAANALOGCHBASE	10

#define TRUE	1
#define FALSE 	0
#define OK		1
#define ERR		0

#define NIVEL		0
#define PULSO		1
#define CONTADOR	2

#define MAXTXBUFFER 	128
#define MAXBUFTIME		32
#define MAXRXFRAME		128
#define MAXLOGBUFFER	128
#define MAXSTMTLENGHT	128

// LINK -----------------------------------------------------------

#define LINKDOWN 		0
#define LINKESTABLISH	1
#define LINKSTANDBY		2
#define LINKUP			3
#define LINKERROR		4

int linkStatus;
int sockfd;

void sig_user1(int signo);

pthread_mutex_t socket_mlock;
pthread_mutex_t link_mlock;
pthread_cond_t socketRdy_cvlock;

pthread_t link_threadHandler;
void *link_thService(void *arg);

// CONFIG-----------------------------------------------------------
// Variables de configuracion

#define MAXCONFIGLINE 			128
#define NROCANALESANALOGICOS	8
struct {
	int intervaloDePoleo;
	int debug;
	char bd[MAXLENPARAMETERS];
	char dlgId[MAXLENPARAMETERS];
	char configFile[MAXLENPARAMETERS];
	char ipaddress[MAXLENPARAMETERS];
	int tcpPort;
	int daemonMode;
	int servicePollTime;
	int serviceLastTime;
	int saveClock;
	int writeToFile;
	int nroRecsInExtFile;
	int resyncOnline;
	int din0;
	int din1;
	int din2;
	int din3;

} systemParameters;

void conf_fnReadConfFile(void);
void conf_fnParseConfLine(char* linea);
void conf_fnPrintConfParams(void);

struct {
	int pktWaitingAck;
	int serviceMode;
} systemVars;

pthread_mutex_t systemVars_mlock;

// TICK --------------------------------------------------------------

#define TIMERTOSAVEDATE 60

void *tick_thService(void *arg);
void initClock(void);
void saveClock(void);
void clockResync( char *s);

pthread_t tick_threadHandler;

// LABJACK ----------------------------------------------------------
void *lbj_thService(void *arg);

pthread_t lbj_threadHandler;

// RX ---------------------------------------------------------------
void *rx_thService(void *arg);

pthread_t rx_threadHandler;

// SQL ---------------------------------------------------------------

sqlite3* db;
pthread_mutex_t sql_mlock;

#define CBK_CALLED		0
#define CBK_NOTCALLED	1

typedef struct {
	int id;						// Nro  de registro devuelto
	char frame[MAXTXBUFFER];	// Frame
	int callbackStatus;			// Si callback fue invocado o no
} sqlStruct;


void db_open(void);
int select_callback(void* data, int ncols, char** values, char** headers);
int SQLexecDelete(char *sql);
int SQLexecInsert(char *sql);
int SQLexecSelect(char *sql, sqlStruct* readData);

// DATAFILE ---------------------------------------------------------

FILE *fileFD;
void dataFile_open(void);
void writeToFile( char *frameBuffer );
void cleanUpFiles(void);

// MAIN: printing ---------------------------------------------------

#define NOLOG		0
#define LOGCONSOLE	1
#define LOGFILE		2
#define LOGSYS		3

#define MAXLINE 512

void printLog( char *logBuffer);

pthread_mutex_t printf_mlock;

void daemonize();
void sig_chld(int signo);

void F_syslog(const char *fmt,...);
void daemonize();

/*------------------------------------------------------------------*/
