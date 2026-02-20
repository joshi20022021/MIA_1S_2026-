#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <string>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include "structures.h"

class DiskManager {
private:
    static std::string expandPath(const std::string& path) {
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
    
    // Crear directorios
    static bool createDirectories(const std::string& path) {
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


public:
    // Crear un disco virtual
    static std::string mkdisk(int size, const std::string& unit, const std::string& path) {
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

    // Eliminar un disco virtual
    static std::string rmdisk(const std::string& path) {
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

    // Obtener información de un disco
    static std::string getDiskInfo(const std::string& path) {
        try {
            // Expandir ruta si contiene ~
            std::string expandedPath = expandPath(path);
            
            std::ifstream diskFile(expandedPath, std::ios::binary);
            if (!diskFile.is_open()) {
                return "Error: No se pudo abrir el disco";
            }

            MBR mbr;
            diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            diskFile.close();

            char timeStr[26];
            struct tm* timeinfo = localtime(&mbr.mbr_creation_date);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

            std::string info = "=== INFORMACIÓN DEL DISCO MBR ===\n";
            info += std::string("  Ruta: ") + expandedPath + "\n";
            info += std::string("  Tamaño: ") + std::to_string(mbr.mbr_size) + " bytes\n";
            info += std::string("  Fecha creación: ") + timeStr + "\n";
            info += std::string("  Firma: ") + std::to_string(mbr.mbr_disk_signature) + "\n";
            info += std::string("  Ajuste: ") + mbr.disk_fit + "\n";
            info += std::string("  \n=== PARTICIONES ===\n");

            for (int i = 0; i < 4; i++) {
                info += "  [" + std::to_string(i+1) + "] ";
                if (mbr.mbr_partitions[i].part_status == '1') {
                    info += "ACTIVA - " + std::string(mbr.mbr_partitions[i].part_name) + 
                           " (" + std::to_string(mbr.mbr_partitions[i].part_size) + " bytes)\n";
                } else {
                    info += "INACTIVA\n";
                }
            }
            
            info += "\n=== ===\n";
            diskFile.open(expandedPath, std::ios::binary);
            char buffer[100];
            diskFile.read(buffer, 100);
            diskFile.close();
            
            for (int i = 0; i < 100; i++) {
                if (i % 16 == 0) info += std::string("\n") + (i < 16 ? "0" : "") + std::to_string(i/16) + "0: ";
                char hex[4];
                sprintf(hex, "%02X ", (unsigned char)buffer[i]);
                info += hex;
            }

            return info;

        } catch (const std::exception& e) {
            return std::string("Error al obtener información: ") + e.what();
        }
    }

    // Gestionar particiones en un disco (fdisk)
    static std::string fdisk(int size, const std::string& unit, const std::string& path, 
                            const std::string& type, const std::string& fit, 
                            const std::string& deleteName, const std::string& name) {
        try {
            std::string expandedPath = expandPath(path);

            // Verificar que el disco exista
            std::ifstream checkFile(expandedPath);
            if (!checkFile.good()) {
                checkFile.close();
                return "Error: El disco no existe";
            }
            checkFile.close();

            // Si es operación de eliminación
            if (!deleteName.empty()) {
                return deletePartition(expandedPath, deleteName);
            }

            // Si es operación de adición (validar parámetros)
            if (name.empty()) {
                return "Error: Se requiere el parámetro -name";
            }
            if (size <= 0) {
                return "Error: El tamaño debe ser mayor a 0";
            }

            // Calcular tamaño en bytes
            int sizeInBytes = size;
            if (unit == "k" || unit == "K") {
                sizeInBytes = size * 1024;
            } else if (unit == "m" || unit == "M") {
                sizeInBytes = size * 1024 * 1024;
            } else {
                return "Error: Unidad no válida. Use 'k' para KB o 'm' para MB";
            }

            // Validar tipo de partición
            char partType = 'P';  // Por defecto primaria
            if (!type.empty()) {
                if (type == "p" || type == "P") {
                    partType = 'P';
                } else if (type == "e" || type == "E") {
                    partType = 'E';
                } else if (type == "l" || type == "L") {
                    partType = 'L';
                } else {
                    return "Error: Tipo inválido. Use P (primaria), E (extendida) o L (lógica)";
                }
            }

            // Validar fit
            char partFit = 'W';  // Worst Fit por defecto
            if (!fit.empty()) {
                if (fit == "bf" || fit == "BF") {
                    partFit = 'B';
                } else if (fit == "ff" || fit == "FF") {
                    partFit = 'F';
                } else if (fit == "wf" || fit == "WF") {
                    partFit = 'W';
                } else {
                    return "Error: Fit inválido. Use BF, FF o WF";
                }
            }

            // Crear la partición
            if (partType == 'L') {
                return createLogicalPartition(expandedPath, sizeInBytes, partFit, name);
            } else {
                return createPrimaryOrExtendedPartition(expandedPath, sizeInBytes, partType, partFit, name);
            }

        } catch (const std::exception& e) {
            return std::string("Error en fdisk: ") + e.what();
        }
    }

private:
    // Crear partición primaria o extendida
    static std::string createPrimaryOrExtendedPartition(const std::string& path, int size, 
                                                       char type, char fit, const std::string& name) {
        // Leer MBR
        std::fstream diskFile(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!diskFile.is_open()) {
            return "Error: No se pudo abrir el disco";
        }

        MBR mbr;
        diskFile.seekg(0, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        // Validar nombre único
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1' && 
                strcmp(mbr.mbr_partitions[i].part_name, name.c_str()) == 0) {
                diskFile.close();
                return "Error: Ya existe una partición con ese nombre";
            }
        }

        // Contar particiones existentes y buscar extendida
        int partCount = 0;
        bool hasExtended = false;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                partCount++;
                if (mbr.mbr_partitions[i].part_type == 'E') {
                    hasExtended = true;
                }
            }
        }

        // Validaciones
        if (partCount >= 4) {
            diskFile.close();
            return "Error: Ya existen 4 particiones (máximo permitido)";
        }

        if (type == 'E' && hasExtended) {
            diskFile.close();
            return "Error: Ya existe una partición extendida";
        }

        // Encontrar espacio disponible según el ajuste
        int selectedSlot = -1;
        int bestStart = -1;

        if (fit == 'F') {  // First Fit: primer espacio que encaje
            for (int i = 0; i < 4; i++) {
                if (mbr.mbr_partitions[i].part_status == '0') {
                    // Buscar espacio disponible
                    int currentPos = sizeof(MBR);
                    
                    for (int j = 0; j < 4; j++) {
                        if (mbr.mbr_partitions[j].part_status == '1' && 
                            mbr.mbr_partitions[j].part_start == currentPos) {
                            currentPos = mbr.mbr_partitions[j].part_start + mbr.mbr_partitions[j].part_size;
                        }
                    }
                    
                    int availableSpace = mbr.mbr_size - currentPos;
                    if (availableSpace >= size) {
                        selectedSlot = i;
                        bestStart = currentPos;
                        break;
                    }
                }
            }
        } else if (fit == 'B') {  // Best Fit: el espacio más pequeño que encaje
            int minWaste = mbr.mbr_size;
            for (int i = 0; i < 4; i++) {
                if (mbr.mbr_partitions[i].part_status == '0') {
                    int currentPos = sizeof(MBR);
                    
                    for (int j = 0; j < 4; j++) {
                        if (mbr.mbr_partitions[j].part_status == '1' && 
                            mbr.mbr_partitions[j].part_start == currentPos) {
                            currentPos = mbr.mbr_partitions[j].part_start + mbr.mbr_partitions[j].part_size;
                        }
                    }
                    
                    int availableSpace = mbr.mbr_size - currentPos;
                    int waste = availableSpace - size;
                    if (availableSpace >= size && waste < minWaste) {
                        selectedSlot = i;
                        bestStart = currentPos;
                        minWaste = waste;
                    }
                }
            }
        } else {  // Worst Fit: el espacio más grande
            int maxSpace = 0;
            for (int i = 0; i < 4; i++) {
                if (mbr.mbr_partitions[i].part_status == '0') {
                    int currentPos = sizeof(MBR);
                    
                    for (int j = 0; j < 4; j++) {
                        if (mbr.mbr_partitions[j].part_status == '1' && 
                            mbr.mbr_partitions[j].part_start == currentPos) {
                            currentPos = mbr.mbr_partitions[j].part_start + mbr.mbr_partitions[j].part_size;
                        }
                    }
                    
                    int availableSpace = mbr.mbr_size - currentPos;
                    if (availableSpace >= size && availableSpace > maxSpace) {
                        selectedSlot = i;
                        bestStart = currentPos;
                        maxSpace = availableSpace;
                    }
                }
            }
        }

        if (selectedSlot == -1 || bestStart == -1) {
            diskFile.close();
            return "Error: No hay espacio suficiente en el disco";
        }

        // Crear la partición
        mbr.mbr_partitions[selectedSlot].part_status = '1';
        mbr.mbr_partitions[selectedSlot].part_type = type;
        mbr.mbr_partitions[selectedSlot].part_fit = fit;
        mbr.mbr_partitions[selectedSlot].part_start = bestStart;
        mbr.mbr_partitions[selectedSlot].part_size = size;
        strncpy(mbr.mbr_partitions[selectedSlot].part_name, name.c_str(), 16);

        // Si es extendida, inicializar con un EBR
        if (type == 'E') {
            EBR ebr;
            ebr.part_status = '0';
            ebr.part_fit = fit;
            ebr.part_start = -1;
            ebr.part_size = 0;
            ebr.part_next = -1;
            memset(ebr.part_name, 0, 16);

            diskFile.seekp(bestStart, std::ios::beg);
            diskFile.write(reinterpret_cast<char*>(&ebr), sizeof(EBR));
        }

        // Escribir MBR actualizado
        diskFile.seekp(0, std::ios::beg);
        diskFile.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        diskFile.close();

        return "Partición " + std::string(1, type) + " '" + name + "' creada exitosamente\n" +
               "  Inicio: " + std::to_string(bestStart) + "\n" +
               "  Tamaño: " + std::to_string(size) + " bytes\n" +
               "  Ajuste: " + std::string(1, fit);
    }

    // Crear partición lógica
    static std::string createLogicalPartition(const std::string& path, int size, 
                                             char fit, const std::string& name) {
        // Leer MBR
        std::fstream diskFile(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!diskFile.is_open()) {
            return "Error: No se pudo abrir el disco";
        }

        MBR mbr;
        diskFile.seekg(0, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        // Buscar partición extendida
        int extendedIndex = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1' && 
                mbr.mbr_partitions[i].part_type == 'E') {
                extendedIndex = i;
                break;
            }
        }

        if (extendedIndex == -1) {
            diskFile.close();
            return "Error: No existe una partición extendida para crear particiones lógicas";
        }

        Partition& extended = mbr.mbr_partitions[extendedIndex];
        int extStart = extended.part_start;
        int extEnd = extended.part_start + extended.part_size;

        // Leer primer EBR
        EBR currentEBR;
        diskFile.seekg(extStart, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));

