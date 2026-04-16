#include "cpu.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: ./bin/cpu [Archivo Config] [Identificador]\n");
        return EXIT_FAILURE;
    }

    t_cpu* cpu = iniciar_cpu(argv[1], argv[2]);

    if (conectar_kernel_memory(cpu) == -1) {
        log_error(cpu->logger, "Error en la conexion con Kernel Memory. Terminando programa."); 
        liberar_cpu(cpu);
        return -1;
    }

    if (conectar_kernel_scheduler(cpu) == -1) {
        log_error(cpu->logger, "Error en la conexion con kernel scheduler. Terminando programa.");
        liberar_cpu(cpu);
        return -1;
    }

    log_info(cpu->logger, "## CPU %s inicializada y conectada exitosamente", cpu->id);

    liberar_cpu(cpu);
    
    return 0;
}