/*
 * dlg2012_labjack.c
 *
 *  Created on: 19/03/2012
 *      Author: pablo
 *
 *
 *  Descripcion:
 *  Esta thread se encarga de configurar las entradas del sistema y luego comienza un loop infinito
 *  en el cual lee todas las entradas y las deja en una base de datos SQLite..
 *
 *
 *------------------------------------------------------------------------------------------------------------------
 * 2013-Ago-04:
 * El nuevo formato va a ser:
 *
 *  HEADER:   DLGID,TIPO, FECHA, HORA                              >MDP001,%,130804,121130,
 *  ANALOG_A: AIN0, AIN1, AIN2, AIN3, AIN4, AIN5, AIN6, AIN7       200,201,202,203,204,205,206,207,
 *  ANALOG_B: AIN8,AIN9,AIN10,AIN11,AIN12,AIN13                    308,309,310,311,312,313
 *  COUNTERS: C0, C1                                               40,41
 *  DIGITAL:  DIN0,DIN1,DIN2,DIN3                                  1,0,34,1
 *  TAIL:     INT_POL                                              60<
 *
 * HEADER: ">MDP001,%,130804,121130,"
 *          -Pos 00: dlgId : MDP001
 *          -Pos 01: tipo  : %
 *          -Pos 02: fecha : 130804
 *          -Pos 03: hora  : 121130
 *
 * >SPY01,%,130806,121656,194,161,146,147,163,163,173,166,270,1,1,73,292,337,1,30,1,30,30<
 * >SPY01,%,130806,121801,174,144,140,156,179,174,169,167,1,1,88,334,327,286,0,0,1,30,1,30,30<
 * MDP001,%,130806,151821,244,116,26,10,259,747,4,343,14,202,4,25,0,0,1,1,1,1,60<
 *
 *
 * DATOS ANALOGICOS:>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * Son los siguientes 8 campos.
 * Se utilizan los canales FIO0..FIO7 ( terminal blocks) de la Labjack.
 * La asignacion es:
 *  FIO0..AIN0...ch0
 *  FIO1..AIN1...ch1
 *  FIO2..AIN2...ch2
 *  FIO3..AIN3...ch3
 *  FIO4..AIN4...ch4
 *  FIO5..AIN5...ch5
 *  FIO6..AIN6...ch6
 *  FIO7..AIN7...ch7
 *
 *       Frame  | id    | Lpj id | Lbj pos |
 *       -------|-------|--------|----------
 *		-Pos 04:  AIN0 ->  FIO0:    TB00
 * 		-Pos 05:  AIN1 ->  FIO1:    TB01
 * 		-Pos 06:  AIN2 ->  FIO2:    TB02
 * 		-Pos 07:  AIN3 ->  FIO3:    TB03
 * 		-Pos 08:  AIN4 ->  FIO4:    TB04
 * 		-Pos 09:  AIN5 ->  FIO5:    TB05
 * 		-Pos 10:  AIN6 ->  FIO6:    TB06
 * 		-Pos 11:  AIN7 ->  FIO7:    TB07
 *
 * ENTRADAS ANALOGICAS EXTRA:>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * Son los siguientes 6 campos.
 * Estan definidas en el conector DB15.
 * Estas son:
 *  EIO2..AIN8....ch8  -> (DB15-5)
 *  EIO3..AIN9....ch9  -> (DB15-13)
 *  EIO4..AIN10...ch10 -> (DB15-6)
 *  EIO5..AIN11...ch11 -> (DB15-14)
 *  EIO6..AIN12...ch12 -> (DB15-7)
 *  EIO7..AIN13...ch13 -> (DB15-15)
 *
 *       Frame  | id    | Lpj id | Lbj pos
 *       -------|-------|--------|-------------
 * 		-Pos 12:  AIN8 ->  EIO2:   (DB15-5)
 * 		-Pos 13:  AIN9 ->  EIO3:   (DB15-13)
 * 		-Pos 14:  AIN10 -> EIO4:   (DB15-6)
 * 		-Pos 15:  AIN11 -> EIO5:   (DB15-14)
 * 		-Pos 16:  AIN12 -> EIO6:   (DB15-7)
 * 		-Pos 17:  AIN13 -> EIO7:   (DB15-15)
 *
 *
 * CONTADORES:>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * Siguientes 2 campos.
 * Definidos en el conector DB15.
 *
 *       Frame  | id    | Lpj id | Lbj pos
 *       -------|-------|--------|-------------
 * 		-Pos 18:  CNT0 ->  EIO0:   (DB15-4)
 * 		-Pos 19:  CNT1 ->  EIO1:   (DB15-12)
 *
 *
 * ENTRADAS DIGITALES:>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * Siguientes 4 campos.
 * Definidos en el conector DB15.
 * Pueden ser por nivel ( reportamos solo 0 o 1 ) o pulsadas en las cuales
 * contemos el tiempo que estan en 1, medido con granularidad de 1s. Esto es para el caso de UTE por ej. en que
 * quiere saber cuanto tiempo(seg) esta c/bomba prendida.
 * Podemos configurarlos como todos digitales por nivel, o todos digitales pulsados o una mezcla de ambos. En
 * el archivo de configuracion debemos indicar cuales son por nivel y cuales pulsados.
 * Por defecto se toman por nivel.
 * Los digitales por nivel se muestrean el ultimo segundo del intervalo de poleo.
 * Los canales que usamos son los que trae la Labjack especificos en el conector DB15 para esto: CIO0..CIO3
 *  (offset digital: 16)
 *  DIN0..CIO0 (DB15-9)
 *  DIN1..CIO1 (DB15-2)
 *  DIN2..CIO2 (DB15-10)
 *  DIN3..CIO3 (DB15-3)
 *
 *       Frame  | id    | Lpj id | Lbj pos
 *       -------|-------|--------|-------------
 * 		-Pos 20: DIN0 ->  CIO0       (DB15-9)
 * 		-Pos 21: DIN1 ->  CIO1       (DB15-2)
 * 		-Pos 22: DIN2 ->  CIO2       (DB15-10)
 * 		-Pos 23: DIN3 ->  CIO3       (DB15-3)
 *
 *
 * TAIL:
 * Agregamos el intervalo de poleo como ultima variable "60<"
 *
 * REGLETA DE LABJACK:
 * <-------------------- analogicos------------->
 * TB00,TB01,TB02,TB03,TB04,TB05,TB06,TB07
 *                <------------analogicos extra-------------->
 *                DB15-5,DB15-13,DB15-6,DB15-14,DB15-7,DB15-15
 *                                                      <--contadores-->
 *                                                       DB15-4,DB15-12,
 *                                                                   <-- digitales (nivel/pulso)--->
 *                                                                   DB15-9, DB15-2, DB15-10, DB15-3
 *
 *------------------------------------------------------------------------------------------------------------------
 *  2012-Nov-30:
 *
 *  Señales:
 *  El terminal block tiene 8 lineas, FIO (flexible IO).
 *  Estas por default estan configuradas para canales analogicos del A0..A7 y
 *  representan los canales 0..7.
 *  El DB15 tiene 15 lineas, EIO (extended IO) que pueden ser Ain, Dio, etc.
 *  Las lineas EIO0..EIO7 por defecto son analogicas y corresponden a los canales 8..15.
 *  Tambien tiene las lineas CIO0..CIO3 que corresponden a IO digitales.
 *
 *  La configuracion de lineas que usamos es:
 *  A) 8 entradas analogicas.(terminal block)
 *  -----------------------------------------
 *  FIO0..AIN0...ch0
 *  FIO1..AIN1...ch1
 *  FIO2..AIN2...ch2
 *  FIO3..AIN3...ch3
 *  FIO4..AIN4...ch4
 *  FIO5..AIN5...ch5
 *  FIO6..AIN6...ch6
 *  FIO7..AIN7...ch7
 *
 *  B) 2 contadores.
 *  ----------------
 *  Su posicion se fija a partir del pin FIO0 + offset ( debe estar entre 4 y 8 ) por lo tanto
 *  el primer pin puede ir desde EIO0 al EIO4.
 *  El posicionamiento es: timer0/timer1/counter0/counter1.
 *  Nosotros no vamos a usar los timers por lo que perderiamos 2 lineas.
 *  Con un offset de 8, la correspondencia que queda es:
 *  timer0->EIO0 (DB15-4)
 *  timer1->EIO1 (DB15-12)
 *  counter0->EIO2 (DB15-5)
 *  counter1->EIO3 (DB15-13)
 *
 *  C) 4 Entradas digitales (por nivel)
 *  -----------------------------------
 *  Los canales digitales se numeran del siguiente modo:
 *  0-7 FIO0..FIO7 ( no los usamos ya que estos canales quedan como analogicos)
 *  8-15 EIO0..EIO7
 *  16-19  CIOO..CIO3
 *
 *  Nosotros para estas entradas digitales vamos a usar las señales CIO0..CIO3
 *  offset digital: 16
 *  din0..CIO0 (DB15-9)
 *  din1..CIO1 (DB15-2)
 *  din2..CIO2 (DB15-10)
 *  din3..CIO3 (DB15-3)
 *
 *  D) 4 Entradas digitales muestreadas c/1 segundo.
 *  Estas entradas las muestreamos c/1 segundo y contamos las veces que leemos un 1.
 *  Sirven por ej. para conectar a las bombas de UTE.
 *  Las unicas que nos quedan son las EIO4..EIO7. Corresponden a los canales digitales
 *  12..15.
 *  cDin0..EIO4..dCh12 (DB15-6)
 *  cDin1..EIO5..dCh13 (DB15-14)
 *  cDin2..EIO6..dCh14 (DB15-7)
 *  cDin3..EIO7..dCh15 (DB15-15)
 *
 *
 *  ================================================================================================
 */
