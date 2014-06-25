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
		int32_t soc;
	}t_param_conec_kernel;
	typedef struct{
		int32_t soc;
	}t_param_conec_cpu;

	//consola
	void *crearConsola();
	void menuPrincipal();
	void cambiarAlgoritmo();
	void cambiarRetardo();
	void operacion();
	void imprimirDump();
	void imp_estructura_mem();
	void imp_mem_prin();
	void imp_cont_mem_prin();
	int32_t imp_tablas_segmentos(int32_t id_proc);

	void encabezado(long byte, char *modo);
	void *admin_conecciones();
	void *admin_conec_kernel();
	void *admin_conec_cpu();
	void compactar();
	int32_t asignarMemoria(int32_t tamanio);
	void recorrerTablaSegmentos();
	void recorrerLista(char *clave, void *ptrLista);
	void insertarNodosBarrera ();
	int32_t asignarMemoriaAleatoria(int32_t tamanio);
	int32_t crearSegmento(t_men_seg *men_ped_seg);
	void destruirSegmentos(char *id_Prog);
	void eliminarElemento(void *elemento);
	int32_t controlarMemPisada(void *lista, int32_t numMemoria, int32_t tamanio);
	void *solicitarBytes (int32_t base, int32_t offset, int32_t tamanio);
	void almacenarBytes (int32_t base,int32_t offset,int32_t tamanio, char *buffer);

	typedef struct t_tabMem {
		int32_t memLogica;
		int32_t memFisica;
		int32_t longitud;
		int32_t tipo_seg;
		} TabMen;

	typedef struct t_auxiliar {
		int32_t ptrInicio;
		int32_t ptrFin;
		void *ptrATabla;
		} ListAuxiliar;

	void completarListaAuxiliar(TabMen *nodo);
	bool compararListaAuxiliar(ListAuxiliar* nodo1, ListAuxiliar* nodo2);
	TabMen *encontrarSegmento(void *lista, int32_t base);

#endif /* UMV_H_ */
