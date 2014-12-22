/*
 * dlg2012_datalink.c
 *
 *  Created on: 21/03/2012
 *      Author: pablo
 *
 *  Descripcion:
 *  Este modulo se encarga de mantener el enlace con el servidor.
 *  - Si el enlace esta caido intenta abrir el socket.
 *  - Cuando abre un socket, si hay datos para trasmitir en el archivo de offline
 *    los tasmite.
 *  - Monitorea el enlace
 *  - Responde los mensajes del servidor (keepAlive)
 *  - Setea la hora local con los datos mandados por el server para que esten siempre
 *    sincronizados.
 *
 *	Modificaciones 2012-12-27:
 *	- Siempre trasmito aunque tenga paquetes pendientes.
 *	- Si tengo paquetes pendientes trasmito c/10s y no en rafaga.
 *	- Cuando tenga 3 ACK pendientes, marco el enlace como caido
 *
 */

#include "dlgLabjackV1.h"

void link_fnLinkError(void);
void link_fnLinkEstablished(void);
void link_fnLinkDown(void);
void link_fnLinkUp(void);
int txFrame(sqlStruct *readData);
int readRssi(void);

char logBuffer[MAXLOGBUFFER];
char txBuffer[MAXTXBUFFER];

/*---------------------------------------------------------------------------*/

void *link_thService(void *arg)
{

int linkSt;

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::link_thService: Iniciando hilo Datalik...");
	F_syslog("%s", logBuffer);

	sleep(10);

	while(1) {

		// Leo el estado del enlace.
		pthread_mutex_lock(&link_mlock);
			linkSt = linkStatus;
		pthread_mutex_unlock(&link_mlock);

		switch (linkSt) {
			case LINKDOWN:
				link_fnLinkDown();
				break;
			case LINKESTABLISH:
				// Enlace establecido: trasmito mensaje inicial
				link_fnLinkEstablished();
				break;
			case LINKUP:
				// Enlace levantado. Si hay datos los trasmito
				link_fnLinkUp();
				break;
			case LINKERROR:
				link_fnLinkError();
				break;
		}
	}
}


/*---------------------------------------------------------------------------*/

void link_fnLinkUp(void)
{

int sqlRet, txRet, linkSt;
sqlStruct readData;
int pktAwaiting;
char *stmt;

// Entry
	sqlRet = 0;
	pktAwaiting = 0;

	// Log
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::link_fnLinkUp: Link UP.");
	F_syslog("%s", logBuffer);

// Loop
	while (1) {

		// Si estoy en modo service no hago nada mas
		if (systemVars.serviceMode > 0 ) {
			sleep(10);
			continue;
		}

		// Leo el status del link.
		pthread_mutex_lock(&link_mlock);
			linkSt = linkStatus;
		pthread_mutex_unlock(&link_mlock);

		// Si cambio el estado del enlace salgo.
		if (linkSt != LINKUP)
			return;

		// Primero vemos si podemos trasmitir
		pthread_mutex_lock(&systemVars_mlock);
			pktAwaiting = systemVars.pktWaitingAck;
		pthread_mutex_unlock(&systemVars_mlock);

		if (pktAwaiting != 0 ) {
			sprintf(logBuffer, "INFO::link_fnLinkUp: awaiting ACK.");
			F_syslog("%s", logBuffer);
			// Como tengo ACKs pendientes no trasmito en rafaga sino que dejo un tiempo
			sleep(10);
		}

		// Con 3 mensajes pendientes, supongo enlace caido.
		if (pktAwaiting == 3 ) {
			sprintf(logBuffer, "INFO::link_fnLinkUp: 3 awaiting ACK. Reset Link.");
			F_syslog("%s", logBuffer);
			close(sockfd);
			pthread_mutex_lock(&link_mlock);
				linkStatus = LINKERROR;
			pthread_mutex_unlock(&link_mlock);
		}

		// Consulto a la bd por datos.
		bzero(&readData, sizeof(sqlStruct));
		stmt =  sqlite3_mprintf("select * from tbl1 order by id limit 1");
		sqlRet =  SQLexecSelect(stmt, &readData);
		// Si hubo un error en la consulta espero y reinento
		if  (sqlRet != SQLITE_OK ) {
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROR::link_fnLinkUp: Sql read error.");
			F_syslog("%s", logBuffer);
			sleep(30);
			continue;
		}

		// Consulta correcta.
		// Si hay datos en la BD los trasmito
		if (readData.id  > 0 ) {
			txRet = txFrame(&readData);
			// Si todo anduvo bien espero 1s y continuo
			if (txRet == OK) {
				sleep(1);
				continue;
			} else {
				// Error en trasmision.
				pthread_mutex_lock(&link_mlock);
					linkStatus = LINKERROR;
				pthread_mutex_unlock(&link_mlock);
				continue;
			}
		}

		// No hay datos para trasmitir.
		sleep(10);
	}
}

