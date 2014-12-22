/*
 * dlg2012_rtc.c
 *
 *  Created on: 19/03/2012
 *      Author: pablo
 *
 *  Implementamos un thread que se ejecuta 1 vez por segundo.
 *  Nos sirve para poner funciones que necesitemos sean periodicas.
 *
 */
/*---------------------------------------------------------------------------*/

#include "dlgLabjackV1.h"

char logBuffer[MAXLOGBUFFER];

void saveSystemDate(void);
int compareNowDateVs(  char *cmpDateTime);
void setDate(char *dbDateTime);
/*---------------------------------------------------------------------------*/

void *tick_thService(void *arg)
{

int timerToSaveDate;
int pktAwaiting;
int resetLinkCounter;

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::tick_thService: Iniciando hilo Ticks...");
	F_syslog("%s", logBuffer);

	timerToSaveDate = systemParameters.saveClock;
	resetLinkCounter = 2 * systemParameters.intervaloDePoleo;;

	while (1) {

		sleep(1);

		// CountDown to save date.
		if (timerToSaveDate-- == 0) {
			timerToSaveDate = systemParameters.saveClock;
			saveSystemDate();
		}

		// Contdown para salir de modo service
		if (systemVars.serviceMode > 0 ) {
			if (systemVars.serviceMode == 1) {
				F_syslog("%s", "SERVICE: Salgo de modo service.");
			}
			systemVars.serviceMode--;
		}

		// Countdown para resetear el enlace si estoy esperando ACKs
		pthread_mutex_lock(&systemVars_mlock);
			pktAwaiting = systemVars.pktWaitingAck;
		pthread_mutex_unlock(&systemVars_mlock);

		if (pktAwaiting > 0 ) {
			if (resetLinkCounter-- == 0) {
				pthread_mutex_lock(&link_mlock);
					linkStatus = LINKDOWN;
				pthread_mutex_unlock(&link_mlock);
			}
		} else {
			resetLinkCounter = 2 * systemParameters.intervaloDePoleo;;
		}

	}
}

/*---------------------------------------------------------------------------*/

void initClock(void)
{
	/* Lee el clock actual del SBC y lo compara con el que lee de
	 * la base de datos.
	 * Si el clock actual es mas viejo que el de la base de datos (01/01/2000)
	 * entonces setea el clock actual con el que lee de la BD.
	 * Esto es porque al apagarse la SBC se pierde el reloj y al arrancar
	 * lo hace con 01/01/2000.
	 */

char dbDateTime[16];
char *stmt;
sqlStruct readData;
int nreg, sqlRet;
int diffTime;

	// Leo la hora de la BD
	stmt =  sqlite3_mprintf("select * from tbl2 order by id desc limit 1");
	sqlRet =  SQLexecSelect(stmt, &readData);
	nreg = readData.id;
	if ( nreg  > 0 ) {
		strncpy( dbDateTime, readData.frame, 16 );
	} else {
		// Error de lectura o no hay fechas guardadas (inicio)
		bzero(logBuffer, sizeof(logBuffer));
    	sprintf(logBuffer, "INFO::initClock: BD(tbl2) sin datos");
    	F_syslog("%s", logBuffer);
		return;
	}

	// Comparo las horas.
	diffTime = compareNowDateVs(dbDateTime);
	if (diffTime > 60) {
		// sysDateTime es mayor que la dbDateTime
	   	bzero(logBuffer, sizeof(logBuffer));
    	sprintf(logBuffer, "INFO::initClock: No necesito actualizar el clock (drift=%d)", diffTime);
    	F_syslog("%s", logBuffer);
		return;
	} else {
		// Actualizo el reloj interno con la dbDateTime.
		setDate(dbDateTime);
		bzero(logBuffer, sizeof(logBuffer));
    	sprintf(logBuffer, "INFO::initClock: Actualizo el clock.(drift=%d)",diffTime);
    	F_syslog("%s", logBuffer);
    	return;
	}

}

/*---------------------------------------------------------------------------*/

void saveSystemDate(void)
{

	// Periodicamente se salva la hora en la BD por si se apaga o resetea
	// que levante con una hora lo mas proxima a la de apagado.
	// El formato que se guarda es YYMMDDhhmmss

time_t sysClock;
struct tm *cur_time;
char sysDateTime[16];
char *stmt;

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::saveSystemDate: Saving sysclock...");
	F_syslog("%s", logBuffer);

	// Leo la hora del sistema en segundos desde el inicio
	sysClock = time (NULL);
	cur_time = localtime(&sysClock);
	strftime (sysDateTime, 16, "%y%m%d%H%M%S", cur_time);

	// Borro las horas viejas
	stmt =  sqlite3_mprintf("delete from  tbl2");
	if ( SQLexecDelete(stmt) != SQLITE_OK ) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROr::saveSystemDate: delete time: %s", stmt);
		F_syslog("%s", logBuffer);
	}

	// Actualizo la Bd con la nueva hora.
	bzero(logBuffer, sizeof(logBuffer));
	stmt =  sqlite3_mprintf("insert into tbl2 (storedDate) values (%s)", sysDateTime);
	if ( SQLexecInsert(stmt) != SQLITE_OK ) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::saveSystemDate: insert date %s", stmt);
		F_syslog("%s", logBuffer);
	}
}

