/*
 ============================================================================
 Name        : dlg2012.c
 Author      : Pablo Peluffo
 Version     :
 Copyright   : Your copyright notice
 Description : Programa multihilo que implementa el datalogger con dispositivos
               labjack y modem USB.
               1- Generamos un hilo que maneja el RTC incrementandolo c/1sec.
               El RTC se almacena en una estructura general del sistema.

 Aplicacion	 : DATALOGGERS UTE/MDP.

 ------------------------------------------------------------------------------------------------------------------
 2013-Ago-04:
 - Modificado el modulo dlgLabjackV1_labjack.c para utilizar un nuevo formato de frame, y una nueva configuracion
   de canales.
   Modificado tambien dlgLabjack_config.c para tomar la configuracion nueva de canales.
   ( Ver los correspondientes archivos)

 ------------------------------------------------------------------------------------------------------------------
 Notas de diseño:
 1- Dado que estamos haciendo un prototipo para luego por medio de una
 compilacion cruzada compilarlo en otra arquitectura, no usamos las librerias
 libusb ni el exodriver como librerias sino que los incorporamos como fuentes de
 nuestra aplicacion
 2- Como usamos funciones matematicas (pow) y de clock, debemos poner especiales
 flags para el linker: estas son: m (math), rt (clock), pthread (hilos), dl (sqlite).
 3- Usamos las funciones provistas por "labjackusb.c"

 Hardware:
 - Entradas analogicas:
   La tarjeta LABJACK U3 tiene 16 entradas analogicas single ended, 0-2.4V.
   Estas son: FIO0 .. FIO7 (terminal block en la propia tarjeta) y
   EIO0..EIO7 en la expansion ( a travez de conector DB15).
   La resolucion es de 12 bits.
   Canal: 0-7  / AIN0..AIN7  / (FIO0..FIO7)
          8-15 / AIN8..AIN15 / (EIO0..EIO7)
   Hay que tener en cuenta que las entradas/salidas de la U3 son configurables.
   Por defecto vamos a tener solo hasta 12 entradas analogicas
   En los equipos de 8 entradas, estas van a ser AIN0..AIN7
   En los equipos de 12 van a ser: AIN0..AIN11.

   Los pines donde van las entradas analogicas se configuran en el primer ciclo de poleo.
   Para leer/configurar las entradas analogicas se usa la funcion eAIN documentada en u3.h

 - Entradas digitales:
   Solo vamos a utilizar 4 entradas digitales, CIO0..CIO3. A estas se accede
   desde el conector DB15.( Jameco Part no. 15035).
   La numeracion para su acceso es:
   Canal: 	0-7    FIO0-FIO7  (0-3 unavailable on U3-HV)
			8-15   EIO0-EIO7
			16-19  CIO0-CIO3
	Para leer las entradas digitales usamos eDI documentada en u3.h
	Las entradas digitales al configurarlas como entradas tienen un pull-up de 100k a 5V.

 - Contadores:
   La U3 tiene 2 timers y 2 contadores que estan en secuencia en los pines: T0 T1 C0 C1.
   La asignacion de pines para T0 es: FIO0 + timerCounterPinOffset.
   Nosotros vamos a usar timerCounterPinOffset = 8 ( puede ir de 4 a 8), de modo que la
   asignacion de pines quede a partir de EIO0.
   T0: EIO0
   T1: EIO1
   C0: EIO2
   C1: EIO3
   Los timer para nuestra aplicacion no nos interesan.
   Los counters son de 32 bits, cuentan los flancos de bajada y el clock interno es el
   default de 48mhz de modo que los pulsos deben ser de menor frecuencia.
   Para leerlos usamos la funcion de read&reset.

	FALTA:
	1- revisar las tarea del clock para setearlo simepre que la diferencia sea > 60s

	Para reducir el tamaño del archivo exe. le corremos "strip".

  30-Abr-2012:
  1- Modifico para que el archivo de configuracion lo tome por defecto del directorio /etc/dlg2012.conf

  01-May-2012:
  1- Cuando el enlace cae y el programa no lo detecta, continua trasmitiendo y borrando registros de la base
     por lo que se pierden los datos.
     Agrego chequear que no hallan ACK pendientes antes de trasmitir.

  17-Jun-2012:
  1- El nro. de canales analogicos es siempre 8.
  3- Trasmito en los frames el intervalo de poleo antes de la fecha.
  4- Agrego poner mas canales (analogicos/digitales)
  5- Agrego el parametro de configuracion NROCANALES_ANALOGICOS_EXTRA

  El formato de datos queda:
  >DL01,%,30,18/06/2012,19:34:47,166,167,157,153,159,154,148,150,163,30,30,30,0,0,1,1,1,1,<
  f1: dlgId
  f2: id de frame
  f3: periodo de poleo en segundos
  f4: Fecha yymmdd
  f5: Hora hhmm
  f6..f13: canales analogicos 0-1023
  f7..f10: canales analogicos/digitales pulsados
  f11,f12: contadores de 32 bits
  f13..f16: canales digitales.

  Mensajes:
  INIT: >%s,$,HW=BC+LbjU3,OS=Linux Deb 3.2.0-psp7, FW=dlg2012v1.0 20120426,00<\n
  DATA:   >DL01,%,30,120618,193447,166,167,157,153,159,154,148,150,163,30,30,30,0,0,1,1,1,1,<

 27-Jun-2012:
 1 - Modifico el formato de los mensajes de data para que el tiempo de poleo sea el ultimo
     argumento.
  	  DATA:   >DL01,%,120618,193447,166,167,157,153,159,154,148,150,163,30,30,30,0,0,1,1,1,1,30<
 2 - Agrego la variable systemVars.serviceMode para pasar el equipo a modo servicio.
     Se pasa enviandole la señal 10 (USER1).
     Con esto el equipo deja de loguear, deja de trasmitir y almacenar frames y pasa a polear c/10s
     y mostrar los datos.
     Sale solo a los 5 mins.
 3 - Corrijo un problema de cambio de estados en el modulo de labjack.
 4 - Como las SBC Beagle tienen RTC elimino lo referente a este.

 04-Jul-2012:
 1- Agrego el parametros saveClockTime para configurar cada cuanto queremos que el clock
    se salve al sistema.
 2- Si hay problemas con la Labjack queda en loop enviando errores.
    En este caso lo que hago es parar la tarea de Labjack, resetear el canal usb de esta y
    reiniciarla

 24-Ago-2012:
 1- Incorporo la medida del RSSI en el modulo de datalink y el envio en el frame inicial
 2- Globalizo el string VERSION.
 3- En base a los pedidos de Montes del Plata, incorporo la escritura de datos en un archivo
    para su posterior tratamiento.
    Para no dejar un firmware muy personalizado, grabamos un solo archivo.
    Por medio de un parametro indicamos cuantos datos debe tener el archivo lo que es lo mismo
    que cada cuanto lo grabamos.
 4- Agrego mensajes solo de DEBUG
 5- Reviso la generacion de logs y normalizo los mensajes. TIPO::Funcion:  (INFO | ERROR | DEBUG )
 6- En la rutina de trasmision de frames había un bug que hacia que si la respuesta del ACK
    era muy rapida, el ACK llegaba antes de marcar que lo estaba esperando.
    Ahora se marca antes de trasmitir.

20121029a:
 1- Quedan muchos procesos en estado <defunct>
    Haciendo ps -ef | grep defunct vemos que todos dependen del dlg2012sbc.
    Supongo que es por el rename de los archivos por lo tanto agrego en esta vesion un control
    de los rename.
    Otra opcion es que sea por el system(dInput) para ajustar la hora.
    Como solucion, en c/caso pongo un if.
 2- Agrego un pclose para cerrar el pipe de lectura del ZRSSI.
    Efectivamente este me generaba el primer defunct.
 3- Para el caso de MDP, agrego la opcion de configuracion de no sincronizarse on-line

 ============================================================================
 */