/*---------------------------------------------------------------------------*/

int txFrame(sqlStruct *readData)
{

int n;
char *stmt;

	// Marco que tengo paquetes pendientes de ACK
	// Lo debo hacer antes por problemas de sincronismo de semaforos.
	pthread_mutex_lock(&systemVars_mlock);
		systemVars.pktWaitingAck++;
	pthread_mutex_unlock(&systemVars_mlock);

	// Trasmito.
	pthread_mutex_lock(&socket_mlock);
		n = write(sockfd, readData->frame, strlen(readData->frame));
	pthread_mutex_unlock(&socket_mlock);

	if (n < 0) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::txFrame: tx error.");
		F_syslog("%s", logBuffer);
		sleep(10);
		return(ERR);

	} else {
		// Trasmision correcta.
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "INFO::txFrame: tx OK: %s", readData->frame);
		F_syslog("%s", logBuffer);


		// Borro el registro de la BD.
		bzero(logBuffer, sizeof(logBuffer));
		stmt =  sqlite3_mprintf("delete from tbl1 where id=%d", readData->id );
		if ( SQLexecDelete(stmt) == SQLITE_OK ) {
			// Log & info.
			sprintf(logBuffer, "INFO::txFrame: sql delete ok %d", readData->id );
		} else {
			sprintf(logBuffer, "ERROR::txFrame: sql delete (%d) err.", readData->id );
		}
		F_syslog("%s", logBuffer);

		return(OK);
	}
}

/*---------------------------------------------------------------------------*/

void link_fnLinkEstablished(void)
{
// El socket y la conexion al server se abriron.
// Debo mandar el mensaje de inicio

int n;
int rssi;

	// Leo el RSSI
	rssi = readRssi();

	/* Frame a enviar: >DLxx:$:HW:SBC+LbjU3,OS:Linux 2.6 FW:dlg2012v1.0 20120326,00< */
	bzero(txBuffer, sizeof(txBuffer));
	sprintf(txBuffer, ">%s,$,HW=BC+LbjU3,OS=Linux Deb 3.2.0-psp7,FW=%s,%d<\n", systemParameters.dlgId, VERSION, rssi);
	pthread_mutex_lock(&socket_mlock);
		n = write(sockfd, txBuffer,strlen(txBuffer));
	pthread_mutex_unlock(&socket_mlock);

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::link_fnLinkEstablished: tx INIT: %s", txBuffer);
	F_syslog("%s", logBuffer);

	if (n < 0) {
    	// Error de trasmision
		sprintf(logBuffer, "ERROR::link_fnLinkEstablished: error de trasmision inicial. Espero 30 secs.");
		F_syslog("%s", logBuffer);
		pthread_mutex_lock(&link_mlock);
			linkStatus = LINKERROR;
		pthread_mutex_unlock(&link_mlock);
		sleep(30);
		return;
	}

	// Aqui es que el socket abrio y esta disponible
	pthread_mutex_lock(&link_mlock);
		linkStatus = LINKUP;
	pthread_mutex_unlock(&link_mlock);

	// Habilito a trasmitir
	pthread_mutex_lock(&systemVars_mlock);
		systemVars.pktWaitingAck = 0;
	pthread_mutex_unlock(&systemVars_mlock);

	// Aviso.
	pthread_cond_broadcast(&socketRdy_cvlock);

}

