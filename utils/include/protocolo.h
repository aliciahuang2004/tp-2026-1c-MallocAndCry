#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdint.h>
#include <commons/string.h>

typedef enum{

    SWAP_HANDSHAKE, // para el handshake inicial entre swap y kernel memory
    SWAP_REQUEST, // para enviar el tamaño de bloque y total a memoria
    CPU_KERNEL_MEMORY_HANDSHAKE,    
    CPU_KERNEL_SCHEDULER_HANDSHAKE
}op_code;

#endif /* PROTOCOLO_H_ */