#ifndef MOUNT_H
#define MOUNT_H

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>
#include "structures.h"

namespace CommandMount {

    // Estructura para almacenar información de particiones montadas
    struct MountedPartition {
        std::string path;          // Ruta del disco
        std::string name;          // Nombre de la partición
        std::string id;            // ID de montaje (341A, 342B, etc.)
        char type;                 // Tipo de partición (P, E, L)
        int start;                 // Byte donde inicia la partición
        int size;                  // Tamaño de la partición
    };

    // Mapa global para almacenar particiones montadas
    static std::map<std::string, MountedPartition> mountedPartitions;

    // Mapa para llevar el conteo de discos montados
    // Key: ruta del disco, Value: letra asignada (A, B, C, etc.)
    static std::map<std::string, char> diskLetters;

    // Contador para la siguiente letra de disco disponible
    static char nextDiskLetter = 'A';

    // Carnet del estudiante (últimos 2 dígitos)
    static const std::string CARNET_SUFFIX = "34";

    // Función auxiliar para expandir ~ a home directory
    inline std::string expandPath(const std::string& path) {
        if (path.empty() || path[0] != '~') {
            return path;
        }

        const char* home = std::getenv("HOME");
        if (!home) {
            home = std::getenv("USERPROFILE"); // Windows
        }

        if (home) {
            return std::string(home) + path.substr(1);
        }
        return path;
    }

    // Función para buscar una partición en el MBR
    // Retorna: tipo de partición, posición de inicio, tamaño, índice en MBR (para primarias) o -1 (lógica)
    inline bool findPartitionInMBR(const std::string& path, const std::string& name,
                                    char& type, int& start, int& size,
                                    int& mbrIndex, int& ebrPos) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        MBR mbr;
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                std::string partName(mbr.mbr_partitions[i].part_name);
                partName = partName.c_str(); // trim at null terminator

                if (partName == name) {
                    type = mbr.mbr_partitions[i].part_type;
                    start = mbr.mbr_partitions[i].part_start;
                    size = mbr.mbr_partitions[i].part_size;
                    mbrIndex = i;
                    ebrPos = -1;
                    file.close();
                    return true;
                }

