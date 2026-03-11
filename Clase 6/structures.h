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

//estructuras para mkfs

struct Superblock {
    int s_filesystem_type;         // Tipo de sistema de archivos: 2 = EXT2, 3 = EXT3
    int s_inodes_count;            // Número total de inodos
    int s_blocks_count;            // Número total de bloques
    int s_free_blocks_count;       // Número de bloques libres
    int s_free_inodes_count;       // Número de inodos libres
    time_t s_mtime;                // Última fecha de montaje
    time_t s_umtime;               // Última fecha de desmontaje
    int s_mnt_count;               // Contador de montajes
    int s_magic;                   // Número mágico del sistema de archivos (0xEF53)
    int s_inode_size;              // Tamaño del inodo
    int s_block_size;              // Tamaño del bloque
    int s_first_ino;               // Primer inodo disponible
    int s_first_blo;               // Primer bloque disponible
    int s_bm_inode_start;          // Inicio del bitmap de inodos
    int s_bm_block_start;          // Inicio del bitmap de bloques
    int s_inode_start;             // Inicio de la tabla de inodos
    int s_block_start;             // Inicio de los bloques

    Superblock() {
        s_filesystem_type = 0;
        s_inodes_count = 0;
        s_blocks_count = 0;
        s_free_blocks_count = 0;
        s_free_inodes_count = 0;
        s_mtime = time(nullptr);
        s_umtime = time(nullptr);
        s_mnt_count = 0;
        s_magic = 0xEF53;
        s_inode_size = 0;  // Se inicializa en mkfs
        s_block_size = 256;
        s_first_ino = 0;
        s_first_blo = 0;
        s_bm_inode_start = 0;
        s_bm_block_start = 0;
        s_inode_start = 0;
        s_block_start = 0;
    }
};

struct Inode {
    int i_uid;                     // UID del usuario propietario
    int i_gid;                     // GID del grupo propietario
    int i_size;                    // Tamaño del archivo en bytes
    time_t i_atime;                // Última fecha de acceso
    time_t i_ctime;                // Fecha de creación
    time_t i_mtime;                // Última fecha de modificación
    int i_block[15];
    char i_type;                   // Tipo: '0' = archivo, '1' = carpeta
    int i_perm;                    // Permisos del archivo/carpeta

    Inode() {
        i_uid = 0;
        i_gid = 0;
        i_size = 0;
        i_atime = time(nullptr);
        i_ctime = time(nullptr);
        i_mtime = time(nullptr);
        for (int i = 0; i < 15; i++) {
            i_block[i] = -1;
        }
        i_type = '0';
        i_perm = 664;
    }
};

struct Content {
    char b_name[12];               // Nombre del archivo o carpeta
    int b_inodo;                   // Apuntador al inodo

    Content() {
        memset(b_name, 0, sizeof(b_name));
        b_inodo = -1;
    }
};

struct FolderBlock {
    Content b_content[4];          // 4 contenidos por bloque de carpeta

    FolderBlock() {
    }
};

struct FileBlock {
    char b_content[256];            // Contenido del archivo

    FileBlock() {
        memset(b_content, 0, sizeof(b_content));
    }
};

struct PointerBlock {
    int b_pointers[16];            // Array de apuntadores a bloques

    PointerBlock() {
        for (int i = 0; i < 16; i++) {
            b_pointers[i] = -1;
        }
    }
};

#endif // STRUCTURES_H
