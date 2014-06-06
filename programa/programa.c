/*
 * programa.c
 *
 *  Created on: 25/04/2014
 *      Author: utnso
 */

#include "funciones.h"
#include <commons/config.h>

#define IP "127.0.0.1" //Esto hay que cargarlo por archivo de config, que esta en la variable de entorno ANSISOP_CONFIG
#define PUERTO 5000


int tamanioArchivo(FILE *archivo);

int main(int argc, char *argv[])
{
	/*t_config *config=config_create("configuracion.conf");
	char *ipKernel=config_get_string_value(config, "IP");
	int puertoKernel = config_get_int_value(config, "PUERTO");*/

	FILE *scriptAProcesar = fopen(argv[1],"r");
	int tamanioScript=tamanioArchivo(scriptAProcesar);
	char *script = malloc(tamanioScript);

	int socket=conectarse(IP,PUERTO);

	int i=0;
	//Lee el script
	while(!feof(scriptAProcesar))
		script[i++]=fgetc(scriptAProcesar);

	char *script_serializado = malloc(sizeof(int)+tamanioScript);
	memcpy(script_serializado,&tamanioScript,sizeof(int));
	memcpy(script_serializado+sizeof(int),script,tamanioScript);

	send(socket,script_serializado,sizeof(int)+tamanioScript,0);

	close(socket);
	fclose(scriptAProcesar);
	free(script);
	return 0;
}

int tamanioArchivo(FILE *archivo){
	fseek(archivo, 0L, SEEK_END);
	int tamanio = ftell(archivo);
	fseek(archivo,0L, SEEK_SET);
	return tamanio;
}