                // Si es extendida, buscar en particiones lógicas
                if (mbr.mbr_partitions[i].part_type == 'E') {
                    int pos = mbr.mbr_partitions[i].part_start;
                    while (pos != -1) {
                        EBR ebr;
                        file.seekg(pos, std::ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));

                        if (ebr.part_status == '1') {
                            std::string ebrName(ebr.part_name);
                            ebrName = ebrName.c_str();

                            if (ebrName == name) {
                                type = 'L';
                                start = ebr.part_start;
                                size = ebr.part_size;
                                mbrIndex = -1;
                                ebrPos = pos;
                                file.close();
                                return true;
                            }
                        }

                        pos = ebr.part_next;
                    }
                }
            }
        }

        file.close();
        return false;
    }

    // Función para generar el ID de montaje: CARNET + correlative + diskLetter
    // Ej: carnet=34, primera partición del disco A -> "341A"
    inline std::string generateMountID(const std::string& path, int& outCorrelative, char& outLetter) {
        // Asignar letra al disco si no tiene
        auto it = diskLetters.find(path);
        if (it == diskLetters.end()) {
            diskLetters[path] = nextDiskLetter++;
        }
        char diskLetter = diskLetters[path];
        outLetter = diskLetter;

        // Contar particiones de este disco para determinar correlativo
        int correlative = 1;
        for (const auto& [id, partition] : mountedPartitions) {
            if (partition.path == path) {
                correlative++;
            }
        }
        outCorrelative = correlative;

        // ID: carnet_suffix + correlative + diskLetter
        return CARNET_SUFFIX + std::to_string(correlative) + diskLetter;
    }

    // Función para verificar si una partición ya está montada
    inline bool isPartitionMounted(const std::string& path, const std::string& name) {
        for (const auto& [id, partition] : mountedPartitions) {
            if (partition.path == path && partition.name == name) {
                return true;
            }
        }
        return false;
    }

    // Escribir el ID de montaje en el MBR (partición primaria/extendida) o EBR (lógica)
    inline void writeMountIDToDisk(const std::string& path, int mbrIndex, int ebrPos,
                                    int correlative, const std::string& mountID) {
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return;

        if (mbrIndex >= 0) {
            // Partición primaria o extendida: actualizar en MBR
            MBR mbr;
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            mbr.mbr_partitions[mbrIndex].part_correlative = correlative;
            memset(mbr.mbr_partitions[mbrIndex].part_id, 0, 4);
            strncpy(mbr.mbr_partitions[mbrIndex].part_id, mountID.c_str(), 4);
            file.seekp(0, std::ios::beg);
            file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        } else if (ebrPos >= 0) {
            // Partición lógica: actualizar en EBR (EBR no tiene part_id, skip for now)
            // EBR struct doesn't have part_id so we just skip
        }
        file.close();
    }

    // Función principal para ejecutar el comando mount (retorna string)
    inline std::string execute(const std::string& pathParam, const std::string& nameParam) {
        std::string path = expandPath(pathParam);
        std::string name = nameParam;

        // Verificar que el archivo del disco existe
        std::ifstream file(path);
        if (!file.good()) {
            return "Error: el disco '" + path + "' no existe";
        }
        file.close();

        // Verificar si la partición ya está montada
        if (isPartitionMounted(path, name)) {
            return "Error: la partición '" + name + "' en '" + path + "' ya está montada";
        }

        // Buscar la partición en el disco
        char type;
        int start, size, mbrIndex, ebrPos;
        if (!findPartitionInMBR(path, name, type, start, size, mbrIndex, ebrPos)) {
            return "Error: no se encontró la partición '" + name + "' en el disco '" + path + "'";
        }

        // Generar ID de montaje
        int correlative;
        char diskLetter;
        std::string mountID = generateMountID(path, correlative, diskLetter);

        // Crear estructura de partición montada
        MountedPartition mounted;
        mounted.path = path;
        mounted.name = name;
        mounted.id = mountID;
        mounted.type = type;
        mounted.start = start;
        mounted.size = size;

        // Agregar al mapa de particiones montadas
        mountedPartitions[mountID] = mounted;

        // Actualizar en disco
        writeMountIDToDisk(path, mbrIndex, ebrPos, correlative, mountID);

        std::ostringstream result;
        result << "\n=== MOUNT ===\n";
        result << "Partición montada exitosamente\n";
        result << "  ID: " << mountID << "\n";
        result << "  Disco: " << path << "\n";
        result << "  Partición: " << name << "\n";
        result << "  Tipo: " << type << "\n";
        result << "  Inicio: " << start << " bytes\n";
        result << "  Tamaño: " << size << " bytes";

        return result.str();
    }

    // Función para listar todas las particiones montadas (retorna string)
    inline std::string listMountedPartitions() {
        if (mountedPartitions.empty()) {
            return "No hay particiones montadas";
        }

        std::ostringstream result;
        result << "\n=== PARTICIONES MONTADAS ===\n";
        for (const auto& [id, partition] : mountedPartitions) {
            result << "ID: " << id << "\n";
            result << "  Disco: " << partition.path << "\n";
            result << "  Partición: " << partition.name << "\n";
            result << "  Tipo: " << partition.type << "\n";
            result << "  Inicio: " << partition.start << " bytes\n";
            result << "  Tamaño: " << partition.size << " bytes\n";
            result << "---\n";
        }
        return result.str();
    }

    // Función para mostrar todas las particiones montadas (imprime en consola)
    inline void showMountedPartitions() {
        std::cout << listMountedPartitions() << std::endl;
    }

    // Función auxiliar para obtener información de una partición montada por su ID
    inline bool getMountedPartition(const std::string& id, MountedPartition& partition) {
        auto it = mountedPartitions.find(id);
        if (it != mountedPartitions.end()) {
            partition = it->second;
            return true;
        }
        return false;
    }

} // namespace CommandMount

#endif // MOUNT_H
