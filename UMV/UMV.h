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
	#include <commons/txt.h>
	#include <commons/collections/dictionary.h>
	#include <commons/collections/list.h>
	//parser-ansisop
	#include <parser/metadata_program.h>
	#include <parser/parser.h>
	//hilos
	#include <pthread.h>

	#define FIRST_FIT 1
	#define WORST_FIT 2

	typedef struct{
		int32_t soc;
	}t_param_conec_cpu;
	typedef struct {
		int32_t id_proc;
		int32_t dir_logica;
		int32_t dir_fisica;
		int32_t tam_seg;
		int32_t tipo_seg;
	} t_seg;

	void crear_hilo(pthread_t *hilo,  void *_funcion (void *),  void *param);
	void *admin_conecciones();
	void *admin_conec_kernel();
	void gestionar_ped_seg(t_men_seg *men_seg,int32_t tipo_resp);
	void gestionar_alm_seg(int32_t id_proc);
	void almacenar_segmento(t_men_comun *aux_men, int32_t id_proc);
	t_seg *buscar_segmento(int32_t tipo_seg,int32_t id_proc);
	void destruir_lista_segmento(t_list *list_seg);
	void *admin_conec_cpu();
	void gestionar_solicitud_bytes(int32_t soc_cpu,t_men_cpu_umv *men_bytes, int32_t proc_activo);
	void gestionar_almacenamiento_bytes(int32_t soc_cpu, t_men_cpu_umv *men_bytes, int32_t proc_activo);
	void compactar();
	int32_t crearSegmento(t_men_seg *men_ped_seg);
	int32_t asignarMemoriaAleatoria(int32_t tamanio);
	t_list *obtener_lista_seg_ord_dir_fisica();
	int32_t buscar_espacio_mem_prin(int32_t tam_a_reservar);
	void destruirSegmentos(int id_prog);

	//consola
	void *crearConsola();
	void menuPrincipal();
	void cambiarAlgoritmo();
	void cambiarRetardo();
	void operacion();
	void operacion_segmentos(char opcion);
	void operacion_memoria(char opcion);
	void imprimirDump();
	void imp_estructura_mem();
	void imp_mem_prin();
	void imp_cont_mem_prin();
	void imp_tablas_segmentos(int32_t id_proc);
	void imp_seg();

	void encabezado(long byte, char *modo);
	void traducir_tipo_de_seg_y_logear(int32_t tipo);
	void traducir_tipo_men_bytes_y_logear(int32_t tipo);

#endif /* UMV_H_ */
