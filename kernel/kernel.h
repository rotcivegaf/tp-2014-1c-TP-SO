#ifndef KERNEL_H_
#define KERNEL_H_

	#include "sockets_lib.h"
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/types.h>
	#include <unistd.h>
	//commons
	#include <commons/config.h>
	#include <commons/collections/dictionary.h>
	#include <commons/collections/queue.h>
	//parser-ansisop
	#include <parser/metadata_program.h>
	#include <parser/parser.h>
	//hilos
	#include <pthread.h>
	//semaforos
	#include <semaphore.h>
	#include <fcntl.h>

	void crear_cont(sem_t *sem, int val_ini);
	void sem_incre(sem_t *sem);
	void sem_decre(sem_t *sem);

	typedef struct{
		int multiprogramacion;
	} t_param_new_ready;
	typedef struct{
		int quantum;
	} t_param_ready_exec;
	typedef struct{
		int cant_unidades;
		int retardo;
		int id_proc;
	} t_param_IO;
	typedef struct {
		int soc_cpu;
		int id_prog_exec;
	}t_cpu;
	typedef int t_dir_mem;
	typedef struct{
		t_queue *cola_ready;
		t_queue *cola_exec;
		t_queue *cola_new;
		t_queue *cola_exit;
		t_queue *cola_block;
	} t_colas;
	typedef struct {
		int id;										//Identificador único del Programa en el sistema
		t_dir_mem dir_primer_byte_umv_segmento_codigo;	//Dirección del primer byte en la UMV del segmento de código
		t_dir_mem dir_primer_byte_umv_segmento_stack;		//Dirección del primer byte en la UMV del segmento de stack
		t_dir_mem dir_primer_byte_umv_contexto_actual;	//Dirección del primer byte en la UMV del Contexto de Ejecución Actual
		t_dir_mem dir_primer_byte_umv_indice_codigo;		//Dirección del primer byte en la UMV del Índice de Código
		t_dir_mem dir_primer_byte_umv_indice_etiquetas;	//Dirección del primer byte en la UMV del Índice de Etiquetas
		int program_counter;						//Número de la próxima instrucción a ejecutar
		int cant_var_contexto_actual;				//Cantidad de variables (locales y parámetros) del Contexto de Ejecución Actual
		int tam_indice_etiquetas;					//Cantidad de bytes que ocupa el Índice de etiquetas
	} t_pcb;
	typedef struct {
		t_pcb *pcb;
		int n_socket;
		int peso;
		int tipo_fin_ejecucion;
	} t_pcb_otros;
	typedef struct{
		char *puerto_prog;
		char *ip_umv;
		char *puerto_umv;
		int retardo;
		int max_multiprogramacion;
		int tam_stack;
	} t_param_plp;
	typedef struct{
		char *puerto_cpu;
		int retardo;
		int max_multiprogramacion;
		t_queue *cola_IO;
		char **semaforos;
		char **valor_semaforos;
		int quantum;
		char **variables_globales;
	} t_param_pcp;
	typedef struct{
		char *puerto_prog;
		char *puerto_cpu;
		char *ip_umv;
		char *puerto_umv;
		t_queue *cola_IO;
		char **semaforos;
		char **valor_semaforos;
		int multiprogramacion;
		int quantum;
		int retardo;
		int tam_stack;
		char **variables_globales;
	} t_datos_config;
	typedef struct{
		char *id_hio;
		int hio_sleep;
	} t_IO;
	typedef struct{
		t_dir_mem dir_primer_byte_umv_segmento_codigo;
		t_dir_mem dir_primer_byte_umv_segmento_stack;
		t_dir_mem dir_primer_byte_umv_indice_codigo;
		t_dir_mem dir_primer_byte_umv_indice_etiquetas;
		int memoria_insuficiente;
	} t_resp_sol_mem;

	t_datos_config *levantar_config();
	void handshake_cpu(int soc);
	void handshake_prog(int soc);
	void handshake_umv(char *ip_umv, char *puerto_umv);
	t_resp_sol_mem * solicitar_mem(t_mensaje *men_cod_prog, int tam_stack, int id_prog);
	t_pcb *crear_pcb_escribir_seg_UMV(t_mensaje *men_cod_prog ,t_resp_sol_mem *resp_sol ,int *contador_id_programa);
	int calcular_peso(t_mensaje *men_cod_prog);
	t_param_plp *ini_pram_plp(t_datos_config *diccionario_config);
	t_param_pcp *ini_pram_pcp(t_datos_config *diccionario_config);
	void *plp();
	void *pcp();
	void *imp_colas();
	void *manejador_new_ready();
	void *manejador_ready_exec();
	void *manejador_exit();
	int mover_pcb_exit(int soc_prog);
	t_pcb_otros *get_pcb_otros_exec(int id_proc);
	t_pcb_otros *get_pcb_otros_exec_sin_quitarlo(int id_proc);
	t_cpu *get_cpu(int soc_cpu);
	void enviar_IO(int soc_cpu, t_queue *cola_IO);
	void moverAblock(t_pcb_otros *pcb_peso);
	void *entrar_IO();
	t_cpu *get_cpu_libre(int *res);
	t_pcb_otros *get_peso_min();
	void umv_destrui_pcb(t_pcb *pcb);
	void socket_send_pcb(int soc,t_pcb *pcb,int quantum);

#endif /* KERNEL_H_ */
