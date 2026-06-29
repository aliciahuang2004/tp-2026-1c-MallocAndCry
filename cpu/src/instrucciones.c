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
    //usleep(1000 * config_get_int_value(cpu->config, "INSTRUCTION_DELAY"));
    log_debug(cpu->logger, "Ejecutando NOOP");
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

    int socket_encontrado = -1;

    pthread_mutex_lock(&cpu->mutex_lista_ms); //cierro

    for(int i = 0; i < list_size(cpu->sockets_memory_sticks); i++) {
        t_ms_conectado* ms = list_get(cpu->sockets_memory_sticks, i);
        if(ms->id == ms_id) {
            socket_encontrado = ms->socket;
            break;
        }
    }
    pthread_mutex_unlock(&cpu->mutex_lista_ms); //abro
    if (socket_encontrado == -1) {
        log_error(cpu->logger, "No se encontró un Memory Stick conectado con el ID %d", ms_id);
    }
    return socket_encontrado;
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

    if (!mmu_traducir_direccion(cpu, ctx, dir_logica, tamanio, &dir_fisica, &ms_id)){
        log_error(cpu->logger, "SEG_FAULT PID %d excedió los límites de memoria.", ctx->pid);
        return 0;
    } 
    
    int socket_ms = obtener_socket_ms(cpu, ms_id);
    if (socket_ms == -1) return 0;

    t_paquete* paquete = crear_paquete(LECTURA_DE_DATOS, crear_buffer());
    agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    enviar_paquete(paquete, socket_ms, cpu->logger);
    eliminar_paquete(paquete);

    t_list* respuesta = recibir_paquete(socket_ms);

    if (respuesta == NULL) {
        log_error(cpu->logger, "Error de red: Se perdió la conexión con el Memory Stick %d durante MOV_IN", ms_id);
        desconectar_memory_stick(cpu, ms_id);
        return 0; 
    }

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

    if (!mmu_traducir_direccion(cpu, ctx, dir_logica, tamanio, &dir_fisica, &ms_id)){
        log_error(cpu->logger, "SEG_FAULT PID %d excedió los límites de memoria.", ctx->pid);
        return 0;
    }
    

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

    if (respuesta == NULL) {
        log_error(cpu->logger, "Error de red: Se perdió la conexión con el Memory Stick %d durante MOV_OUT", ms_id);
        desconectar_memory_stick(cpu, ms_id);
        return 0;
    }

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

    if (!mmu_traducir_direccion(cpu, ctx, dir_logica_origen, tamanio, &dir_fisica_origen, &ms_id_origen)) {
        log_error(cpu->logger, "SEG_FAULT PID %d excedió los límites de memoria.", ctx->pid);
        return 0;
    }
    if (!mmu_traducir_direccion(cpu, ctx, dir_logica_destino, tamanio, &dir_fisica_destino, &ms_id_destino)) {
        log_error(cpu->logger, "SEG_FAULT PID %d excedió los límites de memoria.", ctx->pid);
        return 0;
    }

    int socket_ms_origen = obtener_socket_ms(cpu, ms_id_origen);
    int socket_ms_destino = obtener_socket_ms(cpu, ms_id_destino);

    if (socket_ms_origen == -1 || socket_ms_destino == -1) return 0;

    t_paquete* paquete_leer = crear_paquete(LECTURA_DE_DATOS, crear_buffer());
    agregar_a_paquete(paquete_leer, &dir_fisica_origen, sizeof(uint32_t));
    agregar_a_paquete(paquete_leer, &tamanio, sizeof(int));
    enviar_paquete(paquete_leer, socket_ms_origen, cpu->logger);
    eliminar_paquete(paquete_leer);

    t_list* respuesta_lectura = recibir_paquete(socket_ms_origen);

    if (respuesta_lectura == NULL) {
        log_error(cpu->logger, "Error de red: Conexión perdida con el Memory Stick Origen %d en COPY_MEM", ms_id_origen);
        desconectar_memory_stick(cpu, ms_id_origen);
        return 0;
    }

    void* datos_leidos = list_get(respuesta_lectura, 1);
    
    log_info(cpu->logger, "PID: %d - Acción: LEER - Dirección Física: %u - Valor: <CONTENIDO_COPIADO>", ctx->pid, dir_fisica_origen);

    t_paquete* paquete_escribir = crear_paquete(ESCRITURA_DE_DATOS, crear_buffer());
    agregar_a_paquete(paquete_escribir, &dir_fisica_destino, sizeof(uint32_t));
    agregar_a_paquete(paquete_escribir, &tamanio, sizeof(int));
    agregar_a_paquete(paquete_escribir, datos_leidos, tamanio);
    enviar_paquete(paquete_escribir, socket_ms_destino, cpu->logger);
    eliminar_paquete(paquete_escribir);

    t_list* respuesta_escritura = recibir_paquete(socket_ms_destino);

    if (respuesta_escritura == NULL) {
        log_error(cpu->logger, "Error de red: Conexión perdida con el Memory Stick Destino %d en COPY_MEM", ms_id_destino);
        list_destroy_and_destroy_elements(respuesta_lectura, free);
        desconectar_memory_stick(cpu, ms_id_destino);
        return 0;
    }
    
    log_info(cpu->logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %u - Valor: <CONTENIDO_COPIADO>", ctx->pid, dir_fisica_destino);

    list_destroy_and_destroy_elements(respuesta_lectura, free);
    list_destroy_and_destroy_elements(respuesta_escritura, free);

    return 1;
}

