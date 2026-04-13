#include "paquete.h"
#include "protocolo.h"
#include <string.h>   // Para memcpy
#include <sys/socket.h> // Para send
#include <commons/collections/list.h>

//Buffer
t_buffer *crear_buffer()
{
    t_buffer *buffer = malloc(sizeof(t_buffer));

    buffer->size = 0; 
    buffer->stream = NULL;

    return buffer;
}

//Paquete
t_paquete *crear_paquete(op_code code, t_buffer *buffer)
{
    t_paquete *packet = malloc(sizeof(t_paquete));

    packet->codigo_operacion = code;
    packet->buffer = buffer;

    return packet;
}

//Aniadir al paquete
void agregar_a_paquete(t_paquete *packet, void *stream, int size)
{
    packet->buffer->stream = realloc(packet->buffer->stream, packet->buffer->size + size + sizeof(int));

    memcpy(packet->buffer->stream + packet->buffer->size, &size, sizeof(int));
    memcpy(packet->buffer->stream + packet->buffer->size + sizeof(int), stream, size);

    packet->buffer->size += size + sizeof(int);
}

//Serializar paquete
void* serializar_paquete(t_paquete* paquete, int bytes, t_log* logger)
{
	if (!paquete || !paquete->buffer) {
        log_error(logger, "Intento de serializar paquete nulo");
        return NULL;
    }

	void * magic = malloc(bytes);
	if (!magic) {
        log_error(logger, "Malloc falló en serializar_paquete");
        return NULL;
    }

	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

//Enviar paquete
int enviar_paquete(t_paquete* paquete, int socket_cliente, t_log* logger)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes, logger);

	int resultado = send(socket_cliente, a_enviar, bytes, 0);
	if (resultado <= 0) {
            if (resultado == -1) {
                log_warning(logger, "Error al enviar (socket %d)", socket_cliente);
            } else {
                log_warning(logger, "Socket %d cerrado durante envío", socket_cliente);
            }
            return -1; // fallo
        }
	
	free(a_enviar);
	return 0;

	//free(a_enviar);
}

//eliminarPaquete
void eliminar_paquete(t_paquete *packet)
{
	if(packet != NULL)
	{
    	free(packet->buffer->stream);
    	free(packet->buffer);
    	free(packet);
	}
}

//Paquete del lado del SERVIDOR

t_list* recibir_paquete(int socket_cliente)
{
    int size;
    int desplazamiento = 0;
    int cod_op;
    void * buffer = NULL;
    t_list* valores = list_create();
    int tamanio;
    
    // Recibir codigo de operacion
    int detec = recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL);
    if (detec <= 0) {
        list_destroy(valores);
        return NULL;
    }
    
    int* cod_op_ptr = malloc(sizeof(int));
    *cod_op_ptr = cod_op;
    list_add(valores, cod_op_ptr);
    
    // Recibir buffer
    buffer = recibir_buffer(&size, socket_cliente);
    if (size < 0) {  // Error en la recepcion
        list_destroy_and_destroy_elements(valores, free);
        return NULL;
    }
    
    // Procesar solo si hay datos en el buffer
    if (buffer != NULL && size > 0) {
        while(desplazamiento < size) {
            memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
            desplazamiento += sizeof(int);
            char* valor = malloc(tamanio);
            memcpy(valor, buffer + desplazamiento, tamanio);
            list_add(valores, valor);
            desplazamiento += tamanio;
        }
        free(buffer);
    }
    
    return valores;
}


void* recibir_buffer(int* size, int socket_cliente) 
{
    void * buffer = NULL;
    
    // Recibir el tamanio del buffer
    if (recv(socket_cliente, size, sizeof(int), MSG_WAITALL) <= 0) {
        *size = -1;  // Indica error
        return NULL;
    }
    
    // Si el tamanio es valido y mayor que cero
    if (*size > 0) {
        buffer = malloc(*size);
        if (recv(socket_cliente, buffer, *size, MSG_WAITALL) != *size) {
            free(buffer);
            *size = -1;  // Indicar error
            return NULL;
        }
    }
    // Si size es cero, devolver NULL pero sin error
    return buffer;
}