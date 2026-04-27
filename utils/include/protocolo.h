#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdint.h>
#include <commons/string.h>

typedef enum {
    OP_SLEEP,
    OP_STDIN,
    OP_STDOUT
} t_io_operation;

typedef enum{

    SWAP_HANDSHAKE, // para el handshake inicial entre swap y kernel memory
    SWAP_REQUEST, // para enviar el tamaño de bloque y total a memoria
    //HANDSHAKE PARA KERNEL MEMORY Y EL MODULO CORRESPONDIENTE
    KERNEL_SCHEDULER_HANDSHAKE,
    MEMORY_STICK_HANDSHAKE,
    CPU_HANDSHAKE,
    
    IO_HANDSHAKE,
    
    //handshakes para conexiones:ms-km,ms-cpu
    MS_HANDSHAKE,
    KERNEL_MEMORY_HANDSHAKE,

    // Operaciones de IO
    IO_REQUEST,  // Solicitud de operación de IO de KS a IO
    IO_OK,       // Confirmación de finalización de IO al KS
    PROCESO_A_PROCESAR, //KS -> CPU: ejecutá este PID
    REQUEST_CONTEXTO, //CPU -> KM: dame el contexto de este PID
    CONTEXT_RESPONSE, // KM -> CPU: acá tenes el contexto 

    CREACION_DE_PROCESO,
    PETICION_INSTRUCCION,   // CPU lo manda para pedir el codigo
    RESPUESTA_INSTRUCCION,  // KM lo manda para devolver el string
    ERROR_INSTRUCCION,
    INIT_PROC_OK,
    INIT_PROC_ERROR
}op_code;

#endif /* PROTOCOLO_H_ */
