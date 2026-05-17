#include "instrucciones.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// NOOP
void ejecutar_NOOP(t_cpu* cpu, t_contexto* ctx) {
    usleep(1000 * config_get_int_value(cpu->config, "INSTRUCTION_DELAY"));
}

// SET
void ejecutar_SET(t_registros* reg, char* registro_destino, uint32_t valor) {
    
}

// SUM
void ejecutar_SUM(t_registros* reg, char* registro_destino, char* registro_origen) {
}

// SUB
void ejecutar_SUB(t_registros* reg, char* registro_destino, char* registro_origen) {
   
}

// JNZ
void ejecutar_JNZ(t_registros* reg, char* registro_evaluado, uint32_t nueva_instruccion) {
    
}

// MOV_IN:
int ejecutar_MOV_IN(t_cpu* cpu, t_contexto* ctx, char* registro_datos) {
    
    return 1; // 1 si fue exitoso, 0 si hubo SEG_FAULT para romper el ciclo
}

// MOV_OUT
int ejecutar_MOV_OUT(t_cpu* cpu, t_contexto* ctx, char* registro_datos) {
   
    
    return 1;
}

// COPY_MEM
int ejecutar_COPY_MEM(t_cpu* cpu, t_contexto* ctx, char* registro_tamano) {

    
    return 1;
}