#include "dlgLabjackV1.h"

void labJackDetect(void);
void configCounters(void);
void insertFrameHeader(void);
void readAnalogChannels(void);
void readExtraAnalogChannels(void);
void readCountersChannels(void);
void readDigitalChannels(void);
void insertFrameTail(void);
int resetUsbLbj(void);
void initLabJack(void);

HANDLE hDevice;
u3CalibrationInfo caliInfo;
int localID;

int configIO;
long error;

char logBuffer[MAXLOGBUFFER];
char frameBuffer[MAXTXBUFFER];
int bufPos = 0;

int labjackStatusFlag;

/*---------------------------------------------------------------------------*/

void *lbj_thService(void *arg)
{
/*
 * Thread que atiende las funciones de la tarjeta labjack
 * Polea las entradas analogicas cada systemParameters.intervaloDePoleo y deja los datos
 * en una base de datos SQLite.
 *
 */

char *stmt;
int res;

	// Inicializo el sistema de captura de datos.
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::lbj_thService: Iniciando hilo Labjack...");
	F_syslog("%s", logBuffer);

	//Open first found U3 over USB
	initLabJack();

	while(1) {

		bzero(&frameBuffer, sizeof(frameBuffer));
		bufPos = 0;

		insertFrameHeader();			//  >DL01,%,60,18/06/2012,12:43:13,
		readAnalogChannels();			//  200,201,202,203,204,205,206,207,
		readExtraAnalogChannels();		//  308,309,310,311,312,313
		readCountersChannels();			//  0,0,

		// En la lectura de los canales digitales se genera el delay del ciclo
		readDigitalChannels();			//  (1,0,1,1) o (1,0,34,27)
		insertFrameTail();				//  60<

		if (systemVars.serviceMode > 0) {
			// En serviceMode no inserto en la base, solo lo logueo pero no inserto
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "SERVICE: %s", frameBuffer);
			F_syslog("%s", logBuffer);
		} else {

			// Inserto el frame a trasmitir en la SQLite.
			bzero(logBuffer, sizeof(logBuffer));
			stmt =  sqlite3_mprintf("insert into tbl1 (frame) values ('%q')", frameBuffer);
			if ( SQLexecInsert(stmt) == SQLITE_OK ) {
				// Log & info.
				sprintf(logBuffer, "INFO::lbj_thService: sql insert ok %s", frameBuffer);
			} else {
				sprintf(logBuffer, "ERROR::lbj_thService: sql insert FAIL %s", frameBuffer);
			}
			F_syslog("%s", logBuffer);

			// Tambien debo dejar los datos en un archivo ?
			if (systemParameters.writeToFile == 1) {
				writeToFile( &frameBuffer );
			}
		}

		configIO=0;

		// Problemas con la labjack.
		while (labjackStatusFlag != OK) {
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROR::lbj_thService: LABJACK FAIL");
			F_syslog("%s", logBuffer);
			sleep(60);
			// Reseteo el canal USB de la labjack
			res = resetUsbLbj();
			if ( res == OK ) {
				initLabJack();
			} else {
				sleep(60);
			}

		}
	}

}
/*---------------------------------------------------------------------------*/

