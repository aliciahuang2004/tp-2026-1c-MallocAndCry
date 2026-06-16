#include "instrucciones.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


bool es_registro_8bits(char* nombre_registro) {
    return (strcmp(nombre_registro, "AX") == 0 || 
            strcmp(nombre_registro, "BX") == 0 || 
            strcmp(nombre_registro, "CX") == 0 || 
            strcmp(nombre_registro, "DX") == 0);
}

void* obtener_registro(t_registros* registros, char* nombre_registro) {
    if (strcmp(nombre_registro, "AX") == 0) return &(registros->AX);
    if (strcmp(nombre_registro, "BX") == 0) return &(registros->BX);
    if (strcmp(nombre_registro, "CX") == 0) return &(registros->CX);
    if (strcmp(nombre_registro, "DX") == 0) return &(registros->DX);
    
    if (strcmp(nombre_registro, "EAX") == 0) return &(registros->EAX);
    if (strcmp(nombre_registro, "EBX") == 0) return &(registros->EBX);
    if (strcmp(nombre_registro, "ECX") == 0) return &(registros->ECX);
    if (strcmp(nombre_registro, "EDX") == 0) return &(registros->EDX);
    
    if (strcmp(nombre_registro, "SI") == 0) return &(registros->SI);
    if (strcmp(nombre_registro, "DI") == 0) return &(registros->DI);
    if (strcmp(nombre_registro, "PC") == 0) return &(registros->PC);
    
    return NULL;
}

uint32_t leer_valor_registro(t_registros* registros, char* nombre_registro) {
    void* reg_ptr = obtener_registro(registros, nombre_registro);
    if (reg_ptr == NULL) return 0;
    
    if (es_registro_8bits(nombre_registro)) {
        return (uint32_t)(*(uint8_t*)reg_ptr);
    } else {
        return *(uint32_t*)reg_ptr;
    }
}

// NOOP
void ejecutar_NOOP(t_cpu* cpu, t_contexto* ctx) {
    usleep(1000 * config_get_int_value(cpu->config, "INSTRUCTION_DELAY"));
    log_debug(cpu->logger, "Ejecutando NOOP: Simulando retardo de instrucción");
}

// SET
void ejecutar_SET(t_cpu* cpu,t_registros* reg, char* registro_destino, uint32_t valor) {
    void* reg_ptr = obtener_registro(reg, registro_destino);
    if (reg_ptr != NULL) {
        if (es_registro_8bits(registro_destino)) {
            *(uint8_t*)reg_ptr = (uint8_t)valor;
        } else {
            *(uint32_t*)reg_ptr = valor;
        }
    }
    log_debug(cpu->logger, "Ejecutando SET: Asignando valor %u al registro %s", valor, registro_destino);
}

// SUM
void ejecutar_SUM(t_cpu* cpu,t_registros* reg, char* registro_destino, char* registro_origen) {
    uint32_t valor_origen = leer_valor_registro(reg, registro_origen);
    void* reg_dest_ptr = obtener_registro(reg, registro_destino);
    
    if (reg_dest_ptr != NULL) {
        if (es_registro_8bits(registro_destino)) {
            *(uint8_t*)reg_dest_ptr += (uint8_t)valor_origen;
        } else {
            *(uint32_t*)reg_dest_ptr += valor_origen;
        }
    }
    log_debug(cpu->logger, "Ejecutando SUM: Sumando valor de registro %s al registro %s", registro_origen, registro_destino);
}

// SUB
void ejecutar_SUB(t_cpu* cpu,t_registros* reg, char* registro_destino, char* registro_origen) {
   uint32_t valor_origen = leer_valor_registro(reg, registro_origen);
    void* reg_dest_ptr = obtener_registro(reg, registro_destino);
    
    if (reg_dest_ptr != NULL) {
        if (es_registro_8bits(registro_destino)) {
            *(uint8_t*)reg_dest_ptr -= (uint8_t)valor_origen;
        } else {
            *(uint32_t*)reg_dest_ptr -= valor_origen;
        }
    }
    log_debug(cpu->logger, "Ejecutando SUB: Restando valor de registro %s al registro %s", registro_origen, registro_destino);
}

