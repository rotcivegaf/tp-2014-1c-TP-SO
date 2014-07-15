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
	#include <commons/txt.h>
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
		int32_t tam_stack;
	} t_param_plp;
	typedef struct{
		char *puerto_cpu;
		t_queue *cola_IO;
	} t_param_pcp;
	typedef struct{
		char *puerto_prog;
		char *puerto_cpu;
		char *ip_umv;
		char *puerto_umv;
		int32_t multiprogramacion;
		int32_t tam_stack;
	} t_datos_config;
	typedef struct{
		int32_t  io_sleep;
		t_queue *procesos;
		pthread_t hilo;
		sem_t cont_cant_proc;
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
		int32_t memoria_insuficiente;
	} t_resp_sol_mem;
	typedef struct{
		int32_t valor;
		t_queue *procesos;
	}t_semaforo;

	t_datos_config *levantar_config();
	void handshake_cpu(int32_t  soc);
	void handshake_prog(int32_t  soc);
	void handshake_umv(char *ip_umv, char *puerto_umv);
	t_resp_sol_mem * solicitar_mem(t_men_comun *men_cod_prog, int32_t  tam_stack, int32_t  id_prog);
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
	void *manejador_IO(t_IO *io);
	//Se le manda el numero de socket del programa y lo busca en cada cola, si lo encuentra lo pone en exit (y retorna) y si no lo vuelve a poner
	void  mover_pcb_exit(int32_t  soc_prog);
	t_pcb_otros *buscar_pcb(t_queue *cola, int32_t soc_prog);
	t_pcb_otros *get_pcb_otros_exec(int32_t  id_proc);
	t_pcb_otros *get_pcb_otros_exec_sin_quitarlo(int32_t  id_proc);
	t_cpu *get_cpu(int32_t  soc_cpu);
	void enviar_IO(int32_t  soc_cpu, char *id_IO);
	void moverAblock(t_pcb_otros *pcb_peso);
	t_cpu *get_cpu_libre();
	t_pcb_otros *get_peso_min();
	void umv_destrui_pcb(int32_t id_pcb);
	void enviar_cpu_pcb_destruir(int32_t  soc,t_pcb *pcb,int32_t  quantum);
	void actualizar_pcb(t_pcb *pcb, t_pcb *pcb_actualizado);
	void pasar_pcb_exit(t_pcb_otros *pcb);
	void pasar_pcbBlock_ready(int32_t id_pcb);
	void llamada_erronea(int32_t soc_cpu,int32_t tipo_error);
	t_pcb_otros *actualizar_pcb_y_bloq(t_cpu *cpu);

	//funciones del pcp
	void enviar_IO(int32_t soc_cpu, char *id_IO);
	void fin_ejecucion(int32_t tipo,int32_t socket_cpu);
	void imprimir_valor(int32_t soc, t_men_comun *men_imp_valor);
	void imprimir_texto(int32_t soc, t_men_comun *men_imp_texto);
	void obtener_valor_compartida(int32_t soc_cpu, t_men_comun *men_obt_valor);
	void grabar_valor_compartida(int32_t soc_cpu, t_men_comun *men_gra_valor);
	void fin_quantum(int32_t soc_cpu, t_men_comun *men_fin_quantum);

	void logear_int(FILE* destino,int32_t un_int);

	void enviar_resp_cpu(int32_t soc_cpu, int32_t tipo_mensaje);

#endif /* KERNEL_H_ */