void initLabJack(void)
{

	localID = -1;

	if( (hDevice = openUSBConnection(localID)) == NULL ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::initLabJack: No detecte ningun dispositivo U3.");
		F_syslog("%s", logBuffer);
		abort();
	}

	/* Get calibration information from U3.
	 * Esta funcion debe ser llamada previamente a leer los valores
	 * de la tarjeta.
	 */
	if( getCalibrationInfo(hDevice, &caliInfo) < 0 ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::initLabJack: Error al leer la calibracion.");
		F_syslog("%s", logBuffer);
		abort();
	}

	// Debo configurar los 2 canales para contadores.
	configCounters();

    /* Loop de lectura de datos.
	 * La primera vez que los leemos debemos pasar en el parametro ConfigIO un 1 para
	 * que queden seteados como entradas analogicas.
	 * Luego pasamos un 0.
	 */
	configIO=1;
	sleep(1);
	labjackStatusFlag = OK;

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::initLabJack: Init Labjack card OK");
	F_syslog("%s", logBuffer);

}

/*---------------------------------------------------------------------------*/

int resetUsbLbj(void)
{
	// Reseteo el canal de la Labjack usando el comando resetusb.

FILE *fp;
char line[128];			/* line of data from unix command*/
char *token;
char parseDelimiters[] = " :";
int i,fieldPos;
char *rxFields[30];
char cmdName[64];

	// Detecto el canal USB de la tarjeta

		fp = popen("lsusb | grep LabJack", "r");		/* Issue the command.		*/
		/* Read a line			*/
		if ( fgets( line, sizeof line, fp) > 0 )
		{
			/* Reservo la memoria para los campos */
			for (i=0; i<30; i++) {
				rxFields[i] = malloc(31*sizeof(char));
			}

			// Parseo todo el frame en sus campos para hallar el canal y el bus
			fieldPos = 0;
			token = strtok(line, parseDelimiters);
			rxFields[fieldPos] = token;
			fieldPos++;

			while ( (token = strtok(NULL, parseDelimiters)) != NULL ) {
				rxFields[fieldPos] = token;
				fieldPos++;
			}

			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "INFO::resetUsbLbj: RESET /dev/bus/usb/%s/%s",rxFields[1], rxFields[3] );
			F_syslog("%s", logBuffer);

			// Genero el comando para resetear el usb.
			bzero(cmdName, sizeof(cmdName));
			sprintf(cmdName, "resetusb /dev/bus/usb/%s/%s",rxFields[1], rxFields[3] );
			printf("INFO::resetUsbLbj: usbDev: %s\n", cmdName );

		  } else {
				bzero(logBuffer, sizeof(logBuffer));
				sprintf(logBuffer, "ERROR::resetUsbLbj: LabJack no detectada");
				F_syslog("%s", logBuffer);
				abort();
		  }

		  pclose(fp);

		  // Reseteo la tarjeta

		  fp = popen(&cmdName, "r");
		  if (fp == NULL) {
				bzero(logBuffer, sizeof(logBuffer));
				sprintf(logBuffer, "ERROR::resetUsbLbj: NO PUEDO RESETEAR EL USB DE LABJACK" );
				F_syslog("%s", logBuffer);
				abort();

		  }
		  // Doy tiempo al sistema.
		  sleep(30);
		  return(OK);
}

