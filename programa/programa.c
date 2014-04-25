/*
 * programa.c
 *
 *  Created on: 25/04/2014
 *      Author: utnso
 */

#include <stdio.h>
#include <stdlib.h>

#define CARACTXLINEA 100

int main(int argc, char *argv[])
{
	char *linea = malloc(sizeof(char)*CARACTXLINEA);
	FILE *scriptAProcesar = fopen(argv[1],"r");

	fgets(linea,CARACTXLINEA,scriptAProcesar);
	while(!feof(scriptAProcesar)){
		fgets(linea,CARACTXLINEA,scriptAProcesar);
		printf("%s",linea);
		//El print es para probar, habria que cambiarlo por un send
	}

	fclose(scriptAProcesar);
	free(linea);
	return 0;
}


