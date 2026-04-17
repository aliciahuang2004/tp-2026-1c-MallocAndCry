#include <utils/hello.h>

int main(int argc, char* argv[]) {

    if(argc !=3 ){
        printf("Uso: ./bin/io [Archivo Config] [Tipo]");
        return 1;
    }

    char *archivo_config = argv[1];
    char *tipo_io = argv[2];

    t_io* io=inicializar_io(archivo_config, tipo_io);


    saludar("io");
    return 0;
}
