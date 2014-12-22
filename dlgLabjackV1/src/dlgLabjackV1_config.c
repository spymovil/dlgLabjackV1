/*
 * dlg2012_config.c
 *
 *  Created on: 19/03/2012
 *      Author: pablo
 */

/*--------------------------------------------------------------------------*/

#include "dlgLabjackV1.h"

char logBuffer[MAXLOGBUFFER];


void conf_fnReadConfFile(void)
{

/* Lee el archivo de configuracion (default o pasado como argumento del programa
   Extraemos todos los parametros operativos y los dejamos en una estructura de
   configuracion.

 */
	FILE *configFile_fd;
	char linea[MAXCONFIGLINE];
	if (  (configFile_fd = fopen(systemParameters.configFile, "r" ) )  ==  NULL ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::conf_fnReadConfFile: No es posible abrir el archivo de configuracion %s\n", systemParameters.configFile);
		F_syslog("%s", logBuffer);
		abort();
	}

	bzero(&systemParameters, sizeof(systemParameters));

	/* Valores por defecto */
	systemParameters.daemonMode = 0;
	systemParameters.intervaloDePoleo = 60;
	systemParameters.debug = 0;
	strcpy(systemParameters.bd, "/root/bin/dlg.db");
	strcpy(systemParameters.dlgId, "UTE000");
	strcpy(systemParameters.ipaddress, "0.0.0.0");
	systemParameters.tcpPort = 2006;
	systemParameters.servicePollTime = 5;
	systemParameters.serviceLastTime = 300;
	systemParameters.saveClock = 300;
	systemParameters.writeToFile = 0;
	systemParameters.nroRecsInExtFile = 10;
	systemParameters.resyncOnline = 1;
	systemParameters.din0 = NIVEL;	// Por defecto quedan en modo NIVEL
	systemParameters.din1 = NIVEL;
	systemParameters.din2 = NIVEL;
	systemParameters.din3 = NIVEL;

	/* Leo el archivo de configuracion para reconfigurarme */
	while ( fgets( linea, MAXCONFIGLINE, configFile_fd)  != NULL ) {
		conf_fnParseConfLine(linea);
	}
}

/*--------------------------------------------------------------------------*/

void conf_fnParseConfLine(char* linea)
{
/* Tenemos una linea del archivo de configuracion.
 * Si es comentario ( comienza con #) la eliminamos
 * Si el primer caracter es CR, LF, " " la eliminamos.
*/

char *index;
int largo;

	if (linea[0] == '#') return;

	/* Busco los patrones de configuracion y los extraigo */

	// Configuracion de DIN0..DIN3
	if ( strstr ( linea, "DIN0") != NULL ) {
		if ( strstr ( linea, "DIN0=NIVEL") != NULL ) {
			systemParameters.din0 = NIVEL;
			return;
		}
		if ( strstr ( linea, "DIN0=PULSO") != NULL ) {
			systemParameters.din0 = PULSO;
			return;
		}
		if ( strstr ( linea, "DIN0=CONTADOR") != NULL ) {
			systemParameters.din0 = CONTADOR;
			return;
		}
	}

	if ( strstr ( linea, "DIN1") != NULL ) {
		if ( strstr ( linea, "DIN1=NIVEL") != NULL ) {
			systemParameters.din1 = NIVEL;
			return;
		}
		if ( strstr ( linea, "DIN1=PULSO") != NULL ) {
			systemParameters.din1 = PULSO;
			return;
		}
		if ( strstr ( linea, "DIN1=CONTADOR") != NULL ) {
			systemParameters.din1 = CONTADOR;
			return;
		}
	}

	if ( strstr ( linea, "DIN2") != NULL ) {
		if ( strstr ( linea, "DIN2=NIVEL") != NULL ) {
			systemParameters.din2 = NIVEL;
			return;
		}
		if ( strstr ( linea, "DIN2=PULSO") != NULL ) {
			systemParameters.din2 = PULSO;
			return;
		}
		if ( strstr ( linea, "DIN2=CONTADOR") != NULL ) {
			systemParameters.din2 = CONTADOR;
			return;
		}
	}

	if ( strstr ( linea, "DIN3") != NULL ) {
		if ( strstr ( linea, "DIN3=NIVEL") != NULL ) {
			systemParameters.din3 = NIVEL;
			return;
		}
		if ( strstr ( linea, "DIN3=PULSO") != NULL ) {
			systemParameters.din3 = PULSO;
			return;
		}
		if ( strstr ( linea, "DIN3=CONTADOR") != NULL ) {
			systemParameters.din3 = CONTADOR;
			return;
		}
	}




	// Intervalo de poleo
	if ( strstr ( linea, "INTERVALO_POLEO=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.intervaloDePoleo=atoi(index);
		}
		return;
	}

	// Debug ??
	if ( strstr ( linea, "DEBUG=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.debug=atoi(index);
		}
		return;
	}

	/* Identificador del datalogger */
	if ( strstr ( linea, "DLGID=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			bzero(systemParameters.dlgId, strlen(systemParameters.dlgId));
			strncpy(systemParameters.dlgId, index, (largo));
		}
		return;
	}

	/* Direccion IP del servidor */
	if ( strstr ( linea, "IPADDRESS=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			bzero(systemParameters.ipaddress, strlen(systemParameters.ipaddress));
			strncpy(systemParameters.ipaddress, index, (largo));
		}
		return;
	}

	// TCP_PORT
	if ( strstr ( linea, "TCP_PORT=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.tcpPort=atoi(index);
		}
		return;
	}

	/* Si trabaja en modo demonio o consola */
	if ( strstr ( linea, "DAEMON=") != NULL ) {
		index = strchr(linea, '=') + 1;
		systemParameters.daemonMode = atoi(index);
		return;
	}

	/* Identificador de BD */
	if ( strstr ( linea, "BD=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			bzero(systemParameters.bd, strlen(systemParameters.bd));
			strncpy(systemParameters.bd, index, (largo));
		}
		return;
	}

	// Duracion del poleo en modo service
	if ( strstr ( linea, "SERVICE_POLL_TIME=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.servicePollTime=atoi(index);
		}
		return;
	}

	// Duracion del ciclo  de service
	if ( strstr ( linea, "SERVICE_TIME=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.serviceLastTime=atoi(index);
		}
		return;
	}

	// Cada cuanto salva el clock
	if ( strstr ( linea, "SAVECLOCK_TIME=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.saveClock=atoi(index);
		}
		return;
	}

	// Graba datos en archivo externo
	if ( strstr ( linea, "EXTERN_FILE=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.writeToFile = atoi(index);
		}
		return;
	}

	// Nro registros en el  archivo externo
	if ( strstr ( linea, "NRORECORDS_IN_EXFILE=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.nroRecsInExtFile = atoi(index);
		}
		return;
	}

	// Resync online??
	if ( strstr ( linea, "RESYNC_ONLINE=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			systemParameters.resyncOnline=atoi(index);
		}
		return;
	}

}
/*--------------------------------------------------------------------------*/

