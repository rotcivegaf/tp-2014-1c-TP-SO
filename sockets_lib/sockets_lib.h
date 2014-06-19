#ifndef SOCKETS_LIB_H_
#define SOCKETS_LIB_H_
	#include <sys/socket.h>
	#include <netdb.h>
	#include <sys/types.h>
	#include <unistd.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <stdint.h>
	#include <string.h>
	#include <commons/string.h>

	//tipos de mensajes
	typedef enum {
		CONEC_CERRADA = 0,
		CPU_DESCONEC = 2,
		//handshakes
		HS_KERNEL_UMV = 10,
		HS_KERNEL_PROG = 11,
		HS_KERNEL_CPU = 12,
		HS_UMV_CPU =13,
		IMPRIMIR_VALOR = 21,
		IMPRIMIR_TEXTO =22,
		FIN_EJECUCION = 23,
		//pedido de segmentos
		MEM_INSUFICIENTE = 29,
		RESP_MEM_SEG_COD =30,
		PED_MEM_IND_ETI =31,
		RESP_MEM_IND_ETI = 32,
		PED_MEM_IND_COD = 33,
		RESP_MEM_IND_COD = 34,
		PED_MEM_SEG_STACK = 35,
		RESP_MEM_SEG_STACK = 36,
		//almacenamiento de segmentos
		CODIGO_SCRIPT = 37,
		IND_ETI_FUNC = 38,
		IND_COD = 39,
		IND_STACK = 40,
		//mando pcb
		QUANTUM_MAX = 50,
		CANT_VAR_CONTEXTO_ACTUAL = 51,
		DPBU_CONTEXTO_ACTUAL = 52,
		DPBU_INDICE_CODIGO= 53,
		DPBU_INDICE_ETIQUETAS = 54,
		DPBU_SEGMENTO_CODIGO= 55,
		DPBU_SEGMENTO_STACK= 56,
		ID_PROG = 57,
		PROGRAM_COUNTER = 58,
		TAM_INDICE_ETIQUETAS = 59,
		DESTR_SEGS = 61,
		IO_ID = 62,
		IO_CANT_UNIDADES=63,
		SIGNAL=64,
		WAIT=65,
		PROX_INSTRUCCION = 66,
		ID_PROG_INI = 100
	} tipo_datos;

	int socket_crear_server(char *puerto);
	int socket_accept(int socket_server);
	int socket_crear_client(char *puerto, char *ip);
	void socket_cerrar(int s);

	//para los mensajes comunes
	typedef struct{
		int32_t tam_dato;
		int32_t tipo;
		char *dato;
	}__attribute__((__packed__)) t_men_comun;

	t_men_comun *crear_men_comun(int32_t tipo, char *dato,int32_t tam);
	int socket_send_comun(int soc,t_men_comun *men);
	t_men_comun *socket_recv_comun(int soc);
	char *men_serealizer_comun(t_men_comun *self);
	t_men_comun *men_deserealizer_comun(char *stream);

	//para los mensajes que contienen el pcb + quantum
	typedef struct{
		int32_t cant_var_contexto_actual;
		int32_t dir_primer_byte_umv_contexto_actual;
		int32_t dir_primer_byte_umv_indice_codigo;
		int32_t dir_primer_byte_umv_indice_etiquetas;
		int32_t dir_primer_byte_umv_segmento_codigo;
		int32_t dir_primer_byte_umv_segmento_stack;
		int32_t id;
		int32_t program_counter;
		int32_t tam_indice_etiquetas;
	}__attribute__((__packed__)) t_pcb;
	typedef struct{
		int32_t tipo;
		int32_t quantum;
		t_pcb *pcb;
	}__attribute__((__packed__)) t_men_quantum_pcb;

	t_men_quantum_pcb *crear_men_quantum_pcb(int32_t tipo, int32_t quantum, t_pcb* pcb);
	int socket_send_quantum_pcb(int soc,t_men_quantum_pcb *men);
	t_men_quantum_pcb *socket_recv_quantum_pcb(int soc);
	char *men_serealizer_quantum_pcb(t_men_quantum_pcb *self);
	t_men_quantum_pcb *men_deserealizer_quantum_pcb(char *stream);

	//para los mensaje que solicitan bytes de la umv
	typedef struct{
		int32_t tipo;
		int32_t base;
		int32_t offset;
		int32_t tam;
	}__attribute__((__packed__)) t_men_sol_pos_mem;

	t_men_sol_pos_mem *crear_men_sol_pos_mem(int32_t tipo, int32_t base, int32_t offset, int32_t tam);
	int socket_send_sol_pos_mem(int soc,t_men_sol_pos_mem *men);
	t_men_sol_pos_mem *socket_recv_sol_pos_mem(int soc);
	char *men_serealizer_sol_pos_mem(t_men_sol_pos_mem *self);
	t_men_sol_pos_mem *men_deserealizer_sol_pos_mem(char *stream);

	//para los mensaje que envian bytes para ser almacenados en la umv
	typedef struct{
		int32_t tipo;
		int32_t base;
		int32_t offset;
		int32_t tam;
		char *buffer;
	}__attribute__((__packed__)) t_men_sol_alm_bytes;

	t_men_sol_alm_bytes *crear_men_sol_alm_bytes(int32_t tipo, int32_t base, int32_t offset, int32_t tam, char *buffer);
	int socket_send_sol_alm_bytes(int soc,t_men_sol_alm_bytes *men);
	t_men_sol_alm_bytes *socket_recv_sol_alm_bytes(int soc);
	char *men_serealizer_sol_alm_bytes(t_men_sol_alm_bytes *self);
	t_men_sol_alm_bytes *men_deserealizer_sol_alm_bytes(char *stream);

#endif
