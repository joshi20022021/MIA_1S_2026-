#ifndef MKFS_H
#define MKFS_H

#include <iostream>   // Permite entrada y salida estándar
#include <string>     // Manejo de la clase std::string
#include <sstream>    // Permite usar flujos de texto en memoria
#include <fstream>    // Manejo de archivos
#include <cstring>    // Funciones para manejo de cadenas
#include <cmath>      // Funciones matemáticas
#include <algorithm>  // Algoritmos estándar
#include "structures.h"
#include "mount.h"    

namespace CommandMkfs {
    
    inline std::string toLowerCase(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    inline std::string execute(const std::string& id, const std::string& type) {
        // Validar parámetros
        if (id.empty()) {
            return "Error: mkfs requiere el parámetro -id";
        }
        
        std::string formatType = toLowerCase(type);
        if (formatType.empty()) {
            formatType = "full";  // Por defecto formateo completo
        }
        
        if (formatType != "full") {
            return "Error: type debe ser 'full'";
        }
        
        // Siempre se formatea como ext2
        std::string fsType = "ext2";
        
        // Buscar la partición montada
        CommandMount::MountedPartition partition;
        if (!CommandMount::getMountedPartition(id, partition)) {
            return "Error: la partición con ID '" + id + "' no está montada";
        }
        
        // Abrir el disco
        std::fstream file(partition.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + partition.path + "'";
        }
        
        // Calcular el número de estructuras según el tamaño de la partición
        int partitionSize = partition.size;
        
        // Calcular n (número de estructuras)
        // donde 4 bytes para bitmaps, 1 inodo, 3 bloques por inodo
        int numerator = partitionSize - sizeof(Superblock);
        int denominator = 4 + sizeof(Inode) + 3 * 64;  // 64 es el tamaño de un bloque
        int n = numerator / denominator;
        
        if (n <= 0) {
            file.close();
            return "Error: la partición es muy pequeña para crear un sistema de archivos";
        }
        
        // Crear el Superbloque
        Superblock sb;
        sb.s_filesystem_type = 2;  // ext2
        sb.s_inodes_count = n;
        sb.s_blocks_count = 3 * n;
        sb.s_free_blocks_count = 3 * n - 2;  // Se usan 2 bloques: raíz y users.txt
        sb.s_free_inodes_count = n - 2;      // Se usan 2 inodos: raíz y users.txt
        sb.s_mtime = time(nullptr);
        sb.s_umtime = time(nullptr);
        sb.s_mnt_count = 1;
        sb.s_magic = 0xEF53;
        sb.s_inode_size = sizeof(Inode);
        sb.s_block_size = 64;
        sb.s_first_ino = 2;  // Primer inodo libre (0=raíz, 1=users.txt)
        sb.s_first_blo = 2;  // Primer bloque libre (0=raíz, 1=users.txt)
        sb.s_bm_inode_start = partition.start + sizeof(Superblock);
        sb.s_bm_block_start = sb.s_bm_inode_start + n;
        sb.s_inode_start = sb.s_bm_block_start + 3 * n;
        sb.s_block_start = sb.s_inode_start + n * sizeof(Inode);
        
        // Escribir el Superbloque
        file.seekp(partition.start, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
        
        // Inicializar bitmap de inodos
        file.seekp(sb.s_bm_inode_start, std::ios::beg);
        for (int i = 0; i < n; i++) {
            char bit = (i == 0 || i == 1) ? '1' : '0';  // Marcar inodo 0 (raíz) y 1 (users.txt) como usados
            file.write(&bit, 1);
        }
        
        // Inicializar bitmap de bloques
        file.seekp(sb.s_bm_block_start, std::ios::beg);
        for (int i = 0; i < 3 * n; i++) {
            char bit = (i == 0 || i == 1) ? '1' : '0';  // Marcar bloque 0 (raíz) y 1 (users.txt) como usados
            file.write(&bit, 1);
        }
        
        // Crear inodo raíz (inodo 0 - directorio "/")
        Inode rootInode;
        rootInode.i_uid = 1;
        rootInode.i_gid = 1;
        rootInode.i_size = 0;
        rootInode.i_atime = time(nullptr);
        rootInode.i_ctime = time(nullptr);
        rootInode.i_mtime = time(nullptr);
        rootInode.i_type = '1';  // Es carpeta
        rootInode.i_perm = 664;
        rootInode.i_block[0] = 0;  // Apunta al bloque 0
        
        file.seekp(sb.s_inode_start, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&rootInode), sizeof(Inode));
        
        // Crear inodo para users.txt (inodo 1 - archivo)
        Inode usersInode;
        usersInode.i_uid = 1;
        usersInode.i_gid = 1;
        usersInode.i_size = 27;  // Tamaño del contenido inicial "1,G,root\n1,U,root,root,123\n"
        usersInode.i_atime = time(nullptr);
        usersInode.i_ctime = time(nullptr);
        usersInode.i_mtime = time(nullptr);
        usersInode.i_type = '0';  // Es archivo
        usersInode.i_perm = 664;
        usersInode.i_block[0] = 1;  // Apunta al bloque 1
        
        file.seekp(sb.s_inode_start + sizeof(Inode), std::ios::beg);
        file.write(reinterpret_cast<const char*>(&usersInode), sizeof(Inode));
        
        // Crear bloque de carpeta raíz (bloque 0)
        FolderBlock rootBlock;
        
        // Entrada "." (directorio actual)
        std::strncpy(rootBlock.b_content[0].b_name, ".", 12);
        rootBlock.b_content[0].b_inodo = 0;  // Apunta a sí mismo
        
        // Entrada ".." (directorio padre)
        std::strncpy(rootBlock.b_content[1].b_name, "..", 12);
        rootBlock.b_content[1].b_inodo = 0;  // En raíz, padre es sí mismo
        
        // Entrada "users.txt"
        std::strncpy(rootBlock.b_content[2].b_name, "users.txt", 12);
        rootBlock.b_content[2].b_inodo = 1;  // Apunta al inodo 1
        
        // Entrada 3 vacía
        rootBlock.b_content[3].b_inodo = -1;
        
        file.seekp(sb.s_block_start, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&rootBlock), sizeof(FolderBlock));
        
        // Crear bloque de contenido para users.txt (bloque 1)
        FileBlock usersBlock;
        std::string usersContent = "1,G,root\n1,U,root,root,123\n";
        std::strncpy(usersBlock.b_content, usersContent.c_str(), 64);
        
        file.seekp(sb.s_block_start + 64, std::ios::beg);  // 64 bytes = sizeof(FolderBlock)
        file.write(reinterpret_cast<const char*>(&usersBlock), sizeof(FileBlock));
        
        file.close();
        
        std::ostringstream result;
        result << "\n=== MKFS ===\n";
        result << "Sistema de archivos EXT2 creado exitosamente\n";
        result << "  ID: " << id << "\n";
        result << "  Disco: " << partition.path << "\n";
        result << "  Partición: " << partition.name << "\n";
        result << "  Tamaño: " << partitionSize << " bytes\n";
        result << "  Inodos: " << n << "\n";
        result << "  Bloques: " << (3 * n) << "\n";
        result << "  Archivo users.txt creado en la raíz";
        
        return result.str();
    }
    
} // namespace CommandMkfs

#endif // MKFS_H
