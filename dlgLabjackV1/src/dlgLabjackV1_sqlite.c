/*
 * dlg2012_sqlite.c
 *
 *  Created on: 29/03/2012
 *      Author: pablo
 *
 *  Descripcion:
 *  Modulo donde se implementan todas las funciones de acceso a la base de datos
 *  sqlite.
 *  Los frames que son relevados se almacenan en esta base y para trasmitir
 *  se leen de la misma.
 *  De este modo se logra un interface mas clara y una simplificacion del manejo
 *  offline de la informacion.
 *
 *  El prototipo de instrucciones que usamos es:
 *  	create table tbl1 (id integer primary key, frame varchar(128));
 *  	create table tbl2 (id integer primary key, storedDate bigint);
 *  	create table tbl3 (id integer primary key, filename varchar(32));
 *  	select * from sqlite_master;
 *  	( primer elemento): select * from tbl1 order by id limit 1;
 *  	( ultimo elemento): select * from tbl1 order by id desc limit 1;
 *  	insert into tbl1 ( NULL, 'string');
 *  	delete from tbl1 where id=NN;
 *  	vacuum;
 *
 *	root@omap:~/bin# ./sqlite3 dlg.db
 *	SQLite version 3.7.11 2012-03-20 11:35:50
 *	Enter ".help" for instructions
 *	Enter SQL statements terminated with a ";"
 *	sqlite> select * from sqlite_master;
 *	table|tbl1|tbl1|2|CREATE TABLE tbl1 (id integer primary key, frame varchar(128))
 *	table|tbl2|tbl2|3|CREATE TABLE tbl2 (id integer primary key, storedDate bigint)
 *	table|tbl3|tbl3|4|CREATE TABLE tbl3 (id integer primary key, filename varchar(32))
 *	sqlite>
 *
 */
#include "dlgLabjackV1.h"

char logBuffer[MAXLOGBUFFER];

/*---------------------------------------------------------------------------*/

void db_open(void) {

int ret;
char create_tbl[100];
char *filename;

	filename = "/var/log/dlg.db";
	//ret = sqlite3_open("./dlg.db", &db);
	//ret = sqlite3_open("/root/bin/dlg.db", &db);
	//sqlite3_temp_directory = sqlite3_mprintf("%s", zPathBuf);
	//sqlite3_temp_directory = sqlite3_mprintf("/var/log/");
	//ret = sqlite3_open("dlg.db", &db);
	ret = sqlite3_open(filename, &db);
	//ret = sqlite3_open(systemParameters.bd, &db);
	if(ret != SQLITE_OK) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::db_open: No puedo abrir la base de datos %s: Abort", systemParameters.bd);
		F_syslog("%s", logBuffer);

		abort();

	} else {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "INFO::db_open: Base de datos %s abierta.", systemParameters.bd);
		F_syslog("%s", logBuffer);
	}

	strcpy(create_tbl, "CREATE TABLE IF NOT EXISTS tbl1 (id integer primary key, frame varchar(128))");
	ret = sqlite3_exec( db,create_tbl,0,0,0);
	if(ret != SQLITE_OK) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::db_open: No pude crear tabla tbl1 %s: Abort", systemParameters.bd);
		F_syslog("%s", logBuffer);
		exit(0);
	}

	strcpy(create_tbl, "CREATE TABLE IF NOT EXISTS tbl2 (id integer primary key, storedDate bigint)");
	ret = sqlite3_exec( db,create_tbl,0,0,0);
	if(ret != SQLITE_OK) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::db_open: No pude crear tabla tbl2 %s: Abort", systemParameters.bd);
		F_syslog("%s", logBuffer);
		exit(0);
	}

	strcpy(create_tbl, "CREATE TABLE IF NOT EXISTS tbl3 (id integer primary key, filename varchar(32))");
	ret = sqlite3_exec( db,create_tbl,0,0,0);
	if(ret != SQLITE_OK) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::db_open: No pude crear tabla tbl3 %s: Abort", systemParameters.bd);
		F_syslog("%s", logBuffer);
		exit(0);
	}

	// Cada vez que abro la base la reordeno.
	sqlite3_exec(db, "VACUUM;", NULL, NULL, NULL);


}
/*---------------------------------------------------------------------------*/

int select_callback(void* p_data, int ncols, char** values, char** headers)
{
// La funcion de callback es invocada solo si el select retorno algun valor.
// En este caso ponemos la flag de callback.

sqlStruct *retData = (void*)p_data;

	// El select solo devuelve un registro.
	retData->callbackStatus = CBK_CALLED;				// callback invocada
	retData->id = atoi(values[0]);						// Nro.de registro que devolvio
	memcpy(retData->frame, values[1], MAXTXBUFFER);		// Frame
	return 0;
}

/*---------------------------------------------------------------------------*/

int SQLexecDelete(char *sql)
{
	// Ejecuto la stmt pasada en el argumento que es de delete.

	char *errmsg;
	int   ret;

	pthread_mutex_lock(&sql_mlock);
		ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	pthread_mutex_unlock(&sql_mlock);

	if(ret != SQLITE_OK) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::SQLexecDelete: delete %s [%s]", sql, errmsg);
		F_syslog("%s", logBuffer);
	}

	return(ret);

}

/*---------------------------------------------------------------------------*/

int SQLexecInsert(char *sql)
{

	char *errmsg;
	int   ret;

	pthread_mutex_lock(&sql_mlock);
		ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	pthread_mutex_unlock(&sql_mlock);

	if(ret != SQLITE_OK) {
	   	bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::SQLexecInsert: insert %s [%s]", sql, errmsg);
		F_syslog("%s", logBuffer);
	}

	return(ret);

}

/*---------------------------------------------------------------------------*/

int SQLexecSelect(char *sql, sqlStruct* readData)
{

	char *errmsg;
	int   ret;

	// Leo un solo registro.
	readData->callbackStatus = CBK_NOTCALLED;
	pthread_mutex_lock(&sql_mlock);
		ret = sqlite3_exec(db, sql, select_callback, readData, &errmsg);
	pthread_mutex_unlock(&sql_mlock);

	// Errores ??
	if(ret != SQLITE_OK) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::SQLexecSelect: read %s [%s]", sql, errmsg);
		F_syslog("%s", logBuffer);
	}

    return(ret);

}

/*---------------------------------------------------------------------------*/
