#include "swap.h"

int main(int argc, char* argv[]) {
    printf(" inicia");

   t_swap* swap = inicializar_swap(argc,argv);
   
   verificar_swap(swap);
   
    if(conectar_a_kernel_memory(swap) == -1){
        log_error(swap->logger, "No se pudo conectar a Kernel Memory. Terminando programa.");
        liberar_swap(swap);
        return -1;
    }   

    enviar_handshake(swap);
    enviar_tamanio_bloque(swap);

    liberar_swap(swap);

    // saludar("swap");
    return 0;
}