/*---------------------------------------------------------------------------*/

void clockResync( char *s)
{
	// Compara la hora recibida con la actual. Si la diferencia es de mas
	// de 60s la reconfigura.

char *token, cp[MAXRXFRAME];
char parseDelimiters[] = ">,<";
char fecha[7], hora[7], rcvdDateTime[16];
int diffTime;

	// Parseo la fecha y hora.
	strcpy(cp, s);
	token = strtok(cp, parseDelimiters);
	token = strtok(NULL, parseDelimiters);  /* Frame Type */
	token = strtok(NULL, parseDelimiters);  /* Fecha */
	strcpy(fecha, token);
	token = strtok(NULL, parseDelimiters);  /* Hora */
	strcpy(hora, token);

	bzero(rcvdDateTime, sizeof(rcvdDateTime));
	sprintf(rcvdDateTime,"%s%s", fecha, hora);

	// Comparo las horas.
	diffTime = compareNowDateVs(rcvdDateTime);
	if ( abs(diffTime) < 60) {
		// sysDateTime es mayor que la dbDateTime
	   	bzero(logBuffer, sizeof(logBuffer));
    	sprintf(logBuffer, "INFO::clockResync: No necesito actualizar el clock (drift=%d)", diffTime);
    	F_syslog("%s", logBuffer);
		return;
	} else {
		// Actualizo el reloj interno con la dbDateTime.
		setDate(rcvdDateTime);
		bzero(logBuffer, sizeof(logBuffer));
    	sprintf(logBuffer, "INFO::clockResync: Actualizo el clock.(drift=%d)",diffTime);
    	F_syslog("%s", logBuffer);
    	return;
	}
}
/*---------------------------------------------------------------------------*/

int compareNowDateVs( char *cmpDateTime)
{
time_t sysClock;
struct tm *cur_time;
char sysDateTime[16];
char *p;
double sysDate, cmpDate;

// Leo la hora del sistema en segundos desde el inicio
	sysClock = time (NULL);
	cur_time = localtime(&sysClock);
	strftime (sysDateTime, 16, "%y%m%d%H%M%S", cur_time);

   	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::compareNowDateVs: Compare Dates (%s .VS. %s)", sysDateTime, cmpDateTime );
	F_syslog("%s", logBuffer);

	sysDate = strtod(sysDateTime, &p);
	cmpDate = strtod(cmpDateTime, &p);

	return( (int)(sysDate-cmpDate));

}

/*---------------------------------------------------------------------------*/

void setDate(char *dbDateTime)
{

char dInput[24];
char newDate[16];
int bufPos;
char tmp[3];

	if ( systemParameters.resyncOnline == 0 ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "DEBUG::setDate: Not Resync allowed");
		F_syslog("%s", logBuffer);
		return;
	}

		strcpy(newDate,dbDateTime );

		bzero(dInput, sizeof(dInput));
		bufPos = 0;
	    bufPos += sprintf(dInput + bufPos, "date ");
	    bzero(tmp, sizeof(tmp));
	    memcpy(tmp, &newDate[2], 2);
	    bufPos += sprintf(dInput + bufPos, "%s", tmp); //MM
	    memcpy(tmp, &newDate[4], 2);
	    bufPos += sprintf(dInput + bufPos, "%s", tmp); //MMDD
	    memcpy(tmp, &newDate[6], 2);
	    bufPos += sprintf(dInput + bufPos, "%s", tmp); //MMDDhh
	    memcpy(tmp, &newDate[8], 2);
	    bufPos += sprintf(dInput + bufPos, "%s", tmp); //MMDDhhmm
	    memcpy(tmp, &newDate[0], 2);
	    bufPos += sprintf(dInput + bufPos, "%s", tmp); //MMDDhhmmYY

		//system(dInput);
		bzero(logBuffer, sizeof(logBuffer));
		if ( system(dInput) == 0 ) {
			sprintf(logBuffer, "DEBUG::setDate: OK");
		} else {
			sprintf(logBuffer, "DEBUG::setDate: ERROR");
		}
		F_syslog("%s", logBuffer);



}

/*---------------------------------------------------------------------------*/