#include "dlgLabjackV1.h"

//const char *argp_program_version = "Spymovil dlg2012 V1.0 @20120403";
const char *argp_program_version = VERSION;
//const char *argp_program_bug_address ="<spymovil@spymovil.com>";
/* Program documentation. */
static char doc[] = "dlgLabjackV1: Spymovil datalogger V1.0 SBC + Labjack U3 + SQLite";
/* A description of the arguments we accept. */
static char args_doc[] = "";

/* The options we understand. */
static struct argp_option options[] = {
   {"config", 'c', "/etc/dlgLabjackV1.conf", 0, "Archivo de configuracion" },
   { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
	char *config_file;
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
/* Get the input argument from argp_parse, which we
   know is a pointer to our arguments structure. */
struct arguments *arguments = state->input;

	switch (key)
    {
    case 'c':
    	arguments->config_file = arg;
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
 //   return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

/*---------------------------------------------------------------------------*/

int main(int argc, char **argv) {

struct arguments arguments;
char logBuffer[MAXLOGBUFFER];

	arguments.config_file = "/etc/dlgLabjackV1.conf";
	/* Parse our arguments; every option seen by parse_opt will
	be reflected in arguments. */
	argp_parse (&argp, argc, argv, 0, 0, &arguments);

	// El archivo de configuracion (path) lo leo primero de los argumentos de entrada
	strcpy(systemParameters.configFile,  arguments.config_file);

	// Instalamos el handler de la señal de modo service
	signal (10, sig_user1);

	openlog( "dlgLabjackV1 ", LOG_PID, LOG_USER);
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "INFO::main: Starting dlgLabjackV1...");
	F_syslog("%s", logBuffer);

	conf_fnReadConfFile();
	conf_fnPrintConfParams();
	db_open();

	// Cleanup de los archivos del directorio /root/datFile
	cleanUpFiles();

	// Inicializo el reloj interno.
	initClock();

	if (systemParameters.daemonMode == 1)
		daemonize();

	linkStatus = LINKDOWN;
	systemVars.pktWaitingAck = 0;
	systemVars.serviceMode = 0;

	// Espero a detectar alguna tarjeta.
	labJackDetect();
	/*---------------------------------------------------------------------------*/
	// Inicio el hilo de TICKS
	sleep(1);
	if ( pthread_create( &tick_threadHandler, NULL, tick_thService, NULL) ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::main: error al crear el hilo tick_thread.");
		F_syslog("%s", logBuffer);
	    abort();
	}

	// Inicio el hilo de mantenimiento del enlace
	sleep(1);
	if ( pthread_create( &link_threadHandler, NULL, link_thService, NULL) ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::main: error al crear el hilo link_thread.");
		F_syslog("%s", logBuffer);
	    abort();
	}

	// Inicio el hilo de atencion de rx
	sleep(1);
	if ( pthread_create( &rx_threadHandler, NULL, rx_thService, NULL) ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::main: error al crear el hilo rxdata_thread.");
		F_syslog("%s", logBuffer);
	    abort();
	}

	// Inicio el hilo de Labjack
	sleep(1);
	if ( pthread_create( &lbj_threadHandler, NULL, lbj_thService, NULL) ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::main: error al crear el hilo labjack_thread.");
		F_syslog("%s", logBuffer);
	    abort();
	}

	/*---------------------------------------------------------------------------*/
	/* Espera a finalizacion de hilos */
	if ( pthread_join ( tick_threadHandler, NULL ) ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::main: error joining tick_threadHandler.");
		F_syslog("%s", logBuffer);
	    abort();
	}

	if ( pthread_join( lbj_threadHandler, NULL) ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::main: error joining labjack_threadHandler.");
		F_syslog("%s", logBuffer);
	    abort();
	}

	if ( pthread_join( link_threadHandler, NULL) ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::main: error joining link_threadHandler.");
		F_syslog("%s", logBuffer);
	    abort();
	}

	if ( pthread_join( rx_threadHandler, NULL) ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "ERROR::main: error joining rx_threadHandler.");
		F_syslog("%s", logBuffer);
	    abort();
	}

	exit(0);
}


/*---------------------------------------------------------------------------*/

void sig_user1(int signo)
{

/* Handler de la señal SIGUSR1. La usamos para indicarle al programa
 * que pase a modo service o salga
 * En modo service cambia el tiempo de poleo a 10s, no guarda en la base
 * ni trasmite.
 * Queda en modo service por un maximo de 5 minutos y sale automaticamente.
*/

	// Entro en modo service
	if ( systemVars.serviceMode == 0 ) {
		F_syslog("SERVICE: Entro en modo service...");
		systemVars.serviceMode = systemParameters.serviceLastTime;
		//printf("SERVICE:[%d] Entro en modo service...\n", systemVars.serviceMode);

		return;
	}

	// Salgo de modo service
	if ( systemVars.serviceMode > 0 ) {
		F_syslog("SERVICE: Salgo de modo service...");
		//printf("SERVICE:[%d] Salgo en modo service...\n", systemVars.serviceMode);
		systemVars.serviceMode = 0;
		return;
	}

}

/*---------------------------------------------------------------------------*/

void F_syslog(const char *fmt,...)
{
/* Loguea por medio del syslog la linea de texto con el formato pasado
 * Por ahora solo imprimimos en pantalla
 * Tener en cuenta que en caso de los forks esto puede complicarse por
 * lo cual hay que usar el syslog
 * La funcion la definimos del tipo VARADIC (leer Manual de glibC.) para
 * poder pasar un numero variable de argumentos tal como lo hace printf.
 *
 * Siempre loguea en syslog. Si NO es demonio tambien loguea en stderr
*/
	va_list ap;
	char buf[MAXLINE - 1], printbuf[MAXLINE - 1];
	int i,j;
	time_t sysClock;
	struct tm *cur_time;
	char sysDateTime[32];

	va_start(ap, fmt);

	/* Imprimimos en el buffer los datos pasados en argumento con formato
    * Usamos vsnprintf porque es mas segura que vsprintf ya que controla
    * el tamaño del buffer.
    * Agregamos siempre un \n.
  */
	vsnprintf(buf, MAXLINE, fmt, ap);

	/* Si estoy en modo service solo logueo los mensajes de SERVICE */
	/*
	if (systemVars.serviceMode > 0 ) {
		if (strstr( printbuf, "SERVICE")) {
			return;
		}
	}
	*/
	// Veo si genero el debug o no.
	if ( systemParameters.debug == 0) {
		if ( memmem( buf, 10, "DEBUG", 5 )) {
			return;
		}
		return;
	}

	/* Elimino los caracteres  \r y \n para no distorsionar el formato. */
	i=0;
	j=0;
	while ( buf[i] != '\0') {
		if ( (buf[i] == '\n') || (buf[i] == '\r')) {
			i++;
		} else {
			printbuf[j++] = buf[i++];
		}
	}
	printbuf[j++] = '\n';
	printbuf[j++] = '\0';

	/* Si el programa se transformo en demonio uso syslog, sino el dato lo
      manda tambien como stream al stdout
	*/

	syslog(LOG_INFO | LOG_USER,  printbuf);

	/* Hallo la fecha */
	sysClock = time (NULL);
	cur_time = localtime(&sysClock);
	strftime (sysDateTime, 32, "%F %T", cur_time);

	/* Si el programa NO es demonio, muestro el log por el stdout */
	if (systemParameters.daemonMode == 0) {
		fflush(stdout);
		printf("%s: ", sysDateTime);
		fputs(printbuf, stdout);
		fflush(stdout);
	}

}
/*--------------------------------------------------------------------------*/

void daemonize( void)
{
	pid_t pid;

	if ( ( pid = fork()) != 0)
		exit (0);					/* El parent termina */

	/* Continua el primer child  que se debe volver lider de sesion y no tener terminal */
	setsid();

	signal(SIGHUP, SIG_IGN);

	if ( (pid = fork()) != 0)
		exit(0);					/* El primer child termina. Garantiza que el demonio no pueda
									 * adquirir una terminal
									*/
	/* Continua el segundo child */
//	chdir("/");
	umask(0);

//	for(i=0; i<MAXFD; i++)	/* Cerramos todos los descriptores de archivo que pueda haber heredado */
//		close(i);

}

/*--------------------------------------------------------------------------*/

