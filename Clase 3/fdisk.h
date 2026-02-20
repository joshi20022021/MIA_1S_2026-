#ifndef FDISK_H  
#define FDISK_H   

#include <string>    // Manipula cadenas de texto
#include <iostream>  // Maneja entrada y salida estándar
#include <fstream>   // Proporciona funcionalidades para trabajar con archivos (lectura y escritura).
#include <cstring>   // Manipula cadenas C-style
#include <cstdlib>   // funciones generales como el rand() y conversiones de cadenas a números.
#include "structures.h" // estructuras de datos.


namespace CommandFdisk {
    
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

    // Crear partición primaria o extendida
    inline std::string createPrimaryOrExtendedPartition(const std::string& path, int size, 
                                                       char type, char fit, const std::string& name) {
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

        // Contar particiones y buscar extendida
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

        if (fit == 'F') {  // First Fit
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
                    if (availableSpace >= size) {
                        selectedSlot = i;
                        bestStart = currentPos;
                        break;
                    }
                }
            }
        } else if (fit == 'B') {  // Best Fit
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
        } else {  // Worst Fit
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
    inline std::string createLogicalPartition(const std::string& path, int size, 
                                             char fit, const std::string& name) {
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

        // Buscar el último EBR
        int currentEBRPos = extStart;
        while (true) {
            diskFile.seekg(currentEBRPos, std::ios::beg);
            diskFile.read(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));

            // Validar nombre único
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

    // Comando fdisk: Gestionar particiones en un disco
    inline std::string execute(int size, const std::string& unit, const std::string& path, 
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
                return "Error: La eliminación de particiones no está habilitada";
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

} // namespace CommandFdisk

#endif // FDISK_H