/*---------------------------------------------------------------------------*/

void labJackDetect(void)
{

int numU3s=0;
int lbjDetected = FALSE;

	while ( !lbjDetected ) {
		// Determino cuantas tarjetas tengo conectadas.
		numU3s = LJUSB_GetDevCount(U3_PRODUCT_ID);
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "INFO::labJackDetect: Tarjetas Labjack U3s conectadas=%d", numU3s);
		F_syslog("%s", logBuffer);
		if (numU3s < 1) {
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROR::labJackDetect: No se detectan tarjetas Labjack conectadas.");
			F_syslog("%s", logBuffer);
			sleep(60);
			//abort();
		} else {
			lbjDetected = TRUE;
		}
	}
}

/*---------------------------------------------------------------------------*/

void configCounters(void)
{
// Solo configuro los 2 contadores.
// El esquema es: TIMER0, TIMER1, CNT0, CNT1 a partir del offset.
// Como no configuro los TIMERS, a partir del offset queda CNT0, CNT1.
//
long enableTimers[2] = {0, 0};  						//Disable Timer0-Timer1
long enableCounters[2] = {1, 1};  						//Enable Counter0-Counter1
long TimerModes[2] = {LJ_tmFIRMCOUNTERDEBOUNCE, LJ_tmFIRMCOUNTERDEBOUNCE};  	//Set timer modes
double TimerValues[2] = {0, 0};                        //Set PWM8 duty-cycles to 75%

	/* Configuro los timers (no me interesan) y los contadores.
	   Sobre todo es importante la posicion que ocupan.
	   TCPINOFFSET=8 por lo tanto el mapeo queda
	   counter0->EIO0 (DB15-4)
	   counter1->EIO1 (DB15-12)
	*/
    if( (error = eTCConfig(hDevice, enableTimers, enableCounters, TCPINOFFSET, LJ_tc48MHZ, 0, TimerModes, TimerValues, 0, 0)) != 0 ) {
    	bzero(logBuffer, sizeof(logBuffer));
    	sprintf(logBuffer, "ERROR::configCounters: No puedo configurar los timers/counters.");
		F_syslog("%s", logBuffer);
		abort();
	}

}

