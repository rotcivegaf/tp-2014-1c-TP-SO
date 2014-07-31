#ifndef KERNEL_H_
#define KERNEL_H_

	#include "sockets_lib.h"
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/types.h>
	#include <unistd.h>
	//commons
	#include "commons/config.h"
	#include "commons/txt.h"
	#include "commons/collections/dictionary.h"
	#include "commons/collections/queue.h"
	//parser-ansisop
	#include "parser/metadata_program.h"
	#include "parser/parser.h"
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
		int32_t soc_prog;
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
		t_dir_mem dir_seg_codigo;
		t_dir_mem dir_seg_stack;
		t_dir_mem dir_indice_codigo;
		t_dir_mem dir_indice_etiquetas;
	} t_resp_sol_mem;
	typedef struct{
		int32_t valor;
		t_queue *procesos;
	}t_semaforo;

	//inicio
	void levantar_config();
	void crear_inicializar_estructuras();
	void handshake_cpu(int32_t  soc);
	void handshake_prog(int32_t  soc);
	void handshake_umv();

	//hilos
	void *plp();
	void *pcp();
	void *manejador_new_ready();
	void *manejador_ready_exec();
	void *manejador_exit();
	void *manejador_IO(t_IO *io);

	//funciones del plp
	int32_t ingresar_nuevo_programa();
	void administrar_men_progs(int32_t soc_prog);
	void administrar_prog_cerrado(int32_t soc_prog,t_men_comun *men_prog);
	void administrar_new_script(int32_t soc_prog, t_men_comun *men_prog);

	t_resp_sol_mem * solicitar_mem(t_men_comun *men_cod_prog);
	t_pcb *crear_pcb_escribir_seg_UMV(t_men_comun *men_cod_prog ,t_resp_sol_mem *resp_sol);

	//funciones del pcp
	int32_t ingresar_new_cpu(int32_t listener_cpu);
	void administrar_men_cpus(int32_t soc_cpu);
	void conec_cerrada_cpu(int32_t soc_cpu);
	void manejador_sigusr1(int32_t soc_cpu,t_men_comun *men_cpu);
	void enviar_IO(int32_t soc_cpu, t_men_comun *men_cpu);
	void fin_ejecucion(int32_t tipo,int32_t socket_cpu, t_men_comun *men_cpu);
	void imprimir_valor(int32_t soc, t_men_comun *men_imp_valor);
	void imprimir_texto(int32_t soc, t_men_comun *men_imp_texto);
	void obtener_valor_compartida(int32_t soc_cpu, t_men_comun *men_obt_valor);
	void grabar_valor_compartida(int32_t soc_cpu, t_men_comun *men_gra_valor);
	void fin_quantum(int32_t soc_cpu, t_men_comun *men_cpu);
	void signal(int32_t soc_cpu,t_men_comun *men_cpu);
	void wait(int32_t soc_cpu,t_men_comun *men_cpu);

	void llamada_erronea(int32_t soc_cpu,int32_t tipo_error);

	//funciones cpu
	t_cpu *get_cpu_soc_cpu_remove(int32_t soc_cpu);
	t_cpu *get_cpu(int32_t  soc_cpu);

	//funciones pcb
	int32_t calcular_peso(t_men_comun *men_cod_prog);
	t_pcb_otros *get_peso_min();

	void umv_destrui_pcb(int32_t id_pcb);
	void enviar_cpu_pcb_destruir(int32_t  soc,t_pcb *pcb);
	void recv_actualizar_pcb(int32_t soc_cpu, t_pcb *pcb_actualizar);

	t_pcb_otros *buscar_pcb(t_queue *cola, int32_t soc_prog);
	t_pcb_otros *get_pcb_otros_exec(int32_t  id_proc);

	void moverAblock(t_pcb_otros *pcb_peso);
	void mover_pcb_exit(int32_t soc_prog);
	void pasar_pcb_exit(t_pcb_otros *pcb);
	void pasar_pcbBlock_ready(int32_t id_pcb);

	int32_t actualizar_pcb_y_bloq(t_cpu *aux_cpu);

	//menu imprimir
	void menu_imp();
	void imp_colas();
	void cambiar_multiprog(int un_int);

	void logear_int(FILE* destino,int32_t un_int);

	void lock_todo();
	void unlock_todo();

	void finalizo_ejecucion();
	void limpiar_destruir_dic_var();
	void limpiar_destruir_dic_sem();
	void limpiar_destruir_dic_io();

	void socket_select(int32_t descriptor_max, fd_set *read_fds);

#endif /* KERNEL_H_ */
