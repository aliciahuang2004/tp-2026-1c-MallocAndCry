#ifndef ESTRUCTURAS_H
#define ESTRUCTURAS_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    //int pid;
    uint32_t PC; //4 bytes
    uint8_t AX; //1 byte
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI;
    uint32_t DI;

} t_registros;      

typedef struct {
    int pid;
    t_registros* registros;
    t_list*      tabla_segmentos;  // lista de t_segmento*
} t_contexto;

typedef struct {

    int id_segmento;

    uint32_t base;

    uint32_t limite;

    int memory_stick_id;

    bool en_swap;

    int bloque_swap;

} t_segmento;

typedef struct {

    int pid;

    t_contexto* contexto;

    //char* path_instrucciones;  ******LOS PATHS SE GUARDAN EN KM GLOBALMENTE

    bool suspendido;

} t_proceso;

typedef struct {

    uint32_t base;

    uint32_t tamano;

} t_hueco;

typedef struct {

    int id;

    uint32_t base_global;

    uint32_t tamano;

    int socket;

} t_ms_info;  //t_memory_stick

/*
typedef struct {

    int bloque;

    int pid;

    int id_segmento;

    uint32_t tamanio;

} t_swap_segmento;
*/
#endif 