int ejecutar_SYSCALL(t_cpu* cpu, t_contexto* ctx, t_instruccion_decodificada instruccion){
    log_info(cpu->logger, "Delegando SYSCALL %s al Kernel Scheduler", instruccion.nombre_operacion);

    // se guarda el contexto en memoria
    //enviar_contexto_a_memoria(cpu, ctx);

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

        //obtengo la direccion logica y tamaño desde los registros

        uint32_t dir_logica = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_destino);
        uint32_t tamanio = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_origen);

        uint32_t dir_fisica = 0;
        int ms_id = -1;

        if (!mmu_traducir_direccion(cpu, ctx, dir_logica, tamanio, &dir_fisica, &ms_id)) {
            log_error(cpu->logger, "SEG_FAULT PID %d excedió los límites de memoria.", ctx->pid);
            return 0; 
        }

        paquete = crear_paquete(STDOUT, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    }
    else if (instruccion.identificador_operacion == INST_STDIN) {
        uint32_t dir_logica = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_destino);
        uint32_t tamanio = leer_valor_registro(&(ctx->registros), instruccion.argumento_operando_origen);
        
        uint32_t dir_fisica = 0;
        int ms_id = -1;

        if (!mmu_traducir_direccion(cpu, ctx, dir_logica, tamanio, &dir_fisica, &ms_id)) {
            log_error(cpu->logger, "SEG_FAULT PID %d excedió los límites de memoria.", ctx->pid);
            return 0;    
        }

        paquete = crear_paquete(STDIN, crear_buffer());
        agregar_a_paquete(paquete, &(ctx->pid), sizeof(int));
        agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    }
    
    if(paquete != NULL) {
        //Enviamos el contexto actualizado a memoria
        enviar_contexto_a_memoria(cpu, ctx);

        log_debug(cpu->logger, "Enviando paquete de SYSCALL %s al Kernel Scheduler", instruccion.nombre_operacion);
        enviar_paquete(paquete, cpu->socket_kernel_scheduler, cpu->logger);
        eliminar_paquete(paquete);
    }
    return 1;//todo salió bien
    
}

bool mmu_traducir_direccion(t_cpu* cpu, t_contexto* ctx, uint32_t dir_logica, uint32_t tamano_a_operar, uint32_t* dir_fisica_out, int* ms_id_out) {
    
    uint32_t tamanio_max_segmento = (uint32_t)cpu->segment_max_size; 

    int num_segmento = dir_logica / tamanio_max_segmento;
    int desplazamiento = dir_logica % tamanio_max_segmento;


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

    uint32_t tamanio_real_segmento= (segmento_encontrado->limite - segmento_encontrado->base)+1;

    //2da validacion: el acceso excede el límite del segmento
    if (desplazamiento + tamano_a_operar > tamanio_real_segmento) {
        log_error(cpu->logger, "SEG_FAULT: Desplazamiento (%d) + Tamaño (%u) excede el límite (%u) del Segmento %d", 
                  desplazamiento, tamano_a_operar, tamanio_real_segmento, num_segmento);
        return false;
    }

    // Paso las validaciones, se traduce la dirección lógica a física
    *dir_fisica_out = segmento_encontrado->base + desplazamiento;
    *ms_id_out = segmento_encontrado->memory_stick_id;

    log_debug(cpu->logger, "Traducción exitosa: Dir. Lógica %u -> Dir. Física %u (Segmento %d, Desplazamiento %d)", 
              dir_logica, *dir_fisica_out, num_segmento, desplazamiento);

    return true;
}
/*
t_list* fragmentar_acceso_memoria(t_cpu* cpu, uint32_t dir_fisica_global, int tamanio_total) {
    t_list* fragmentos = list_create();
    int bytes_restantes = tamanio_total;
    uint32_t dir_actual = dir_fisica_global;
    int offset = 0;

    pthread_mutex_lock(&cpu->mutex_lista_ms); // Bloqueamos lectura

    while (bytes_restantes > 0) {
        t_ms_conectado* ms_encontrado = NULL;
        
        // 1. Buscamos en qué MS cae la dir_actual
        for(int i = 0; i < list_size(cpu->sockets_memory_sticks); i++) {
            t_ms_conectado* ms = list_get(cpu->sockets_memory_sticks, i);
            if (dir_actual >= ms->base_global && dir_actual <= ms->limite_global) {
                ms_encontrado = ms;
                break;
            }
        }

        if (ms_encontrado == NULL) {
            log_error(cpu->logger, "SEG_FAULT de Hardware: Dir %u fuera de límites de MS", dir_actual);
            list_destroy_and_destroy_elements(fragmentos, free);
            pthread_mutex_unlock(&cpu->mutex_lista_ms);
            return NULL;
        }

        // 2. Calculamos cuánto entra en este MS
        uint32_t dir_local = dir_actual - ms_encontrado->base_global;
        uint32_t espacio_disponible = (ms_encontrado->limite_global - ms_encontrado->base_global + 1) - dir_local;
        int bytes_a_operar = (bytes_restantes < espacio_disponible) ? bytes_restantes : espacio_disponible;

        // 3. Guardamos el fragmento
        t_fragmento_cpu* frag = malloc(sizeof(t_fragmento_cpu));
        frag->socket_ms = ms_encontrado->socket;
        frag->dir_local = dir_local;
        frag->tamano = bytes_a_operar;
        frag->offset = offset;
        list_add(fragmentos, frag);

        // 4. Avanzamos los contadores
        bytes_restantes -= bytes_a_operar;
        dir_actual += bytes_a_operar;
        offset += bytes_a_operar;
    }

    pthread_mutex_unlock(&cpu->mutex_lista_ms);
    return fragmentos;
}
    */