/*---------------------------------------------------------------------------*/

void insertFrameHeader(void)
{

struct tm *Sys_T = NULL;
time_t Tval = 0;

	Tval = time(NULL);
	Sys_T = localtime(&Tval);

	// Header id: >DL01,%,
    bufPos += sprintf(frameBuffer + bufPos, ">%s,%c,", systemParameters.dlgId, '%');
    // Header date, time: 120626,084934,
    bufPos += sprintf(frameBuffer + bufPos, "%02d%02d%02d,%02d%02d%02d,", (Sys_T->tm_year - 100), (Sys_T->tm_mon + 1), Sys_T->tm_mday , Sys_T->tm_hour, Sys_T->tm_min, Sys_T->tm_sec);

}

/*---------------------------------------------------------------------------*/

void readAnalogChannels(void)
{
	/* Canales Analogicos Fijos (8 canales)
       Los canales van del 0 al 7 por lo tanto el mapeo es:
       FIO0..AIN0...ch0
       FIO1..AIN1...ch1
       FIO2..AIN2...ch2
       FIO3..AIN3...ch3
       FIO4..AIN4...ch4
       FIO5..AIN5...ch5
       FIO6..AIN6...ch6
       FIO7..AIN7...ch7
    */

int channel;
double dblVoltage;
int dblCounts1023;
long DAC1Enable;


	for (channel=0; channel < NROCANALESANALOGICOS; channel++) {
		if( (error = eAIN(hDevice, &caliInfo, configIO, &DAC1Enable, channel, 31, &dblVoltage, 0, 0, 0, 0, 0, 0)) != 0 )  {
		   	labjackStatusFlag = ERR;
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROR::readAnalogChannels: Error al leer eAIN %d", channel);
			F_syslog("%s", logBuffer);
			return;
		}

		// Valor analogico en voltios [0..2,4V]
		//bufPos += sprintf(frameBuffer + bufPos, "%.3f,",dblVoltage);

		// Valor analogico en count 10 bits [0..1023]
		dblCounts1023 = (int) (dblVoltage * 1023 / 2.4);
		bufPos += sprintf(frameBuffer + bufPos, "%d,",dblCounts1023);
	}
}

