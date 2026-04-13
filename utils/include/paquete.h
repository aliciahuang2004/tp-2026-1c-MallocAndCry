#ifndef PAQUETE_H_
#define PAQUETE_H_

/* ============================= INCLUDES ============================= */
#include <stdlib.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <readline/readline.h>
#include "protocolo.h"

/* ============================ ESTRUCTURAS ============================ */

/**
 * @struct t_buffer
 * @brief Estructura que representa un buffer de datos para comunicación
 * 
 * @var t_buffer::size
 *      Tamaño total del buffer en bytes
 * @var t_buffer::stream
 *      Puntero al stream de datos
 */
typedef struct {
    int     size;   /**< Tamaño del buffer en bytes */
    void*   stream; /**< Puntero al stream de datos */
} t_buffer;

/**
 * @struct t_paquete  
 * @brief Estructura que representa un paquete de comunicación
 * 
 * @var t_paquete::codigo_operacion
 *      Código de operación que identifica el tipo de paquete
 * @var t_paquete::buffer
 *      Buffer que contiene los datos del paquete
 */
typedef struct {
    op_code     codigo_operacion; /**< Código de operación del paquete */
    t_buffer*   buffer;           /**< Buffer con los datos del paquete */
} t_paquete;

/* ======================== FUNCIONES PÚBLICAS ======================== */

// Gestión de buffers
t_buffer*    crear_buffer(void);

// Gestión de paquetes
t_paquete*   crear_paquete(op_code code, t_buffer* buffer);
void         agregar_a_paquete(t_paquete* packet, void* stream, int size);
void*        serializar_paquete(t_paquete* packet, int bytes, t_log* logger);
int         enviar_paquete(t_paquete* packet, int client_socket, t_log* logger);
void         eliminar_paquete(t_paquete* packet);

// Funciones del lado del servidor
t_list*      recibir_paquete(int socket);
void         recibir_mensaje(int socket);
void*        recibir_buffer(int* size, int socket_cliente);

#endif /* PAQUETE_H_ */