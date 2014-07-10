/*agregar en el .bashrc
 * export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/workspace/tp-2014-1c-hashtaggers/sockets_lib/Debug
 */

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

	//tipos de mensajes
	typedef enum {
		CONEC_CERRADA = 0,
		//handshakes
		HS_KERNEL = 10,
		HS_PROG = 11,
		HS_CPU = 12,
		HS_UMV =13,

		//mensajes q envia el cpu al kernel
		FIN_QUANTUM=20,
		CPU_DESCONEC = 21,
		IMPRIMIR_VALOR = 22,
		IMPRIMIR_TEXTO =23,
		FIN_EJECUCION = 24,
		OBTENER_VALOR = 25,
		GRABAR_VALOR = 26,
		VALOR_ASIGNADO =27,
		SEM_BLOQUEADO = 28,
		SEM_OK = 29,
		//mensajes q envia el kernel a la cpu
		R_IMPRIMIR = 50,
		R_OBTENER_VALOR = 52,
		R_GRABAR_VALOR = 53,
		R_VALOR_ASIGNADO =54,
		R_SEM_BLOQUEADO = 55,
		SINCRO_OK = 61,
		ID_PROG = 62,
		IO_ID = 63,
		IO_CANT_UNIDADES=64,
		SIGNAL=65,
		WAIT=66,
		PROX_INSTRUCCION =67,
		PCB_Y_QUANTUM = 68,
		VAR_INEX = 69,
		SEM_INEX = 70,

		//mensajes recibidos por la UMV departe del KERNEL
		PED_MEM_SEG_COD = 200,
		PED_MEM_IND_ETI = 201,
		PED_MEM_IND_COD = 202,
		PED_MEM_SEG_STACK = 203,
		DESTR_SEGS = 204,
		ESCRIBIR_SEG = 205,
		CODIGO_SCRIPT = 206,
		IND_ETI_FUNC = 207,
		IND_COD = 208,
		IND_STACK = 209,
		//mensajes enviados por la UMV al KERNEL
		RESP_MEM_SEG_COD = 250,
		RESP_MEM_IND_ETI = 251,
		RESP_MEM_IND_COD = 252,
		RESP_MEM_SEG_STACK = 253,
		MEN_ALM_OK = 254,
		//mensajes recibidos por la UMV departe del CPU
		SOL_BYTES = 300,
		ALM_BYTES = 301,
		CAMBIO_PA = 302,
		//mensajes enviados por la UMV al CPU
		R_SOL_BYTES = 350,
		R_ALM_BYTES = 351,
		MEM_OVERLOAD = 352,		//tambien es enviado al kernel
		SEGMEN_FAULT = 353,
		QUIT_SISTEMA_UMV = 500
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
	void enviar_men_comun_destruir(int32_t soc, int32_t tipo, char *dato, int32_t tam);
	t_men_comun *socket_recv_comun(int soc);
	char *men_serealizer_comun(t_men_comun *self);
	t_men_comun *men_deserealizer_comun(char *stream);
	void destruir_men_comun(t_men_comun *men_comun);

	//para los mensajes que contienen el pcb + quantum
	typedef int32_t  t_dir_mem;
	typedef struct{
		int32_t cant_var_contexto_actual;
		t_dir_mem dir_primer_byte_umv_contexto_actual;
		t_dir_mem dir_primer_byte_umv_indice_codigo;
		t_dir_mem dir_primer_byte_umv_indice_etiquetas;
		t_dir_mem dir_primer_byte_umv_segmento_codigo;
		t_dir_mem dir_primer_byte_umv_segmento_stack;
		int32_t id;
		int32_t program_counter;
		int32_t tam_indice_etiquetas;
		int32_t cant_instrucciones;
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
	void destruir_quantum_pcb(t_men_quantum_pcb *men_pcb);

	//para los mensaje que solicitan/almacenan bytes en la umv

	typedef struct{
		int32_t tipo;
		int32_t base;
		int32_t offset;
		int32_t tam;
		char *buffer;
	}__attribute__((__packed__)) t_men_cpu_umv;

	t_men_cpu_umv *crear_men_cpu_umv(int32_t tipo, int32_t base, int32_t offset, int32_t tam, char *buffer);
	int socket_send_cpu_umv(int soc,t_men_cpu_umv *men);
	t_men_cpu_umv *socket_recv_cpu_umv(int soc);
	char *men_serealizer_cpu_umv(t_men_cpu_umv *self);
	t_men_cpu_umv *men_deserealizer_cpu_umv(char *stream);
	void destruir_men_cpu_umv(t_men_cpu_umv *men_sol);

	typedef struct{
		int32_t tipo;
		int32_t id_prog;
		int32_t tam_seg;
	}__attribute__((__packed__)) t_men_seg;

	t_men_seg *crear_men_seg(int32_t tipo, int32_t id_prog, int32_t tam_seg);
	int socket_send_seg(int soc,t_men_seg *men);
	void enviar_umv_mem_seg_destruir(int32_t soc, int32_t tipo_men,int32_t id_prog,int32_t tam_seg);
	t_men_seg *socket_recv_seg(int soc);
	char *men_serealizer_seg(t_men_seg *self);
	t_men_seg *men_deserealizer_seg(char *stream);
	void destruir_men_seg(t_men_seg *men_seg);

#endif
