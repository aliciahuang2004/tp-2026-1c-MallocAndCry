#include "io.h"

int main(int argc, char* argv[]) {

    if(argc !=3 ){
        printf("Uso: ./bin/io [Archivo Config] [Tipo]");
        return 1;
    }

    char *archivo_config = argv[1];
    char *tipo_io = argv[2];

    t_io* io=inicializar_io(archivo_config, tipo_io);

    verificar_io(io);

    if(conectar_a_kernel_scheduler(io) == -1){
        log_error(io->logger, "No se pudo conectar a Kernel Scheduler. Terminando programa.");
        liberar_io(io);
        return -1;
    }

    enviar_handshake(io);

    liberar_io(io);
    // saludar("io");
    return 0;
}
