#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/string.h>
#include <commons/config.h>

#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>





#define MAX 50

void encabezado(long byte, char *modo);
int clasificarComando(char *comando);
void operacion(int proceso, int base, int offset, int tamanio);
void retardo(int milisegundos);
void algoritmo(char *modo);
void compactar();
void dump();
int asignarMemoria(char modo);
void recorrerTablaSegmentos();

void recorrerLista(void *ptrLista);



void *ptrMemoria;
void *tablaProgramas;
char modoOperacion;
void *listaAuxiliar;

typedef struct t_tabMem {
	int memLogica;
	int longitud;
	int memFisica;
	} TabMen;

typedef struct t_auxiliar {
	int ptrInicio;
	int ptrFin;
	void *ptrATabla;
	} ListAuxiliar;


void completarListaAuxiliar(TabMen *nodo);


int main()
{

	char **arrayComando;
	int a;
	void *ptrConfig;
	char entrada[100];

	extern void *ptrMemoria;

	ptrConfig = config_create("umv_config.txt");

	encabezado(config_get_int_value(ptrConfig,"tamanio"),config_get_string_value(ptrConfig,"modo"));

	ptrMemoria = malloc(config_get_int_value(ptrConfig,"tamanio"));

	tablaProgramas = dictionary_create();

	printf("UMV >> ");
	gets(entrada);
	arrayComando =  string_split(entrada," ");
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
	gets(entrada);
	arrayComando =  string_split(entrada," ");
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
	if (!strcmp(comando,"exit"))
		{
		a=6;
		free(ptrMemoria);
		}
	printf("%d",a);
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
	if (!strcmp(modo,"WorstFit") || !strcmp(modo,"FirstFit"))
		modoOperacion=modo[0];
	else
		printf("Modo incorrecto");
}

bool CompararListaAuxiliar(ListAuxiliar* nodo1, ListAuxiliar* nodo2){
	return nodo1->ptrInicio < nodo2->ptrInicio;
}

void compactar()
{
	ListAuxiliar *p1 = malloc(sizeof(ListAuxiliar));
	ListAuxiliar *p2 = malloc(sizeof(ListAuxiliar));
	int x;

	recorrerTablaSegmentos();
	list_sort(listaAuxiliar, CompararListaAuxiliar);
	for (x=0;x<list_size(listaAuxiliar)-1;x++){
		p1 = list_get(listaAuxiliar,x);
		p2 = list_get(listaAuxiliar,x+1);

		if (p2->ptrInicio != (p1->ptrFin) +1)
		{
			//desplazarMemoria(deDonde,ADonde);
			//actualizarDiccionario(nuevoInicio,DireccionDelNodoAActualizar)
		}
	}
	list_destroy(listaAuxiliar);
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


void crearSegmento(char *id_Prog, int tamanio)
{
	void *lista;
	TabMen *nodoTab = malloc(sizeof(TabMen));

	nodoTab->memLogica = 0;
	nodoTab->longitud = tamanio;
	nodoTab->memFisica = asignarMemoria(modoOperacion);

	if (dictionary_has_key(tablaProgramas, id_Prog))
		{
		lista = dictionary_get(tablaProgramas,id_Prog);
		list_add(lista,nodoTab);
		}
	else
	{
		lista = list_create();
		list_add(lista,nodoTab);
		dictionary_put(tablaProgramas, id_Prog, lista);
	}

}

int asignarMemoria(char modo)
{
	return 0;
}


void recorrerTablaSegmentos()
{
	listaAuxiliar= list_create();
	dictionary_iterator(tablaProgramas,recorrerLista);
}

void recorrerLista(char* clave, void *ptrLista){
	list_iterate(ptrLista,completarListaAuxiliar);
}

void completarListaAuxiliar(TabMen *nodo)
{
	ListAuxiliar *nodoAux = malloc(sizeof(ListAuxiliar));

	nodoAux->ptrInicio = nodo->memFisica;
	nodoAux->ptrFin = nodo->memFisica + nodo->longitud;
	nodoAux->ptrATabla = nodo;

	list_add(listaAuxiliar, nodoAux);
}
// error en el sort y en los iterator
