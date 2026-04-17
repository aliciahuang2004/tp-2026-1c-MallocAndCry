#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdint.h>
#include <commons/string.h>

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
    KERNEL_MEMORY_HANDSHAKE
}op_code;

#endif /* PROTOCOLO_H_ */
