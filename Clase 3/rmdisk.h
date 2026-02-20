#ifndef RMDISK_H
#define RMDISK_H

#include <string>        // Manipula cadenas de texto
#include <iostream>      // Maneja entrada y salida estándar
#include <fstream>       // funcionalidades para trabajar con archivos
#include <cstring>       // Manipula cadenas C-style
#include <sys/stat.h>    // funciones para obtener información sobre archivos.
#include <cstdlib>       // funciones generales como rand() y conversiones de cadenas a números.
#include <cstdio>        // funciones para trabajar con archivos en estilo C
#include <filesystem>    // Proporciona funciones para trabajar con el sistema de archivos (archivos, directorios).
#include "structures.h"  // Define estructuras de datos personalizadas.

namespace CommandRmdisk {
    
    // Expandir rutas que contengan ~ (home directory)
    inline std::string expandPath(const std::string& path) {
        if (path.empty() || path[0] != '~') {
            return path;
        }
        
        const char* home = std::getenv("HOME");
        if (!home) {
            std::cerr << "Error: No se pudo obtener el directorio HOME" << std::endl;
            return path;
        }
        
        return std::string(home) + path.substr(1);
    }

    // Comando rmdisk: Eliminar un disco virtual
    inline std::string execute(const std::string& path) {
        try {
            // Expandir ruta si contiene ~
            std::string expandedPath = expandPath(path);
            
            // Verificar si el archivo existe
            std::ifstream checkFile(expandedPath);
            if (!checkFile.good()) {
                checkFile.close();
                return "Error: El disco no existe en la ruta especificada";
            }
            checkFile.close();

            // Leer el MBR para obtener información del disco
            std::ifstream diskFile(expandedPath, std::ios::binary);
            MBR mbr;
            diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            diskFile.close();

            // Obtener información antes de eliminar
            char timeStr[26];
            struct tm* timeinfo = localtime(&mbr.mbr_creation_date);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

            std::string info = "Disco eliminado:\n" +
                              std::string("  Ruta: ") + expandedPath + "\n" +
                              std::string("  Tamaño: ") + std::to_string(mbr.mbr_size) + " bytes\n" +
                              std::string("  Fecha creación: ") + timeStr + "\n" +
                              std::string("  Firma: ") + std::to_string(mbr.mbr_disk_signature);

            // Eliminar el archivo
            if (remove(expandedPath.c_str()) != 0) {
                return "Error: No se pudo eliminar el archivo del disco";
            }

            return info;

        } catch (const std::exception& e) {
            return std::string("Error al eliminar disco: ") + e.what();
        }
    }

} // namespace CommandRmdisk

#endif // RMDISK_H
