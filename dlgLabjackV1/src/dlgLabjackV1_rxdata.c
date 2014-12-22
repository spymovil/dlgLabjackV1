/*
 * dlg2012_rxdata.c
 *
 *  Created on: 26/03/2012
 *      Author: pablo
 *
 *  Descripcion:
 *  Esta thread se encarga de recibir los datos del servidor y responderlos
 *  y realizar las acciones que implican los mensajes.
 *
 */

#include "dlgLabjackV1.h"

char rxFrame[MAXRXFRAME];
char logBuffer[MAXLOGBUFFER];

ssize_t readline(int fd, void *vptr, size_t maxlen);
void decodeRxFrame(void);
void resyncFrame(void);
void ackFrame(void);

/*---------------------------------------------------------------------------*/

void *rx_thService(void *arg)
{
int n;

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::rx_thService: Iniciando hilo rxdata...");
	F_syslog("%s", logBuffer);

	while(1) {

		// Espero a que el socket este disponible
		pthread_mutex_lock(&link_mlock);
			// No hay datos a procesar: espero
			while (linkStatus != LINKUP) {
				pthread_cond_wait(&socketRdy_cvlock, &link_mlock);
			}
		pthread_mutex_unlock(&link_mlock);

		// Quedo bloqueado leyendo el socket
		while ( ( n = readline(sockfd, rxFrame, MAXRXFRAME) ) > 0) {

			decodeRxFrame();
			bzero(&rxFrame, sizeof(rxFrame));

			pthread_mutex_lock(&link_mlock);
				linkStatus = LINKUP;
			pthread_mutex_unlock(&link_mlock);

		}

		// Sali de while porque paso algo....

		switch ( n ) {
		case 0:
			// El server cerro la conexion.
			pthread_mutex_lock(&link_mlock);
				linkStatus = LINKDOWN;
			pthread_mutex_unlock(&link_mlock);
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "INFO::rx_thService: El server cerro el socket.");
			F_syslog("%s", logBuffer);
			break;
		case -1:
			/* Error en el socket al leer: lo cierro */
			pthread_mutex_lock(&link_mlock);
				linkStatus = LINKERROR;
			pthread_mutex_unlock(&link_mlock);
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROr::rx_thService: Error al leer el socket.");
			F_syslog("%s", logBuffer);
			break;
		}

	}
}

/*--------------------------------------------------------------------------*/

ssize_t readline(int fd, void *vptr, size_t maxlen)
{
/* Usamos las funciones I/O standard UNIX y no las ANSI porque
   no queremos leer en modo buffereado sino poder procesar c/caracter
   en la medida que lo vamos recibiendo.
	No guardo el CR LF
*/
	ssize_t n, rc;
	char c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		again:
			if ( ( rc = read(fd, &c, 1)) == 1) {		/* Leo del socket y paso de a un caracter */
				if (c == '\0')
					goto again;
				*ptr++ = c;
				if (c == '\n')
					break;			/* Linea completa: almaceno el newline como fgets() */
			} else if (rc == 0) {
				if ( n == 1)
					return(0);		/* EOF, no lei ningun dato */
				else
					break;			/* EOF pero lei datos */
			} else {
				if (errno == EINTR) {
					goto again;
				}
				return (-1);		/* Errro: la valiable errno la seteo la funcion read() */
			}
	}

	*ptr = 0;	/* null terminate como fgets() */
	return(n);
}

/*--------------------------------------------------------------------*/

void decodeRxFrame(void)
{
char *token, cp[MAXRXFRAME];
char parseDelimiters[] = ">,<";
char dlgId[7];
char frameType[2];

		strcpy(cp, rxFrame);
		token = strtok(cp, parseDelimiters);

		/* DlgId */
		strcpy( dlgId, token);

		/* Frame Type */
		token = strtok(NULL, parseDelimiters);
		strncpy( frameType, token, 1);
		switch (frameType[0]) {
			case '?':
				/* En este caso el servidor me actualiza el RTC */
				/* >DL00,?,YYMMDD,hhmmss< */
				resyncFrame();
				break;

			case '!':
				/* >DL00,!,120702,215255< */
				ackFrame();
				break;

			default:
				bzero(logBuffer, sizeof(logBuffer));
				sprintf(logBuffer, "ERROR::decodeRxFrame: rx err frame: %s", rxFrame);
				F_syslog("%s", logBuffer);
				break;
		}

}
/*--------------------------------------------------------------------*/

void resyncFrame(void)
{
// >DL00:?:120326,174834<

int n;
char txBuffer[MAXTXBUFFER];

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::resyncFrame: rx resync frame: %s", rxFrame);
	F_syslog("%s", logBuffer);
	//
	// Respondo el mensaje
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(txBuffer, ">%s,?<\n", systemParameters.dlgId);
	pthread_mutex_lock(&socket_mlock);
		n = write(sockfd, txBuffer,strlen(txBuffer));
	pthread_mutex_unlock(&socket_mlock);

	bzero(logBuffer, sizeof(logBuffer));
	if (n < 0) {
    	// Error de trasmision
		pthread_mutex_lock(&link_mlock);
			linkStatus = LINKERROR;
		pthread_mutex_unlock(&link_mlock);
		sprintf(logBuffer, "ERROR::resyncFrame: error de trasmision de resync ack");
	} else {
		sprintf(logBuffer, "INFO::resyncFrame: tx resync ack: %s", txBuffer);
	}

	F_syslog("%s", logBuffer);

	// Extraigo la fecha y hora del sistema y me resincronizo
	clockResync(rxFrame);

}

/*--------------------------------------------------------------------*/

void ackFrame(void)
{
	// Al recibir un ACK habilito a trasmitir el segundo frame.
	pthread_mutex_lock(&systemVars_mlock);
		systemVars.pktWaitingAck = 0;
	pthread_mutex_unlock(&systemVars_mlock);

	// >DL00:!:120326,175015<
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::ackFrame: rx ack frame: %s", rxFrame);
	F_syslog("%s", logBuffer);

}

/*--------------------------------------------------------------------*/
