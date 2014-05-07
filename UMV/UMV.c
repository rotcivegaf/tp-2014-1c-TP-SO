#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/string.h>

#define MAX 50

void encabezado(long byte, char *modo);
int clasificarComando(char *comando);
void operacion(int proceso, int base, int offset, int tamanio);
void retardo(int milisegundos);
void algoritmo(char *modo);
void compactar();
void dump();


int main()
{
	FILE *archConfig;
	char cadTamanio[10];
	char modo[10];
	long tamanio;
	char **arrayComando;
	char entrada[100];
	int a;

	archConfig = fopen("umv_config.txt","r");

	while (feof(archConfig) == 0)
		{
		fgets (cadTamanio, MAX, archConfig);
		fgets (modo,MAX,archConfig);
		}

	tamanio = atoi(cadTamanio);
	encabezado(tamanio, modo);
	fclose(archConfig);

	printf("UMV >> ");
	fgets(entrada, MAX, stdin);

	arrayComando = string_get_string_as_array(entrada);
	a = clasificarComando(arrayComando[0]);


	while (a != 6)
	{
		switch (a){
			case 1:
				operacion(atoi(arrayComando[1]),atoi(arrayComando[2]),atoi(arrayComando[3]),atoi(arrayComando[4]));
				break;
			case 2:
				retardo(atoi(arrayComando[1]));
				break;
			case 3:
				algoritmo(arrayComando[1]);
				break;
			case 4:
				compactar();
				break;
			case 5:
				dump();
				break;
			default:
				printf("Comando desconocido");
				break;
			}
		printf("\nUMV >> ");
		fgets(entrada, MAX, stdin);

		arrayComando = string_get_string_as_array(entrada);
		a = clasificarComando(arrayComando[0]);
	}
	return 0;
}


int clasificarComando (char *comando)
{
	int a;
	if (!strcmp(comando,"operacion")) a=1;
	if (!strcmp(comando,"retardo")) a=2;
	if (!strcmp(comando,"algoritmo")) a=3;
	if (!strcmp(comando,"compactacion")) a=4;
	if (!strcmp(comando,"dump")) a=5;
	if (!strcmp(comando,"exit")) a=6;
	return a;
}

void operacion(int proceso, int base, int offset, int tamanio)
{
	printf("operacion");
}
void retardo(int milisegundos)
{
	printf("retardo");
}

void algoritmo(char *modo)
{
	printf("algoritmo");
}
void compactar()
{
	printf("compactar");
}

void dump()
{
	printf("dump");
}


void encabezado(long byte, char *modo)
{
	printf("** UMV: Unidad de Memoria Virtual **\n");
	printf("Operando en modo: %s\n", modo);
	printf("Espacio reservado: %ld bytes\n", byte);
}
