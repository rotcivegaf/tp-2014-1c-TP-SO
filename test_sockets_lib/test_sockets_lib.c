#include "sockets_lib.h"

int main(){
	int i;
	//abro server
	int soc_escucha = socket_crear_server("6667");
	//creo un cliente
	int soc_envio = socket_crear_client("6667", "127.0.0.1");
	//el server acepta la coneccion del cliente
	int soc_mensaje = socket_accept(soc_escucha);

	//test enviar un mensaje comun vacio

	//el cliente le envia un mensaje vacio al server
	t_men_comun *men_comun = crear_men_comun(CONEC_CERRADA, NULL, 0);
	socket_send_comun(soc_envio,men_comun);
	destruir_men_comun(men_comun);


	//el server recibe un mensaje comun vacio
	t_men_comun *men_comun_vacio = socket_recv_comun(soc_mensaje);
	printf(	"		|Espero | Recibo\n");
	printf(	"Mensaje comun vacio\n"
			"	Tipo    |  0    |  %i\n"
			"	Tam_dato|  0    |  %i\n"
			"	Dato    |(null) | %s\n",men_comun_vacio->tipo,men_comun_vacio->tam_dato,men_comun_vacio->dato);
	destruir_men_comun(men_comun_vacio);

	//test recibir un mensaje comun con un char* con \0

	//el cliente le envia un mensaje comun con un char* con \0 al server
	char *dato=malloc(3);
	dato[0] = 1;
	dato[1] = '\0';
	dato[2] = 4;
	t_men_comun *men_comun_con_datos = crear_men_comun(10, dato, 3);
	free(dato);
	socket_send_comun(soc_envio,men_comun_con_datos);
	destruir_men_comun(men_comun_con_datos);

	//el server recibe un mensaje comun con un char* con \0
	t_men_comun *men_comun_con_barra_0 = socket_recv_comun(soc_mensaje);
	printf(	"Mensaje comun con barra 0\n"
			"	Tipo    |  10   |  %i\n"
			"	Tam_dato|   3   |  %i\n",men_comun_con_barra_0->tipo,men_comun_con_barra_0->tam_dato);
	printf(	"	Dato    | 1-0-4-| ");
	for (i=0;i<men_comun_con_barra_0->tam_dato;i++){
		printf("%i-",men_comun_con_barra_0->dato[i]);
	}
	printf("\n");
	destruir_men_comun(men_comun_con_barra_0);

	//test recibir un mensaje quantum pcb

	//el cliente le envia un mensaje quantum pcb
    t_pcb *pcb = malloc(sizeof (t_pcb));
    pcb->cant_var_contexto_actual = 3;
    pcb->dir_primer_byte_umv_contexto_actual = 4;
    pcb->dir_primer_byte_umv_indice_codigo = 5;
    pcb->dir_primer_byte_umv_indice_etiquetas = 6;
    pcb->dir_primer_byte_umv_segmento_codigo = 7;
    pcb->dir_primer_byte_umv_segmento_stack = 8;
    pcb->id = 9;
    pcb->program_counter = 10;
    pcb->tam_indice_etiquetas = 11;
    t_men_quantum_pcb *men_pcb = crear_men_quantum_pcb(1, 2, pcb);
    free(pcb);
	socket_send_quantum_pcb(soc_envio,men_pcb);
	destruir_quantum_pcb(men_pcb);

	//el server recibe un mensaje quantum pcb
	men_pcb = socket_recv_quantum_pcb(soc_mensaje);
	printf(	"Mensaje quatum_pcb\n"
			"	Tipo    |  1   |  %i\n"
			"	Quantum |  2   |  %i\n",men_pcb->tipo,men_pcb->quantum);
	printf(	"	Dato    | 3-4-5-6-7-8-9-10-11|%i-%i-%i-%i-%i-%i-%i-%i-%i",
			men_pcb->pcb->cant_var_contexto_actual,men_pcb->pcb->dir_primer_byte_umv_contexto_actual,
			men_pcb->pcb->dir_primer_byte_umv_indice_codigo,men_pcb->pcb->dir_primer_byte_umv_indice_etiquetas ,
			men_pcb->pcb->dir_primer_byte_umv_segmento_codigo ,men_pcb->pcb->dir_primer_byte_umv_segmento_stack ,
			men_pcb->pcb->id ,men_pcb->pcb->program_counter ,men_pcb->pcb->tam_indice_etiquetas);
	printf("\n");
	destruir_quantum_pcb(men_pcb);

	//test enviar un mensaje solicitud de memoria

	//el cliente le envia un mensaje de solicitud de memoria al server
	t_men_cpu_umv *men_sol = crear_men_cpu_umv(5, 6, 7, 8,NULL);
	socket_send_cpu_umv(soc_envio,men_sol);
	destruir_men_cpu_umv(men_sol);

	//el server recibe un mensaje solicitud de memoria
	men_sol = socket_recv_cpu_umv(soc_mensaje);
	printf(	"Mensaje solicitud de memoria\n"
			"	Tipo    |  5   |  %i\n"
			"	Base    |  6   |  %i\n"
			"	Offset  |  7   |  %i\n"
			"	Tamanio |  0   |  %i\n",men_sol->tipo,men_sol->base,men_sol->offset,men_sol->tam);
	destruir_men_cpu_umv(men_sol);

	//test enviar un mensaje solicitud de almacenamiento en  memoria

	//el cliente le envia un mensaje solicitud de almacenamiento en  memoria
	char *dato2=malloc(3);
	dato2[0] = 5;
	dato2[1] = '\0';
	dato2[2] = 1;
	t_men_cpu_umv *men_alm_bytes = crear_men_cpu_umv(1, 2, 2, 3, dato2);
	free(dato2);
	socket_send_cpu_umv(soc_envio,men_alm_bytes);
	destruir_men_cpu_umv(men_alm_bytes);

	//el server recibe un mensaje solicitud de almacenamiento en  memoria
	men_alm_bytes  = socket_recv_cpu_umv(soc_mensaje);
	printf(	"Mensaje almacenar de memoria\n"
			"	Tipo    |  1   |  %i\n"
			"	Base    |  2   |  %i\n"
			"	Offset  |  2   |  %i\n"
			"	Tamanio |  3   |  %i\n",men_alm_bytes->tipo,men_alm_bytes->base,men_alm_bytes->offset,men_alm_bytes->tam);
	printf(	"	Buffer  | 5-0-1-| ");
	for (i=0;i<men_alm_bytes->tam;i++){
		printf("%i-",men_alm_bytes->buffer[i]);
	}
	printf("\n");
	destruir_men_cpu_umv(men_alm_bytes);

	//test enviar un mensaje para crear segmentos

	//el cliente le envia un mensaje t_men_seg
	t_men_seg *men_crear = crear_men_seg(1, 2, 100);
	socket_send_seg(soc_envio,men_crear);
	destruir_men_seg(men_crear);

	//el server recibe un mensaje t_men_seg
	men_crear = socket_recv_seg(soc_mensaje);
	printf(	"Mensaje crear segmento\n"
			"	Tipo    |  1   |  %i\n"
			"	Id_prog |  2   |  %i\n"
			"	Tamanio |  100 |  %i\n",men_crear->tipo,men_crear->id_prog,men_crear->tam_seg);
	destruir_men_seg(men_crear);

	//test enviar un mensaje para destruir segmentos

	//el cliente le envia un mensaje t_men_seg
	t_men_seg *men_dest = crear_men_seg(2, 4, 0);
	socket_send_seg(soc_envio,men_dest);
	destruir_men_seg(men_dest);

	//el server recibe un mensaje t_men_seg
	men_dest = socket_recv_seg(soc_mensaje);
	printf(	"Mensaje destruir segmento\n"
			"	Tipo    |  2   |  %i\n"
			"	Id_prog |  4   |  %i\n"
			"	Tamanio |  0   |  %i\n",men_dest->tipo,men_dest->id_prog,men_dest->tam_seg);
	destruir_men_seg(men_dest);

	socket_cerrar(soc_envio);
	men_dest = socket_recv_seg(soc_mensaje);
	destruir_men_seg(men_dest);

	socket_cerrar(soc_escucha);
	socket_cerrar(soc_mensaje);
	return 0;
}