        // Si el primer EBR está vacío
        if (currentEBR.part_status == '0') {
            currentEBR.part_status = '1';
            currentEBR.part_fit = fit;
            currentEBR.part_start = extStart + sizeof(EBR);
            currentEBR.part_size = size;
            currentEBR.part_next = -1;
            strncpy(currentEBR.part_name, name.c_str(), 16);

            diskFile.seekp(extStart, std::ios::beg);
            diskFile.write(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));
            diskFile.close();

            return "Partición lógica '" + name + "' creada exitosamente\n" +
                   "  Inicio: " + std::to_string(currentEBR.part_start) + "\n" +
                   "  Tamaño: " + std::to_string(size) + " bytes";
        }

        // Buscar el último EBR o un espacio adecuado
        int currentEBRPos = extStart;
        while (true) {
            diskFile.seekg(currentEBRPos, std::ios::beg);
            diskFile.read(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));

            // Verificar nombre único
            if (currentEBR.part_status == '1' && 
                strcmp(currentEBR.part_name, name.c_str()) == 0) {
                diskFile.close();
                return "Error: Ya existe una partición lógica con ese nombre";
            }

            if (currentEBR.part_next == -1) {
                // Último EBR encontrado
                int nextEBRPos = currentEBR.part_start + currentEBR.part_size;
                int availableSpace = extEnd - nextEBRPos - sizeof(EBR);

                if (availableSpace < size) {
                    diskFile.close();
                    return "Error: No hay espacio suficiente en la partición extendida";
                }

                // Crear nuevo EBR
                EBR newEBR;
                newEBR.part_status = '1';
                newEBR.part_fit = fit;
                newEBR.part_start = nextEBRPos + sizeof(EBR);
                newEBR.part_size = size;
                newEBR.part_next = -1;
                strncpy(newEBR.part_name, name.c_str(), 16);

                // Actualizar EBR anterior
                currentEBR.part_next = nextEBRPos;
                diskFile.seekp(currentEBRPos, std::ios::beg);
                diskFile.write(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));

                // Escribir nuevo EBR
                diskFile.seekp(nextEBRPos, std::ios::beg);
                diskFile.write(reinterpret_cast<char*>(&newEBR), sizeof(EBR));
                diskFile.close();

                return "Partición lógica '" + name + "' creada exitosamente\n" +
                       "  Inicio: " + std::to_string(newEBR.part_start) + "\n" +
                       "  Tamaño: " + std::to_string(size) + " bytes";
            }

            currentEBRPos = currentEBR.part_next;
        }
    }

    // Eliminar partición
    static std::string deletePartition(const std::string& path, const std::string& name) {
        std::fstream diskFile(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!diskFile.is_open()) {
            return "Error: No se pudo abrir el disco";
        }

        MBR mbr;
        diskFile.seekg(0, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        // Buscar y eliminar partición primaria o extendida
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1' && 
                strcmp(mbr.mbr_partitions[i].part_name, name.c_str()) == 0) {
                
                std::string info = "Partición eliminada: " + name + "\n" +
                                  "  Tipo: " + std::string(1, mbr.mbr_partitions[i].part_type) + "\n" +
                                  "  Tamaño: " + std::to_string(mbr.mbr_partitions[i].part_size) + " bytes";

                // Marcar como inactiva
                mbr.mbr_partitions[i].part_status = '0';
                mbr.mbr_partitions[i].part_type = '\0';
                mbr.mbr_partitions[i].part_fit = '\0';
                mbr.mbr_partitions[i].part_start = -1;
                mbr.mbr_partitions[i].part_size = 0;
                memset(mbr.mbr_partitions[i].part_name, 0, 16);

                diskFile.seekp(0, std::ios::beg);
                diskFile.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
                diskFile.close();
                return info;
            }
        }

        // Buscar y eliminar partición lógica
        int extendedIndex = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1' && 
                mbr.mbr_partitions[i].part_type == 'E') {
                extendedIndex = i;
                break;
            }
        }

        if (extendedIndex != -1) {
            int extStart = mbr.mbr_partitions[extendedIndex].part_start;
            int currentEBRPos = extStart;
            int prevEBRPos = -1;
            EBR prevEBR;

            while (currentEBRPos != -1) {
                EBR currentEBR;
                diskFile.seekg(currentEBRPos, std::ios::beg);
                diskFile.read(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));

                if (currentEBR.part_status == '1' && 
                    strcmp(currentEBR.part_name, name.c_str()) == 0) {
                    
                    std::string info = "Partición lógica eliminada: " + name + "\n" +
                                      "  Tamaño: " + std::to_string(currentEBR.part_size) + " bytes";

                    // Actualizar enlaces
                    if (prevEBRPos != -1) {
                        prevEBR.part_next = currentEBR.part_next;
                        diskFile.seekp(prevEBRPos, std::ios::beg);
                        diskFile.write(reinterpret_cast<char*>(&prevEBR), sizeof(EBR));
                    } else {
                        // Es el primer EBR
                        if (currentEBR.part_next != -1) {
                            EBR nextEBR;
                            diskFile.seekg(currentEBR.part_next, std::ios::beg);
                            diskFile.read(reinterpret_cast<char*>(&nextEBR), sizeof(EBR));
                            diskFile.seekp(extStart, std::ios::beg);
                            diskFile.write(reinterpret_cast<char*>(&nextEBR), sizeof(EBR));
                        } else {
                            // Era la única lógica
                            EBR emptyEBR;
                            emptyEBR.part_status = '0';
                            emptyEBR.part_fit = 'F';
                            emptyEBR.part_start = -1;
                            emptyEBR.part_size = 0;
                            emptyEBR.part_next = -1;
                            memset(emptyEBR.part_name, 0, 16);
                            diskFile.seekp(extStart, std::ios::beg);
                            diskFile.write(reinterpret_cast<char*>(&emptyEBR), sizeof(EBR));
                        }
                    }

                    diskFile.close();
                    return info;
                }

                prevEBRPos = currentEBRPos;
                prevEBR = currentEBR;
                currentEBRPos = currentEBR.part_next;
            }
        }

        diskFile.close();
        return "Error: No se encontró una partición con el nombre '" + name + "'";
    }
};

#endif // DISK_MANAGER_H