// JNZ
void ejecutar_JNZ(t_cpu* cpu,t_registros* reg, char* registro_evaluado, uint32_t nueva_instruccion) {
    uint32_t valor = leer_valor_registro(reg, registro_evaluado);
    // Solo actualiza el PC si el registro evaluado es distinto de 0
    if (valor != 0) {
        reg->PC = nueva_instruccion;
    }
    log_debug(cpu->logger, "Ejecutando JNZ: Evaluando registro %s para saltar a instrucción %u", registro_evaluado, nueva_instruccion);
}

int obtener_socket_ms(t_cpu* cpu, int ms_id) {
    for(int i = 0; i < list_size(cpu->sockets_memory_sticks); i++) {
        t_ms_conectado* ms = list_get(cpu->sockets_memory_sticks, i);
        if(ms->id == ms_id) return ms->socket;
    }
    log_error(cpu->logger, "No se encontró un Memory Stick conectado con el ID %d", ms_id);
    return -1;
}

// MOV_IN:
int ejecutar_MOV_IN(t_cpu* cpu, t_contexto* ctx, char* registro_datos) {
    uint32_t dir_logica = ctx->registros.SI;
    uint32_t tamanio;

    if (es_registro_8bits(registro_datos) == true) {
        tamanio = 1;
    } else {
        tamanio = 4;
    }

    uint32_t dir_fisica = 0;
    int ms_id = 0;

    if (!mmu_traducir_direccion(cpu, ctx, dir_logica, tamanio, &dir_fisica, &ms_id)) return 0; 
    
    int socket_ms = obtener_socket_ms(cpu, ms_id);
    if (socket_ms == -1) return 0;

    t_paquete* paquete = crear_paquete(LECTURA_DE_DATOS, crear_buffer());
    agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    enviar_paquete(paquete, socket_ms, cpu->logger);
    eliminar_paquete(paquete);

    t_list* respuesta = recibir_paquete(socket_ms);
    int cod_op = *(int*)list_get(respuesta, 0);
    
    if (cod_op == DATOS_LEIDOS) {
        void* datos = list_get(respuesta, 1);
        
        void* reg_ptr = obtener_registro(&(ctx->registros), registro_datos);
        uint32_t valor_leido = 0;

        if (tamanio == 1) {
            *(uint8_t*)reg_ptr = *(uint8_t*)datos;
            valor_leido = *(uint8_t*)datos;
        } else {
            *(uint32_t*)reg_ptr = *(uint32_t*)datos;
            valor_leido = *(uint32_t*)datos;
        }
        
        log_info(cpu->logger, "PID: %d - Acción: LEER - Dirección Física: %u - Valor: %u", 
                 ctx->pid, dir_fisica, valor_leido);
    }
    
    list_destroy_and_destroy_elements(respuesta, free);
    return 1;
}

// MOV_OUT
int ejecutar_MOV_OUT(t_cpu* cpu, t_contexto* ctx, char* registro_datos) {
    uint32_t dir_logica = ctx->registros.DI;
    uint32_t tamanio;

    if (es_registro_8bits(registro_datos) == true) {
        tamanio = 1;
    } else {
        tamanio = 4;
    }

    uint32_t dir_fisica = 0;
    int ms_id = 0;

    if (!mmu_traducir_direccion(cpu, ctx, dir_logica, tamanio, &dir_fisica, &ms_id)) return 0;

    int socket_ms = obtener_socket_ms(cpu, ms_id);
    if (socket_ms == -1) return 0;

    uint32_t valor_a_escribir = leer_valor_registro(&(ctx->registros), registro_datos);

    t_paquete* paquete = crear_paquete(ESCRITURA_DE_DATOS, crear_buffer());
    agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    
    if (tamanio == 1) {
        uint8_t val8 = (uint8_t)valor_a_escribir;
        agregar_a_paquete(paquete, &val8, 1);
    } else {
        agregar_a_paquete(paquete, &valor_a_escribir, 4);
    }
    
    enviar_paquete(paquete, socket_ms, cpu->logger);
    eliminar_paquete(paquete);

    t_list* respuesta = recibir_paquete(socket_ms);
    int cod_op = *(int*)list_get(respuesta, 0);
    
    if (cod_op == IO_OK) {
         log_info(cpu->logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %u - Valor: %u", 
                 ctx->pid, dir_fisica, valor_a_escribir);
    }
    
    list_destroy_and_destroy_elements(respuesta, free);
    return 1;
}

