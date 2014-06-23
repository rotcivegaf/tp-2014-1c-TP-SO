#ifndef UMV_H_
#define UMV_H_

	#include <stdbool.h>
	#include <math.h>
	#include <time.h>
	#include "sockets_lib.h"
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/types.h>
	#include <unistd.h>
	//commons
	#include <commons/string.h>
	#include <commons/config.h>
	#include <commons/collections/dictionary.h>
	#include <commons/collections/list.h>
	//parser-ansisop
	#include <parser/metadata_program.h>
	#include <parser/parser.h>
	//hilos
	#include <pthread.h>

	#define MAX 50

	typedef struct{
		int soc;
	}t_param_conec_kernel;

	void encabezado(long byte, char *modo);
	void *crearConsola();
	void *admin_conecciones();
	void *admin_conec_kernel();
	void *admin_conec_cpu();
	int clasificarComando(char *comando);
	void operacion(int proceso, int base, int offset, int tamanio);
	void retardo(int milisegundos);
	void algoritmo(char *modo);
	void compactar();
	void dump();
	int asignarMemoria(int tamanio);
	void recorrerTablaSegmentos();
	void recorrerLista(char *clave, void *ptrLista);
	void insertarNodosBarrera ();
	int asignarMemoriaAleatoria(int tamanio);
	void destruirSegmentos(char *id_Prog);
	void eliminarElemento(void *elemento);
	int controlarMemPisada(void *lista, int numMemoria, int tamanio);
	void *solicitarBytes (int base, int offset, int tamanio);
	void almacenarBytes (int base,int offset,int tamanio, /*void/char *buffer*/ );

	void *ptrMemoria;
	void *tablaProgramas;
	char modoOperacion;
	void *listaAuxiliar;
	void *ptrConfig;

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
	bool compararListaAuxiliar(ListAuxiliar* nodo1, ListAuxiliar* nodo2);
	TabMen *encontrarSegmento(void *lista, int base);

#endif /* UMV_H_ */
