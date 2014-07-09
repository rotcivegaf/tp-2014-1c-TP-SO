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

	void crear_cont(sem_t *sem, int32_t  val_ini);
	void sem_incre(sem_t *sem);
	void sem_decre(sem_t *sem);

	typedef struct{
		int32_t multiprogramacion;
	} t_param_new_ready;
	typedef struct{
		int32_t quantum;
	} t_param_ready_exec;
	typedef struct{
		int32_t cant_unidades;
		int32_t retardo;
		int32_t id_proc;
	} t_param_IO;
	typedef struct {
		int32_t soc_cpu;
		int32_t id_prog_exec;
	}t_cpu;
	typedef struct{
		t_queue *cola_ready;
		t_queue *cola_exec;
		t_queue *cola_new;
		t_queue *cola_exit;
		t_queue *cola_block;
	} t_colas;
	typedef struct {
		t_pcb *pcb;
		int32_t n_socket;
		int32_t peso;
		int32_t tipo_fin_ejecucion;
	} t_pcb_otros;
	typedef struct{
		char *puerto_prog;
		char *ip_umv;
		char *puerto_umv;
		int32_t  retardo;
		int32_t  max_multiprogramacion;
		int32_t  tam_stack;
	} t_param_plp;
	typedef struct{
		char *puerto_cpu;
		int32_t  retardo;
		int32_t  max_multiprogramacion;
		t_queue *cola_IO;
		char **semaforos;
		char **valor_semaforos;
		int32_t  quantum;
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
		int32_t  multiprogramacion;
		int32_t  quantum;
		int32_t  retardo;
		int32_t  tam_stack;
		char **variables_globales;
	} t_datos_config;
	typedef struct{
		char *id_hio;
		int32_t  hio_sleep;
		t_queue *procesos;
		pthread_t hilo;
		pthread_mutex_t mutex_dispositivo;
	} t_IO;
	typedef struct{
		int32_t id_prog;
		int32_t unidades;
	}t_IO_espera;
	typedef struct{
		t_dir_mem dir_primer_byte_umv_segmento_codigo;
		t_dir_mem dir_primer_byte_umv_segmento_stack;
		t_dir_mem dir_primer_byte_umv_indice_codigo;
		t_dir_mem dir_primer_byte_umv_indice_etiquetas;
		int32_t  memoria_insuficiente;
	} t_resp_sol_mem;
	typedef struct{
		char *id_sem;
		int32_t valor;
		t_queue *procesos;
	}t_semaforo;

	t_datos_config *levantar_config();
	void handshake_cpu(int32_t  soc);
	void handshake_prog(int32_t  soc);
	void handshake_umv(char *ip_umv, char *puerto_umv);
	t_resp_sol_mem * solicitar_mem(char *script, int32_t  tam_stack, int32_t  id_prog);
	t_pcb *crear_pcb_escribir_seg_UMV(t_men_comun *men_cod_prog ,t_resp_sol_mem *resp_sol ,int32_t  contador_id_programa);
	int32_t  calcular_peso(t_men_comun *men_cod_prog);
	t_param_plp *ini_pram_plp(t_datos_config *diccionario_config);
	t_param_pcp *ini_pram_pcp(t_datos_config *diccionario_config);
	void *plp();
	void *pcp();
	void *imp_colas();
	void *manejador_new_ready();
	void *manejador_ready_exec();
	void *manejador_exit();
	//Se le manda el numero de socket del programa y lo busca en cada cola, si lo encuentra lo pone en exit (y retorna) y si no lo vuelve a poner
	int32_t  mover_pcb_exit(int32_t  soc_prog);
	t_pcb_otros *get_pcb_otros_exec(int32_t  id_proc);
	t_pcb_otros *get_pcb_otros_exec_sin_quitarlo(int32_t  id_proc);
	t_cpu *get_cpu(int32_t  soc_cpu);
	void enviar_IO(int32_t  soc_cpu, int32_t id_IO);
	void moverAblock(t_pcb_otros *pcb_peso);
	void *entrar_IO();
	t_cpu *get_cpu_libre(int32_t  *res);
	t_pcb_otros *get_peso_min();
	void umv_destrui_pcb(int32_t id_pcb);
	void socket_send_pcb(int32_t  soc,t_pcb *pcb,int32_t  quantum);
	void actualizar_pcb(t_pcb *pcb, t_pcb *pcb_actualizado);
	void pasar_pcb_exit(t_pcb_otros *pcb);
	void pasar_pcbBlock_exit(int32_t id_pcb);
	void llamada_erronea(int32_t tipo_error, int32_t soc_cpu);
	t_pcb_otros *actualizar_pcb_y_bloq(t_cpu *cpu);

#endif /* KERNEL_H_ */