void conf_fnPrintConfParams(void)
{

	pthread_mutex_lock(&printf_mlock);
		F_syslog("  INFO::config: INTERVALO_POLEO=%d\n", systemParameters.intervaloDePoleo );
		if (systemParameters.din0 == PULSO ) {
			F_syslog("  INFO::config: DIN0=PULSO\n");
		} else if (systemParameters.din0 == NIVEL ) {
			F_syslog("  INFO::config: DIN0=NIVEL\n");
		} else if (systemParameters.din0 == CONTADOR ) {
			F_syslog("  INFO::config: DIN0=CONTADOR\n");
		}

		if (systemParameters.din1 == PULSO ) {
			F_syslog("  INFO::config: DIN1=PULSO\n");
		} else if (systemParameters.din1 == NIVEL ) {
			F_syslog("  INFO::config: DIN1=NIVEL\n");
		} else if (systemParameters.din1 == CONTADOR ) {
			F_syslog("  INFO::config: DIN1=CONTADOR\n");
		}

		if (systemParameters.din2 == PULSO ) {
			F_syslog("  INFO::config: DIN2=PULSO\n");
		} else if (systemParameters.din2 == NIVEL ) {
			F_syslog("  INFO::config: DIN2=NIVEL\n");
		} else if (systemParameters.din2 == CONTADOR ) {
			F_syslog("  INFO::config: DIN2=CONTADOR\n");
		}

		if (systemParameters.din3 == PULSO ) {
			F_syslog("  INFO::config: DIN3=PULSO\n");
		} else if (systemParameters.din3 == NIVEL ) {
			F_syslog("  INFO::config: DIN3=NIVEL\n");
		} else if (systemParameters.din3 == CONTADOR ) {
			F_syslog("  INFO::config: DIN3=CONTADOR\n");
		}



		F_syslog("  INFO::config: DEBUG=%d\n", systemParameters.debug );
		F_syslog("  INFO::config: DLGID=%s\n", systemParameters.dlgId );
		F_syslog("  INFO::config: BD=%s\n", systemParameters.bd );
		F_syslog("  INFO::config: IPADDRESS=%s\n", systemParameters.ipaddress );
		F_syslog("  INFO::config: TCP_PORT=%d\n", systemParameters.tcpPort );
		F_syslog("  INFO::config: DAEMON=%d\n", systemParameters.daemonMode );
		F_syslog("  INFO::config: SERVICE_POLL_TIME=%d\n", systemParameters.servicePollTime );
		F_syslog("  INFO::config: SERVICE_TIME=%d\n", systemParameters.serviceLastTime );
		F_syslog("  INFO::config: SAVECLOCK_TIME=%d\n", systemParameters.saveClock );
		F_syslog("  INFO::config: EXTERN_FILE=%d\n", systemParameters.writeToFile );
		F_syslog("  INFO::config: RECORDS_IN_EXFILE=%d\n", systemParameters.nroRecsInExtFile );
		F_syslog("  INFO::config: RESYNC_ONLINE=%d\n", systemParameters.resyncOnline );
		F_syslog("---------------------------------------");
		F_syslog("INFO::config: dlgLabjack running...");
	pthread_mutex_unlock(&printf_mlock);

}
/*--------------------------------------------------------------------------*/

