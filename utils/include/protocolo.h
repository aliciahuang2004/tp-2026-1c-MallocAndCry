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

    //SYSCALL
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOCK,
    MEM_ALLOC,
    MEM_FREE,
    SLEEP,
    STDOUT,
    STDIN,
    INIT_PROC,
    EXIT_PROC,
    SYSCALL_IO,
    // Operaciones de IO
    IO_REQUEST,  // Solicitud de operación de IO de KS a IO
    EJECUTAR_TAREA,
    IO_OK,       // Confirmación de finalización de IO al KS
    PROCESO_A_PROCESAR, //KS -> CPU: ejecutá este PID
    REQUEST_CONTEXTO, //CPU -> KM: dame el contexto de este PID
    CONTEXT_RESPONSE, // KM -> CPU: acá tenes el contexto 

    CREACION_DE_PROCESO,   
    PETICION_INSTRUCCION,   // CPU lo manda para pedir el codigo
    RESPUESTA_INSTRUCCION,  // KM lo manda para devolver el string
    ERROR_INSTRUCCION,
    CREACION_DE_PROCESO_OK,
    CREACION_DE_PROCESO_ERROR,
    CONTEXT_ERROR,           //KM responde esto cuando no encuentra el CTX solicitado
    NUEVO_MEMORY_STICK,
    ESCRITURA_DE_DATOS,              //KS envía una serie de bytes y dir logica a KM para que MS lo escriba en sus segmentos
    LECTURA_DE_DATOS,
    FINALIZAR_PROCESO,
    SUSPENSION_DE_PROCESO,
    DESUSPENSION_DE_PROCESO,
    ELIMINACION_DE_SEGMENTO,
    CREACION_DE_SEGMENTO,       //ks envia este protocolo a km para que km pueda crear el segmento 
    ACTUALIZAR_CONTEXTO,            
    PROCESO_DESALOJADO,
    PROCESO_DESALOJADO_QUANTUM,
    PROCESO_DESALOJADO_PRIORIDAD,
    PROCESO_DESALOJADO_COMPACTACION,
    CORRUPCION_MEMORIA,                  //km envia esto a ks para avisar que un ms se desconectó            
    INICIAR_COMPACTACION,
    CPUS_DESALOJADAS,
    AUMENTO_DE_MEMORIA,
    SEG_MAX_SIZE,
    MS_NUEVO_CPU          
}op_code;

#endif /* PROTOCOLO_H_ */