/*---------------------------------------------------------------------------*/

void readExtraAnalogChannels(void)
{
// Ver asignacion de canales en Labjack U3 user guide, 2.6.1 - Channel Numbers
// 	Positive Channel #
//	0-7	    AIN0-AIN7 (FIO0-FIO7)
//	8-15	AIN8-AIN15 (EIO0-EIO7)
//	30	    Temp Sensor
//	31	    Vreg
//
// Como definimos los canales analogicos extra a partir de EI02, el offset es 10.
//

int nroCanalLbj, i;
double dblVoltage;
int dblCounts1023;
long DAC1Enable;


	for (i=0; i<6; i++) {
		nroCanalLbj = EXTRAANALOGCHBASE + i;
		if( (error = eAIN(hDevice, &caliInfo, configIO, &DAC1Enable, nroCanalLbj, 31, &dblVoltage, 0, 0, 0, 0, 0, 0)) != 0 )  {
		   	labjackStatusFlag = ERR;
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "ERROR::readExtraAnalogChannels: Error al leer eAIN %d", nroCanalLbj);
			F_syslog("%s", logBuffer);
			return;
		}

		// Valor analogico en voltios [0..2,4V]
		//bufPos += sprintf(frameBuffer + bufPos, "%.3f,",dblVoltage);

		// Valor analogico en count 10 bits [0..1023]
		dblCounts1023 = (int) (dblVoltage * 1023 / 2.4);
		bufPos += sprintf(frameBuffer + bufPos, "%d,",dblCounts1023);

	}
}

/*---------------------------------------------------------------------------*/

void readCountersChannels(void)
{
long aReadTimers[2] = {0, 0};         					//Read Timer0|1: No tengo definidos asi que no los leo.
long aUpdateResetTimers[2] = {0, 0};  					//Update/reset timer0|1: Idem
long aReadCounters[2] = {1, 1};       					//Read Counter0|1
long aResetCounters[2] = {1, 1};      					//Reset Counter0|1
double aTimerValues[2] = {0, 0};
double aCounterValues[2] = {0, 0};

	/* Contadores (2).
	   counter0->EIO0 (DB15-4)
       counter1->EIO1 (DB15-12)
	 */

	if( (error = eTCValues(hDevice, aReadTimers, aUpdateResetTimers, aReadCounters, aResetCounters, aTimerValues, aCounterValues, 0, 0)) != 0 ) {
	   	labjackStatusFlag = ERR;
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::readCountersChannels: Error al leer Counters");
		F_syslog("%s", logBuffer);
		return;
		bufPos += sprintf(frameBuffer + bufPos, "-1,-1,");
    } else {
    	bufPos += sprintf(frameBuffer + bufPos, "%.0f,%.0f,", aCounterValues[0], aCounterValues[1]);
    }
}

/*---------------------------------------------------------------------------*/

