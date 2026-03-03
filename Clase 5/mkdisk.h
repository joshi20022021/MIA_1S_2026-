#ifndef MKDISK_H
#define MKDISK_H

#include <string>      // Manipula cadenas de texto (std::string).
#include <iostream>    // Maneja entrada y salida estándar (cin, cout).
#include <fstream>     // Proporciona funcionalidades para trabajar con archivos (lectura y escritura).
#include <cstring>     // Manipula cadenas C-style (funciones como strcpy, strcmp, etc.).
#include <cstdlib>     // Proporciona funciones generales como rand() y conversiones de cadenas a números.
#include <filesystem>  // Proporciona funciones para trabajar con el sistema de archivos (archivos, directorios).
#include "structures.h" // Define estructuras de datos personalizadas.


namespace CommandMkdisk {
    
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
    
    // Crear directorios padre si no existen
    inline bool createDirectories(const std::string& path) {
        try {
            std::filesystem::path filePath(path);
            std::filesystem::path parentPath = filePath.parent_path();
            
            if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                std::filesystem::create_directories(parentPath);
                std::cout << "Creadas carpetas: " << parentPath << std::endl;
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error creando carpetas: " << e.what() << std::endl;
            return false;
        }
    }

    // Comando mkdisk: Crear un disco virtual
    inline std::string execute(int size, const std::string& unit, const std::string& path) {
        try {
            std::string expandedPath = expandPath(path);
            
            // Validar parámetros
            if (size <= 0) {
                return "Error: El tamaño debe ser mayor a 0";
            }

            // Calcular tamaño en bytes
            int sizeInBytes = size;
            if (unit == "k" || unit == "K") {
                sizeInBytes = size * 1024;  // Kilobytes
            } else if (unit == "m" || unit == "M") {
                sizeInBytes = size * 1024 * 1024;  // Megabytes
            } else {
                return "Error: Unidad no válida. Use 'k' para KB o 'm' para MB";
            }

            // Crear directorios padre si no existen
            if (!createDirectories(expandedPath)) {
                return "Error: No se pudieron crear las carpetas necesarias";
            }

            // Verificar si el archivo ya existe
            std::ifstream checkFile(expandedPath);
            if (checkFile.good()) {
                checkFile.close();
                return "Error: El disco ya existe en la ruta especificada";
            }

            // Crear el archivo del disco
            std::ofstream diskFile(expandedPath, std::ios::binary);
            if (!diskFile.is_open()) {
                return "Error: No se pudo crear el archivo del disco";
            }

            // Llenar el archivo con ceros
            char zero = '\0';
            for (int i = 0; i < sizeInBytes; i++) {
                diskFile.write(&zero, 1);
            }

            // Crear y escribir el MBR
            MBR mbr = {}; // Inicializar con ceros
            
            mbr.mbr_size = sizeInBytes;
            mbr.mbr_creation_date = time(nullptr);
            mbr.mbr_disk_signature = rand();
            mbr.disk_fit = 'F';  // First Fit por defecto
            
            // Inicializar todas las particiones como inactivas
            for (int i = 0; i < 4; i++) {
                mbr.mbr_partitions[i].part_status = '0';
                mbr.mbr_partitions[i].part_type = '\0';
                mbr.mbr_partitions[i].part_fit = '\0';
                mbr.mbr_partitions[i].part_start = -1;
                mbr.mbr_partitions[i].part_size = 0;
                memset(mbr.mbr_partitions[i].part_name, 0, 16);
            }

            diskFile.seekp(0, std::ios::beg);
            diskFile.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            diskFile.close();

            // Crear mensaje de éxito
            char timeStr[26];
            struct tm* timeinfo = localtime(&mbr.mbr_creation_date);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

            return "Disco creado exitosamente\n" +
                   std::string("  Ruta: ") + expandedPath + "\n" +
                   std::string("  Tamaño: ") + std::to_string(size) + " " + unit + 
                   " (" + std::to_string(sizeInBytes) + " bytes)\n" +
                   std::string("  Fecha: ") + timeStr + "\n" +
                   std::string("  Firma: ") + std::to_string(mbr.mbr_disk_signature);

        } catch (const std::exception& e) {
            return std::string("Error al crear disco: ") + e.what();
        }
    }

} // namespace CommandMkdisk

#endif // MKDISK_H
