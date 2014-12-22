/*
 * dlg2012sbc_datafile.c
 *
 *  Created on: 24/08/2012
 *      Author: root
 */

/*---------------------------------------------------------------------------*/

#include "dlgLabjackV1.h"

char logBuffer[MAXLOGBUFFER];
char fullFileName[48];
char partFileName[48];

/*---------------------------------------------------------------------------*/
void cleanUpFiles(void)
{

char cmdLine[64];

	bzero(cmdLine, sizeof(cmdLine));
//	sprintf(cmdLine , "for i in /root/datFile/*.part; do mv \"$i\" \"${i/.part}\"; done;");
	sprintf(cmdLine , "rename 's/\.part//' /root/datFile/*.part");

	//system (cmdLine);
	bzero(logBuffer, sizeof(logBuffer));
	if ( system (cmdLine) == 0 ) {
		sprintf(logBuffer, "DEBUG::cleanUpFiles: OK");
	} else {
		sprintf(logBuffer, "DEBUG::cleanUpFiles: ERROR");
	}
	F_syslog("%s", logBuffer);


	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "DEBUG::cleanUpFiles: %s", &cmdLine);
	F_syslog("%s", logBuffer);



}
/*---------------------------------------------------------------------------*/

void dataFile_open(void)
{

char fileName[8];
char *stmt;
sqlStruct readData;
int nreg, sqlRet;
int fileCount;

	// Leo de la BD el nombre del archivo a abrir.
	stmt =  sqlite3_mprintf("select * from tbl3 order by id desc limit 1");
	sqlRet =  SQLexecSelect(stmt, &readData);
	nreg = readData.id;
	if ( nreg  > 0 ) {
		strncpy( fileName, readData.frame, sizeof(fileName) );
	} else {
		// Error de lectura o no hay fechas guardadas (inicio)
		bzero(logBuffer, sizeof(logBuffer));
    	sprintf(logBuffer, "INFO::dataFile_open: BD(tbl3) SIN DATOS. Inicializo");
    	F_syslog("%s", logBuffer);
    	// Inicializo el nombre de archivo
		strncpy( fileName, "000001", 6 );
	}

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "DEBUG::dataFile_open: FileName=%s", &fileName);
	F_syslog("%s", logBuffer);

	// Abro el archivo
	sprintf(partFileName, "/root/datFile/%s.txt.part", &fileName);
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "DEBUG::dataFile_open: partFileName=%s", &partFileName);
	F_syslog("%s", logBuffer);

	fileFD = fopen(&partFileName, "a");
	if ( fileFD == NULL ) {
		bzero(logBuffer, sizeof(logBuffer));
    	sprintf(logBuffer, "ERROR::dataFile_open: No puedo abrir archivo.");
    	F_syslog("%s", logBuffer);
 		return;
	}

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "DEBUG::dataFile_open: File %s abierto.", &partFileName );
	F_syslog("%s", logBuffer);

	sprintf(fullFileName, "/root/datFile/%s.txt", &fileName);
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "DEBUG::dataFile_open: fullFileName=%s", &fullFileName);
	F_syslog("%s", logBuffer);

	// Genero el siguiente nombre de archivo y lo guardo en la BD.
	fileCount = atoi(fileName) + 1;
	bzero(fileName, sizeof(fileName));
	sprintf(fileName, "%06d", fileCount);
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "DEBUG::dataFile_open: NextFileName=%s", fileName);
	F_syslog("%s", logBuffer);

	// Borro los nombres viejos
	stmt =  sqlite3_mprintf("delete from  tbl3");
	if ( SQLexecDelete(stmt) != SQLITE_OK ) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::dataFile_open: delete filename %s", fileName );
		F_syslog("%s", logBuffer);
		return;
	}

	// Actualizo la BD con el nombre nuevo.
	bzero(logBuffer, sizeof(logBuffer));
	stmt =  sqlite3_mprintf("insert into tbl3 (fileName) values ('%q')", fileName );
	if ( SQLexecInsert(stmt) != SQLITE_OK ) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::dataFile_open: insert newFilename %s", fileName );
		F_syslog("%s", logBuffer);
		return;
	}

}
/*---------------------------------------------------------------------------*/

void writeToFile( char *frameBuffer )
{

static framesWrited = 0;

	// Salgo si no esta habilitada la escritura en archivos.
	if (systemParameters.writeToFile == 0 )
		return;

	// Abro el archivo si no lo tengo abierto.
	if ( fileFD == NULL ) {
		dataFile_open();
		//  Si no pude abrirlo .
		if ( fileFD == NULL ) {
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROR::writeToFile: file closed.");
			F_syslog("%s", logBuffer);
			return;
		}
	}

	//Grabo la linea en el archivo
	fprintf(fileFD, "%s", frameBuffer );
	fflush(fileFD);
	// Si ya tengo todos los frames lo cierro
	framesWrited++;
	if ( framesWrited == systemParameters.nroRecsInExtFile ) {
		framesWrited = 0;
		F_syslog("DEBUG:: Cierro archivo");
		fclose(fileFD);
		fileFD = NULL;
		// y lo renombro.
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "DEBUG::writeToFile: renaming partFileName=%s", &partFileName);
		F_syslog("%s", logBuffer);
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "DEBUG::writeToFile: renaming to fullFileName=%s", &fullFileName);
		F_syslog("%s", logBuffer);

		bzero(logBuffer, sizeof(logBuffer));
		if ( rename (partFileName, fullFileName) == 0 ) {
			sprintf(logBuffer, "DEBUG::writeToFile: renaming OK");
		} else {
			sprintf(logBuffer, "DEBUG::writeToFile: renaming ERROR");
		}
		F_syslog("%s", logBuffer);

	}

}

/*---------------------------------------------------------------------------*/
