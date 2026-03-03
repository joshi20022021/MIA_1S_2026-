#ifndef REP_H
#define REP_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <sys/stat.h>
#include <libgen.h>
#include "structures.h"
#include "mount.h"

namespace CommandRep {
    
    inline std::string toLowerCase(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    // Crear directorios recursivamente si no existen
    inline bool createDirectories(const std::string& path) {
        char tmp[1024];
        char *p = nullptr;
        size_t len;
        
        snprintf(tmp, sizeof(tmp), "%s", path.c_str());
        len = strlen(tmp);
        if (tmp[len - 1] == '/') {
            tmp[len - 1] = 0;
        }
        
        for (p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = 0;
                mkdir(tmp, S_IRWXU);
                *p = '/';
            }
        }
        mkdir(tmp, S_IRWXU);
        return true;
    }
    
    // Obtener el directorio padre de una ruta
    inline std::string getParentPath(const std::string& path) {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "%s", path.c_str());
        return std::string(dirname(tmp));
    }
    
    // Obtener extensión del archivo
    inline std::string getExtension(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos != std::string::npos) {
            return toLowerCase(path.substr(pos + 1));
        }
        return "jpg"; // Por defecto
    }
    
    // Reporte MBR - Muestra toda la información del MBR y EBR en una sola tabla
    inline std::string reportMBR(const std::string& path, const std::string& diskPath) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer MBR
        MBR mbr;
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // Generar el DOT para Graphviz
        std::ostringstream dot;
        dot << "digraph MBR_Report {\n";
        dot << "    node [shape=plaintext]\n";
        dot << "    rankdir=TB;\n\n";
        
        // Iniciar tabla única
        dot << "    mbr [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
        
        // Cabecera MBR
        dot << "        <TR><TD COLSPAN=\"2\" BGCOLOR=\"#000000\"><FONT COLOR=\"white\"><B>MBR</B></FONT></TD></TR>\n";
        dot << "        <TR><TD><B>mbr_tamano</B></TD><TD>" << mbr.mbr_size << "</TD></TR>\n";
        
        // Formatear fecha sin salto de línea
        char dateStr[100];
        struct tm* timeinfo = localtime(&mbr.mbr_creation_date);
        strftime(dateStr, sizeof(dateStr), "%d/%m/%Y %H:%M:%S", timeinfo);
        dot << "        <TR><TD><B>mbr_fecha_creacion</B></TD><TD>" << dateStr << "</TD></TR>\n";
        dot << "        <TR><TD><B>mbr_dsk_signature</B></TD><TD>" << mbr.mbr_disk_signature << "</TD></TR>\n";
        dot << "        <TR><TD><B>dsk_fit</B></TD><TD>" << mbr.disk_fit << "</TD></TR>\n";
        
        // Agregar solo las particiones que existen (status='1')
        int partNum = 1;
        for (int i = 0; i < 4; i++) {
            Partition& part = mbr.mbr_partitions[i];
            
            // Solo mostrar particiones activas
            if (part.part_status == '1') {
                // Cabecera de partición
                dot << "        <TR><TD COLSPAN=\"2\" BGCOLOR=\"#000000\"><FONT COLOR=\"white\"><B>Partition " << partNum << "</B></FONT></TD></TR>\n";
                dot << "        <TR><TD><B>status</B></TD><TD>" << part.part_status << "</TD></TR>\n";
                dot << "        <TR><TD><B>type</B></TD><TD>" << part.part_type << "</TD></TR>\n";
                dot << "        <TR><TD><B>fit</B></TD><TD>" << part.part_fit << "</TD></TR>\n";
                dot << "        <TR><TD><B>start</B></TD><TD>" << part.part_start << "</TD></TR>\n";
                dot << "        <TR><TD><B>size</B></TD><TD>" << part.part_size << "</TD></TR>\n";
                dot << "        <TR><TD><B>name</B></TD><TD>" << part.part_name << "</TD></TR>\n";
                partNum++;
            }
        }
        
        dot << "    </TABLE>>];\n\n";
        dot << "}\n";
        
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Guardar archivo .dot
        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();
        
        // Ejecutar Graphviz para generar la imagen
        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\"";
        int result = system(cmd.c_str());
        
        // Eliminar archivo .dot temporal
        remove(dotPath.c_str());
        
        if (result != 0) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        
        return "Reporte MBR generado exitosamente en: " + path;
    }
    
    // Reporte DISK - Muestra la estructura del disco con porcentajes (incluye EBR/Lógicas intercaladas)
    inline std::string reportDISK(const std::string& path, const std::string& diskPath) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer MBR
        MBR mbr;
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        int diskSize = mbr.mbr_size;
        
        // Estructura para secciones del disco
        struct DiskSection {
            std::string type;
            std::string name;
            int start;
            int size;
            double percent;
            bool isExtended;
        };
        
        std::vector<DiskSection> allSections;
        
        // Agregar MBR
        DiskSection mbrSec;
        mbrSec.type = "mbr";
        mbrSec.name = "MBR";
        mbrSec.start = 0;
        mbrSec.size = sizeof(MBR);
        mbrSec.percent = (sizeof(MBR) * 100.0) / diskSize;
        mbrSec.isExtended = false;
        allSections.push_back(mbrSec);
        
        // Procesar particiones
        for (int i = 0; i < 4; i++) {
            Partition& part = mbr.mbr_partitions[i];
            if (part.part_status == '1') {
                if (part.part_type == 'E' || part.part_type == 'e') {
                    // Partición extendida - expandir con EBR y lógicas
                    int ebr_start = part.part_start;
                    int ext_end = part.part_start + part.part_size;
                    int current_pos = part.part_start;
                    
                    while (ebr_start != -1 && ebr_start < ext_end) {
                        EBR ebr;
                        file.seekg(ebr_start, std::ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                        
                        // Espacio libre antes del EBR
                        if (ebr_start > current_pos) {
                            DiskSection freeSec;
                            freeSec.type = "free";
                            freeSec.name = "Libre";
                            freeSec.start = current_pos;
                            freeSec.size = ebr_start - current_pos;
                            freeSec.percent = (freeSec.size * 100.0) / diskSize;
                            freeSec.isExtended = true;
                            allSections.push_back(freeSec);
                        }
                        
                        if (ebr.part_status == '1') {
                            // EBR
                            DiskSection ebrSec;
                            ebrSec.type = "ebr";
                            ebrSec.name = "EBR";
                            ebrSec.start = ebr_start;
                            ebrSec.size = sizeof(EBR);
                            ebrSec.percent = (sizeof(EBR) * 100.0) / diskSize;
                            ebrSec.isExtended = true;
                            allSections.push_back(ebrSec);
                            
                            // Partición Lógica
                            DiskSection logSec;
                            logSec.type = "logical";
                            logSec.name = std::string(ebr.part_name);
                            logSec.start = ebr_start + sizeof(EBR);
                            logSec.size = ebr.part_size;
                            logSec.percent = (ebr.part_size * 100.0) / diskSize;
                            logSec.isExtended = true;
                            allSections.push_back(logSec);
                            
                            current_pos = ebr_start + sizeof(EBR) + ebr.part_size;
                        }
                        
                        ebr_start = ebr.part_next;
                        if (ebr_start <= 0) break;
                    }
                    
                    // Espacio libre al final de la extendida
                    if (current_pos < ext_end) {
                        DiskSection freeSec;
                        freeSec.type = "free";
                        freeSec.name = "Libre";
                        freeSec.start = current_pos;
                        freeSec.size = ext_end - current_pos;
                        freeSec.percent = (freeSec.size * 100.0) / diskSize;
                        freeSec.isExtended = true;
                        allSections.push_back(freeSec);
                    }
                } else {
                    // Partición primaria
                    DiskSection primSec;
                    primSec.type = "partition";
                    primSec.name = std::string(part.part_name);
                    primSec.start = part.part_start;
                    primSec.size = part.part_size;
                    primSec.percent = (part.part_size * 100.0) / diskSize;
                    primSec.isExtended = false;
                    allSections.push_back(primSec);
                }
            }
        }
        
        // Ordenar por posición
        std::sort(allSections.begin(), allSections.end(), 
                  [](const DiskSection& a, const DiskSection& b) { return a.start < b.start; });
        
        // Calcular espacios libres entre secciones (fuera de extendida)
        std::vector<DiskSection> finalSections;
        int currentPos = 0;
        
        for (const auto& sec : allSections) {
            if (!sec.isExtended && sec.start > currentPos && sec.type != "mbr") {
                DiskSection freeSec;
                freeSec.type = "free";
                freeSec.name = "Libre";
                freeSec.start = currentPos;
                freeSec.size = sec.start - currentPos;
                freeSec.percent = (freeSec.size * 100.0) / diskSize;
                freeSec.isExtended = false;
                finalSections.push_back(freeSec);
            }
            
            finalSections.push_back(sec);
            
            if (!sec.isExtended) {
                currentPos = sec.start + sec.size;
            }
        }
        
        // Espacio libre al final
        if (currentPos < diskSize) {
            DiskSection freeSec;
            freeSec.type = "free";
            freeSec.name = "Libre";
            freeSec.start = currentPos;
            freeSec.size = diskSize - currentPos;
            freeSec.percent = (freeSec.size * 100.0) / diskSize;
            freeSec.isExtended = false;
            finalSections.push_back(freeSec);
        }
        
        // Generar código DOT para Graphviz
        std::ostringstream dot;
        dot << "digraph DISK_Report {\n";
        dot << "    node [shape=plaintext]\n";
        dot << "    rankdir=LR;\n\n";
        
        // Crear tabla con dos filas: encabezado "Extendida" y contenido
        dot << "    disk [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
        
        // Primera fila: encabezados (solo "Extendida" sobre las secciones extendidas)
        dot << "        <TR>\n";
        bool inExtended = false;
        int extendedCols = 0;
        
        // Contar columnas de extendida
        for (const auto& sec : finalSections) {
            if (sec.isExtended) {
                extendedCols++;
            }
        }
        
        for (const auto& sec : finalSections) {
            if (sec.isExtended && !inExtended) {
                // Inicio de sección extendida
                inExtended = true;
                dot << "            <TD COLSPAN=\"" << extendedCols << "\" BGCOLOR=\"#FFFFFF\"><B>Extendida</B></TD>\n";
            } else if (!sec.isExtended && inExtended) {
                inExtended = false;
                dot << "            <TD></TD>\n";
            } else if (!sec.isExtended && !inExtended) {
                dot << "            <TD></TD>\n";
            }
        }
        dot << "        </TR>\n";
        
        // Segunda fila: contenido real
        dot << "        <TR>\n";
        
        for (const auto& sec : finalSections) {
            std::string color;
            
            if (sec.type == "mbr") {
                color = "#CCCCCC";
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>" << sec.name << "</B></TD>\n";
            } else if (sec.type == "free") {
                color = "#FFFFFF";
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>" << sec.name << "<BR/>"
                    << std::fixed << std::setprecision(0) << sec.percent << "% del disco</B></TD>\n";
            } else if (sec.type == "ebr") {
                color = "#B0BEC5";  // Gris más oscuro para EBR
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>" << sec.name << "</B></TD>\n";
            } else if (sec.type == "logical") {
                color = "#E3F2FD";  // Azul claro para lógicas
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>Lógica<BR/>"
                    << std::fixed << std::setprecision(0) << sec.percent << "% Del Disco</B></TD>\n";
            } else if (sec.type == "partition") {
                color = "#C8E6C9";  // Verde claro para primarias
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>Primaria<BR/>"
                    << std::fixed << std::setprecision(0) << sec.percent << "% del disco</B></TD>\n";
            }
        }
        
        dot << "        </TR>\n";
        dot << "    </TABLE>>];\n\n";
        dot << "}\n";
        
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Guardar archivo .dot
        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();
        
        // Ejecutar Graphviz para generar la imagen
        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\"";
        int result = system(cmd.c_str());
        
        // Eliminar archivo .dot temporal
        remove(dotPath.c_str());
        
        if (result != 0) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        
        return "Reporte DISK generado exitosamente en: " + path;
    }
    
    // Función principal del comando REP
    inline std::string execute(const std::string& name, const std::string& path, 
                               const std::string& id, const std::string& pathFileLs) {
        // Validar parámetros obligatorios
        if (name.empty()) {
            return "Error: rep requiere el parámetro -name";
        }
        if (path.empty()) {
            return "Error: rep requiere el parámetro -path";
        }
        if (id.empty()) {
            return "Error: rep requiere el parámetro -id";
        }
        
        std::string reportType = toLowerCase(name);
        
        // Validar tipo de reporte
        if (reportType != "mbr" && reportType != "disk" && reportType != "inode" && 
            reportType != "block" && reportType != "bm_inode" && reportType != "bm_block" && 
            reportType != "tree" && reportType != "sb" && reportType != "file" && reportType != "ls") {
            return "Error: tipo de reporte no válido. Valores permitidos: mbr, disk, inode, block, bm_inode, bm_block, tree, sb, file, ls";
        }
        
        // Buscar la partición montada
        CommandMount::MountedPartition partition;
        if (!CommandMount::getMountedPartition(id, partition)) {
            return "Error: la partición con ID '" + id + "' no está montada";
        }
        
        std::ostringstream result;
        result << "\n=== REP ===\n";
        result << "Generando reporte '" << reportType << "'...\n";
        
        // Ejecutar el reporte correspondiente
        if (reportType == "mbr") {
            std::string res = reportMBR(path, partition.path);
            result << res << "\n";
        } else if (reportType == "disk") {
            std::string res = reportDISK(path, partition.path);
            result << res << "\n";
        } else {
            result << "Reporte '" << reportType << "' aún no implementado\n";
        }
        
        return result.str();
    }
    
} // namespace CommandRep

#endif // REP_H
