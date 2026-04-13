#include <utils/hello.h>

void saludar(char* quien) {
    printf("Hola desde %s!!\n", quien);
}


/*Kernel memory -> server  
    PUERTO_ESCUCHA: 9001

Kernel Scheduler -> server/client
    PUERTO_ESCUCHA: 9002
    IP_KERNEL_MEMORY: 127.0.0.1
    PUERTO_KERNEL_MEMORY: 9001

Memory stick -> client/server
    PUERTO_ESCUCHA:9003
    IP_KERNEL_MEMORY: 127.0.0.1
    PUERTO_KERNEL_MEMORY: 9001

CPus -> client
    PUERTO_ESCUCHA:9004
    IP_KERNEL_SCHEDULER:127.0.0.1
    PUERTO_KERNEL_SCHEDULER: 9002
    IP_KERNEL_MEMORY:127.0.0.1
    PUERTO_KERNEL_MEMORY: 9001
    IP_MEMORY_STICK:127.0.0.1
    PUERTO_MEMORY_STICK: 9003

IO -> client
    IP_KERNEL_SCHEDULER:127.0.0.1
    PUERTO_KERNEL_SCHEDULER: 9002

SWAP -> client
    IP_KERNEL_MEMORY:127.0.0.1
    PUERTO_KERNEL_MEMORY: 9001
*/