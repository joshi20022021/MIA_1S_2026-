#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <ctime>
#include <cstring>


struct Partition {
    char part_status;          // Estado de la partición: '0' = inactiva, '1' = activa
    char part_type;            // Tipo: 'P' = Primaria, 'E' = Extendida, 'L' = Lógica
    char part_fit;             // Ajuste: 'B' = Best Fit, 'F' = First Fit, 'W' = Worst Fit
    int part_start;            // Byte donde inicia la partición
    int part_size;             // Tamaño de la partición en bytes
    char part_name[16];        // Nombre de la partición (máx. 16 caracteres)

    Partition() {
        part_status = '0';
        part_type = '\0';
        part_fit = '\0';
        part_start = -1;
        part_size = 0;
        memset(part_name, 0, sizeof(part_name));
    }
};


struct MBR {
    int mbr_size;                      // Tamaño total del disco en bytes
    time_t mbr_creation_date;          // Fecha de creación del disco
    int mbr_disk_signature;            // Firma única del disco
    char disk_fit;                     // Ajuste del disco: 'B', 'F', 'W'
    Partition mbr_partitions[4];       // Máximo 4 particiones (3 primarias + 1 extendida o 4 primarias)

    MBR() {
        mbr_size = 0;
        mbr_creation_date = time(nullptr);
        mbr_disk_signature = rand();
        disk_fit = 'F';  // First Fit por defecto
    }
};

struct EBR {
    char part_status;          // Estado de la partición lógica
    char part_fit;             // Ajuste de la partición
    int part_start;            // Byte donde inicia la partición lógica
    int part_size;             // Tamaño de la partición lógica
    int part_next;             // Byte donde inicia el siguiente EBR (-1 si no hay más)
    char part_name[16];        // Nombre de la partición

    EBR() {
        part_status = '0';
        part_fit = 'F';
        part_start = -1;
        part_size = 0;
        part_next = -1;
        memset(part_name, 0, sizeof(part_name));
    }
};

#endif // STRUCTURES_H