void readDigitalChannels(void)
{
/*
 * Ver asignacion de canales en Labjack U3 user guide, 22.11 - DB15
 *  the CIO are addressed as bits 16-19.
 *  0-7    FIO0-FIO7
 *  8-15   EIO0-EIO7
 *  16-19  CIO0-CIO3
 *
 * Digitales.(4 canales)
 *     offset digital (DINCHBASE) : 16
 *     din0..CIO0 (DB15-9)
 *     din1..CIO1 (DB15-2)
 *     din2..CIO2 (DB15-10)
 *     din3..CIO3 (DB15-3)
 *
 * Como pueden estar configurados en modo nivel o en modo pulsado, lo que hago es leerlos todos
 * en modo pulsado, y al finalizar el ciclo leo su nivel.
 * Al armar el frame es que resulevo cuales van por nivel (primero) y cuales pulsados ( a continuacion)
 *
 * Como los leo c/1s, en esta funcion se genera el delay del proceso.
 *
 * Estos canales pueden estar como contadores asi que debo llevar la cuenta de los flancos que aparecen
 * Son contadores lentos ya que por firmware tienen un debounde de 1Hz.
 */

int L_pulseCount[4], L_digitalLevel[4], L_flancosCount[4], L_states[4];
int loopSecs ,din, error;
int nroCanalLbj;
long dinState;

	// Calculo el delay del ciclo
	if (systemVars.serviceMode > 0 ) {
		// Modo service: poleo c/5 secs.
		loopSecs = systemParameters.servicePollTime;
	} else {
		loopSecs = systemParameters.intervaloDePoleo;
	}

	// inicializo.
	for (din=0; din<4; din++) {
		L_pulseCount[din] = 0;
		L_digitalLevel[din] = 0;
		L_flancosCount[din] = 0;
	}

	// Genero el ciclo.
	while(loopSecs-- > 0) {
		// Genero el delay de 1s.
		sleep(1);
		// Leo los 4 canales.
		// En un array guardo los segundos acumulados en 1.
		// En otro el valor instantaneo (0 o 1).
		for (din=0; din<4; din++) {
			nroCanalLbj = DINCHBASE + din;
			if( (error = eDI(hDevice, configIO, nroCanalLbj, &dinState)) != 0 )  {
				// Error de lectura
			   	labjackStatusFlag = ERR;
				bzero(logBuffer, sizeof(logBuffer));
				sprintf(logBuffer, "ERROR::readDigitalChannels: Error al leer DIN %d", nroCanalLbj);
				F_syslog("%s", logBuffer);
				L_pulseCount[din] = -1;
				return;
			} else {
				// Acumulo segundos en nivel 1
				L_pulseCount[din] += dinState;
				// Guardo el nivel.
				L_digitalLevel[din] = dinState;
				if ( L_states[din] != dinState  ) {	// Aparecio un flanco
					// Si es de bajada lo almaceno
					if ( ( L_states[din] == 1) && (  dinState == 0) ) {
						L_flancosCount[din]++;			// Incremento el contador
					}
					// Guardo el estado
					L_states[din] = dinState;		// Actualizo el estado para la proxima vuelta
				}
			}
		}
	}

	// Termino el loop de delay. Paso datos al buffer
	// DIN0:
	if ( systemParameters.din0 == NIVEL ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,", L_digitalLevel[0] );
	} else if ( systemParameters.din0 == PULSO ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,", L_pulseCount[0] );
	} else if ( systemParameters.din0 == CONTADOR ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,",  L_flancosCount[0] );
	}

	// DIN1:
	if ( systemParameters.din1 == NIVEL ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,", L_digitalLevel[1] );
	} else if ( systemParameters.din1 == PULSO ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,", L_pulseCount[1] );
	} else if ( systemParameters.din1 == CONTADOR ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,",  L_flancosCount[1] );
	}

	// DIN2:
	if ( systemParameters.din2 == NIVEL ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,", L_digitalLevel[2] );
	} else if ( systemParameters.din2 == PULSO ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,", L_pulseCount[2] );
	} else if ( systemParameters.din2 == CONTADOR ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,",  L_flancosCount[2] );
	}

	// DIN3:
	if ( systemParameters.din3 == NIVEL ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,", L_digitalLevel[3] );
	} else if ( systemParameters.din3 == PULSO ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,", L_pulseCount[3] );
	} else if ( systemParameters.din3 == CONTADOR ) {
		bufPos += sprintf(frameBuffer + bufPos, "%d,",  L_flancosCount[3] );
	}


}

/*---------------------------------------------------------------------------*/

void insertFrameTail(void)
{
	// Tail
    bufPos += sprintf(frameBuffer + bufPos, "%d", systemParameters.intervaloDePoleo);
	sprintf(frameBuffer + bufPos, "<\n");

}

/*---------------------------------------------------------------------------*/