// COPY_MEM
int ejecutar_COPY_MEM(t_cpu* cpu, t_contexto* ctx, char* registro_tamano) {
    uint32_t dir_logica_origen = ctx->registros.SI;
    uint32_t dir_logica_destino = ctx->registros.DI;
    uint32_t tamanio = leer_valor_registro(&(ctx->registros), registro_tamano);
    
    uint32_t dir_fisica_origen = 0, dir_fisica_destino = 0;
    int ms_id_origen = 0, ms_id_destino = 0;

    if (!mmu_traducir_direccion(cpu, ctx, dir_logica_origen, tamanio, &dir_fisica_origen, &ms_id_origen)) return 0;
    if (!mmu_traducir_direccion(cpu, ctx, dir_logica_destino, tamanio, &dir_fisica_destino, &ms_id_destino)) return 0;

    int socket_ms_origen = obtener_socket_ms(cpu, ms_id_origen);
    int socket_ms_destino = obtener_socket_ms(cpu, ms_id_destino);

    if (socket_ms_origen == -1 || socket_ms_destino == -1) return 0;

    t_paquete* paquete_leer = crear_paquete(LECTURA_DE_DATOS, crear_buffer());
    agregar_a_paquete(paquete_leer, &dir_fisica_origen, sizeof(uint32_t));
    agregar_a_paquete(paquete_leer, &tamanio, sizeof(int));
    enviar_paquete(paquete_leer, socket_ms_origen, cpu->logger);
    eliminar_paquete(paquete_leer);

    t_list* respuesta_lectura = recibir_paquete(socket_ms_origen);
    void* datos_leidos = list_get(respuesta_lectura, 1);
    
    log_info(cpu->logger, "PID: %d - Acción: LEER - Dirección Física: %u - Valor: <CONTENIDO_COPIADO>", ctx->pid, dir_fisica_origen);

    t_paquete* paquete_escribir = crear_paquete(ESCRITURA_DE_DATOS, crear_buffer());
    agregar_a_paquete(paquete_escribir, &dir_fisica_destino, sizeof(uint32_t));
    agregar_a_paquete(paquete_escribir, &tamanio, sizeof(int));
    agregar_a_paquete(paquete_escribir, datos_leidos, tamanio);
    enviar_paquete(paquete_escribir, socket_ms_destino, cpu->logger);
    eliminar_paquete(paquete_escribir);

    t_list* respuesta_escritura = recibir_paquete(socket_ms_destino);
    
    log_info(cpu->logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %u - Valor: <CONTENIDO_COPIADO>", ctx->pid, dir_fisica_destino);

    list_destroy_and_destroy_elements(respuesta_lectura, free);
    list_destroy_and_destroy_elements(respuesta_escritura, free);

    return 1;
}

void ejecutar_SYSCALL(t_cpu* cpu, t_contexto* ctx, t_instruccion_decodificada instruccion){
    log_info(cpu->logger, "Delegando SYSCALL %s al Kernel Scheduler", instruccion.nombre_operacion);

    // se guarda el contexto en memoria
    enviar_contexto_a_memoria(cpu, ctx);

    t_paquete* paquete= NULL;

    if (instruccion.identificador_operacion == INST_SLEEP) {
        paquete = crear_paquete(SLEEP, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        
        int tiempo = atoi(instruccion.argumento_operando_destino); // Convertimos el string a int
        agregar_a_paquete(paquete, &tiempo, sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_MUTEX_CREATE) {
        paquete = crear_paquete(MUTEX_CREATE, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1);
    }
    else if (instruccion.identificador_operacion == INST_MUTEX_LOCK) {
        paquete = crear_paquete(MUTEX_LOCK, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1);
    }
    else if (instruccion.identificador_operacion == INST_MUTEX_UNLOCK) {
        paquete = crear_paquete(MUTEX_UNLOCK, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1);
    }
    else if (instruccion.identificador_operacion == INST_INIT_PROC) {
        paquete = crear_paquete(INIT_PROC, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1); // Path
        
        int prioridad = atoi(instruccion.argumento_operando_origen); // Convertimos la prioridad a int
        agregar_a_paquete(paquete, &prioridad, sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_EXIT) {
        paquete = crear_paquete(EXIT_PROC, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_MEM_ALLOC) {
        paquete = crear_paquete(MEM_ALLOC, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
       
        int id_segmento = atoi(instruccion.argumento_operando_destino);
        int tamanio = atoi(instruccion.argumento_operando_origen);
        
        agregar_a_paquete(paquete, &id_segmento, sizeof(int));
        agregar_a_paquete(paquete, &tamanio, sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_MEM_FREE) {
        paquete = crear_paquete(MEM_FREE, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        
        // Un solo argumento numérico directo
        int id_segmento = atoi(instruccion.argumento_operando_destino);
        
        agregar_a_paquete(paquete, &id_segmento, sizeof(int));
    }
    else if (instruccion.identificador_operacion == INST_STDOUT) {
        paquete = crear_paquete(STDOUT, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        
        // nos manda los nombres de los registros: "AX" "BX"
        // tenemos que leer qué valor numerico tienen adentro antes de mandar el paquete
        uint32_t dir_logica = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_destino);
        uint32_t tamanio = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_origen);
        
        agregar_a_paquete(paquete, &dir_logica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    }
    else if (instruccion.identificador_operacion == INST_STDIN) {
        paquete = crear_paquete(STDIN, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        
        uint32_t dir_logica = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_destino);
        uint32_t tamanio = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_origen);
        
        agregar_a_paquete(paquete, &dir_logica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    }
    /*// avisamos al Scheduler que el proceso fue desalojado por una syscall
    // Usamos el codigo PROCESO_DESALOJADO y enviamos la Syscall y sus parametros para que el Kernel la procese
    t_paquete* paquete = crear_paquete(PROCESO_DESALOJADO, crear_buffer());
    agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
    agregar_a_paquete(paquete, instruccion.nombre_operacion, strlen(instruccion.nombre_operacion) + 1);
    

    // enviamos argumentos
    if (instruccion.argumento_operando_destino) {
        agregar_a_paquete(paquete, instruccion.argumento_operando_destino, strlen(instruccion.argumento_operando_destino) + 1);
    } else {
        agregar_a_paquete(paquete, "", 1);
    }
    
    if (instruccion.argumento_operando_origen) {
        agregar_a_paquete(paquete, instruccion.argumento_operando_origen, strlen(instruccion.argumento_operando_origen) + 1);
    } else {
        agregar_a_paquete(paquete, "", 1);
    }

*/
    if(paquete != NULL) {
        log_debug(cpu->logger, "Enviando paquete de SYSCALL %s al Kernel Scheduler", instruccion.nombre_operacion);
        enviar_paquete(paquete, cpu->socket_kernel_scheduler, cpu->logger);
        eliminar_paquete(paquete);
    }
    
}

bool mmu_traducir_direccion(t_cpu* cpu, t_contexto* ctx, uint32_t dir_logica, uint32_t tamano_a_operar, uint32_t* dir_fisica_out, int* ms_id_out) {
    
    uint32_t tamanio_segmento = (uint32_t)cpu->segment_max_size; 

    int num_segmento = dir_logica / tamanio_segmento;
    int desplazamiento = dir_logica % tamanio_segmento;


    //busco el segmento en la tabla del proceso
    t_segmento* segmento_encontrado = NULL;
    for (int i = 0; i < list_size(ctx->tabla_segmentos); i++) {
        t_segmento* seg = list_get(ctx->tabla_segmentos, i);
        if (seg->id_segmento == num_segmento) {
            segmento_encontrado = seg;
            break;
        }
    }

    //1era validacion: el segmento no existe
    if (segmento_encontrado == NULL) {
        log_error(cpu->logger, "SEG_FAULT: El segmento %d no existe para la dirección lógica %u", num_segmento, dir_logica);
        return false;
    }

    //2da validacion: el acceso excede el límite del segmento
    if (desplazamiento + tamano_a_operar > segmento_encontrado->limite) {
        log_error(cpu->logger, "SEG_FAULT: Desplazamiento (%d) + Tamaño (%u) excede el límite (%u) del Segmento %d", 
                  desplazamiento, tamano_a_operar, segmento_encontrado->limite, num_segmento);
        return false;
    }

    // Paso las validaciones, se traduce la dirección lógica a física
    *dir_fisica_out = segmento_encontrado->base + desplazamiento;
    *ms_id_out = segmento_encontrado->memory_stick_id;

    log_debug(cpu->logger, "Traducción exitosa: Dir. Lógica %u -> Dir. Física %u (Segmento %d, Desplazamiento %d)", 
              dir_logica, *dir_fisica_out, num_segmento, desplazamiento);

    return true;
}