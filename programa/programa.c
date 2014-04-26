/*
 * programa.c
 *
 *  Created on: 25/04/2014
 *      Author: utnso
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <commons/config.h>

#define IP "127.0.0.1"
#define PUERTO "6667"
#define CARACTXLINEA 100


int main(int argc, char *argv[])
{
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/*t_config *config=config_create("programa.conf"); 	el arch de config se saca de la variable de entorno ANSISOP_CONFIG, lo cual ni idea
	int ipKernel = config_get_int_value(config, "IP");
	int puertoKernel = config_get_int_value(config, "Puerto");*/

	getaddrinfo(IP, PUERTO, &hints, &serverInfo); 	//Carga los datos de la conexion
	int serversocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	connect(serversocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo); 						//Ya no se necesita la info

	char linea[CARACTXLINEA];
	FILE *scriptAProcesar = fopen(argv[1],"r");

	fgets(linea,CARACTXLINEA,scriptAProcesar);		//Para que no tome la primera
	while(!feof(scriptAProcesar)){
		fgets(linea,CARACTXLINEA,scriptAProcesar);
		send(serversocket, linea, strlen(linea) + 1, 0);
	}

	close(serversocket);
	fclose(scriptAProcesar);
	return 0;
}