/*---------------------------------------------------------------------------*/

void link_fnLinkError(void)
{
	// El alguna parte se detecto un error. Debo reiniciar las comms. cerrando el socket
	if ( close(sockfd) < 0 ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::link_fnLinkError: error al cerrar socket. Espero 30 secs.");
		F_syslog("%s", logBuffer);
		sleep(30);
	}

	//shutdown(sockfd,2);
	// marco el enlace como caido.
	pthread_mutex_lock(&link_mlock);
		linkStatus = LINKDOWN;
	pthread_mutex_unlock(&link_mlock);
}

/*---------------------------------------------------------------------------*/

void link_fnLinkDown(void)
{
// El link esta down. Intento abrirlo.
struct sockaddr_in serv_addr;
int linkSt;

// Entry:

// Loop:
	while (1) {

		// Leo el status del link.
		pthread_mutex_lock(&link_mlock);
			linkSt = linkStatus;
		pthread_mutex_unlock(&link_mlock);

		// Si cambio el estado del enlace salgo.
		if (linkSt != LINKDOWN)
			return;

		// Log
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "INFO::link_fnLinkDown: Link Down.Intento abrir socket.");
		F_syslog("%s", logBuffer);

		// Intento abrir el socket
		if ( (sockfd = socket(AF_INET, SOCK_STREAM,0)) < 0) {
			// No me conecte.
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROR::link_fnLinkDown: error al abrir socket. Espero 60 secs.");
			F_syslog("%s", logBuffer);
			// Espero 60s para reintentar
			sleep(60);
			continue;
		}

		// Abri el socket. Intento conectarme
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(systemParameters.tcpPort);
		inet_pton(AF_INET, &systemParameters.ipaddress, &serv_addr.sin_addr);
		if (connect(sockfd,&serv_addr,sizeof(serv_addr)) < 0) {
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROR::link_fnLinkDown: error al conectarme al server. Espero 60 secs.");
			F_syslog("%s", logBuffer);
			// Espero 60s para reintentar
			sleep(60);
			continue;
		}

		// Si llegue aqui es que estoy conectado
		break;
	}

// Exit

	// Estoy conectado. Salgo
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::link_fnLinkDown: Conexion establecida.");
	F_syslog("%s", logBuffer);
	// Marco el nuevo estado
	pthread_mutex_lock(&link_mlock);
		linkStatus = LINKESTABLISH;
	pthread_mutex_unlock(&link_mlock);

}

/*---------------------------------------------------------------------------*/

int readRssi(void)
{
// Leo del archivo /var/log/wvdial.log una linea del tipo +ZRSSI: 43,16,102

FILE *fp;
char line[64];			/* line of data from unix command*/
char *token, *rs;
int RS;
char parseDelimiters[] = " :,";

	fp = popen("cat /var/log/wvdial.log | grep +ZRSSI:", "r");		/* Issue the command. */
	/* Read a line			*/
	if ( fgets( line, sizeof(line), fp) > 0 )
	{
		token = strtok(line, parseDelimiters);	//+ZRSSI:
		rs = strtok(NULL, parseDelimiters);		// 43
		RS=atoi(rs);
		//bzero(logBuffer, sizeof(logBuffer));
		//sprintf(logBuffer, "rssi line [%s][%d]", rs, RS);
		//F_syslog("%s", logBuffer);
		return(RS);

	} else {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "rssi Error");
		F_syslog("%s", logBuffer);
		return(0);
	}

	if (pclose(fp) == -1) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::readRssi pclose FAIL");
		F_syslog("%s", logBuffer);
		return(-1);
	}
}
/*---------------------------------------------------------------------------*/
