#ifndef FILESYSTEM_H
#define FILESYSTEM_H

/**
 * filesystem.h - Operaciones del sistema de archivos EXT2
 * Comandos: mkdir, mkfile
 */

#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstring>
#include <ctime>
#include "structures.h"
#include "mount.h"
#include "ext2_utils.h"
#include "session.h"

namespace CommandFS {

    // Crear directorio (con soporte para -p que crea directorios intermedios)
    // path: ruta absoluta como "/home/user/docs"
    inline std::string mkdir_cmd(const std::string& path, bool createParents) {
        if (!CommandSession::isLoggedIn()) return "Error: debe iniciar sesión primero";
        if (path.empty()) return "Error: mkdir requiere -path";

        const auto& sess = CommandSession::getSession();
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(sess.mountId, part)) return "Error: partición no montada";

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);

        // Dividir la ruta en partes
        std::vector<std::string> parts;
        std::istringstream iss(path);
        std::string token;
        while (std::getline(iss, token, '/')) {
            if (!token.empty()) parts.push_back(token);
        }

        if (parts.empty()) {
            file.close();
            return "Error: ruta inválida";
        }

        int currentInode = 0; // Empezar desde raíz
        std::string currentPath = "";

        for (size_t i = 0; i < parts.size(); i++) {
            const std::string& dirName = parts[i];
            currentPath += "/" + dirName;

            int found = Ext2Utils::findInDir(file, sb, currentInode, dirName);

            if (found != -1) {
                // Ya existe
                Inode existing = Ext2Utils::readInode(file, sb, found);
                if (existing.i_type != '1') {
                    file.close();
                    return "Error: '" + currentPath + "' existe y no es un directorio";
                }
                currentInode = found;
                continue;
            }

            // No existe: crear si es el último componente o si -p está activo
            if (i < parts.size() - 1 && !createParents) {
                file.close();
                return "Error: directorio padre '" + currentPath + "' no existe. Use -p para crear directorios intermedios";
            }

            // Asignar inodo nuevo
            int newInodeNum = Ext2Utils::allocateBitmap(file, sb.s_bm_inode_start, sb.s_inodes_count);
            if (newInodeNum == -1) {
                file.close();
                return "Error: no hay inodos disponibles";
            }
            sb.s_free_inodes_count--;

            // Asignar bloque para el directorio
            int newBlockNum = Ext2Utils::allocateBitmap(file, sb.s_bm_block_start, sb.s_blocks_count);
            if (newBlockNum == -1) {
                file.close();
                return "Error: no hay bloques disponibles";
            }
            sb.s_free_blocks_count--;

            // Crear inodo del directorio
            Inode newInode;
            newInode.i_uid = sess.uid;
            newInode.i_gid = sess.gid;
            newInode.i_size = 0;
            newInode.i_atime = time(nullptr);
            newInode.i_ctime = time(nullptr);
            newInode.i_mtime = time(nullptr);
            newInode.i_type = '1';  // Es carpeta
            newInode.i_perm = 664;
            newInode.i_block[0] = newBlockNum;

            // Crear bloque de carpeta con "." y ".."
            FolderBlock fb;
            strncpy(fb.b_content[0].b_name, ".", 12);
            fb.b_content[0].b_inodo = newInodeNum;
            strncpy(fb.b_content[1].b_name, "..", 12);
            fb.b_content[1].b_inodo = currentInode;

            Ext2Utils::writeInode(file, sb, newInodeNum, newInode);
            Ext2Utils::writeFolderBlock(file, sb, newBlockNum, fb);

            // Agregar entry al directorio padre
            if (!Ext2Utils::addEntryToDir(file, sb, part.start, currentInode, dirName, newInodeNum)) {
                file.close();
                return "Error: no se pudo agregar la entrada al directorio padre";
            }

            Ext2Utils::writeSuperblock(file, part.start, sb);
            currentInode = newInodeNum;
        }

        file.close();
        return "Directorio '" + path + "' creado exitosamente";
    }

    // Crear archivo
    // -path=ruta -r (recursivo/crea padres) -size=N (bytes a llenar) -cont=contenido
    inline std::string mkfile(const std::string& path, bool recursive,
                               int size, const std::string& cont) {
        if (!CommandSession::isLoggedIn()) return "Error: debe iniciar sesión primero";
        if (path.empty()) return "Error: mkfile requiere -path";

        const auto& sess = CommandSession::getSession();
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(sess.mountId, part)) return "Error: partición no montada";

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);

        // Dividir ruta en padre y nombre del archivo
        std::string parentPath, fileName;
        Ext2Utils::splitPath(path, parentPath, fileName);

        if (fileName.empty()) {
            file.close();
            return "Error: nombre de archivo inválido";
        }

        // Navegar al directorio padre (creando si -r está activo)
        int parentInodeNum;
        if (parentPath == "/" || parentPath.empty()) {
            parentInodeNum = 0;
        } else {
            parentInodeNum = Ext2Utils::traversePath(file, sb, parentPath);
            if (parentInodeNum == -1) {
                if (!recursive) {
                    file.close();
                    return "Error: directorio padre '" + parentPath + "' no existe. Use -r para crearlo";
                }
                file.close();
                // Crear directorio padre primero
                std::string mkdirResult = mkdir_cmd(parentPath, true);
                if (mkdirResult.find("Error") != std::string::npos) {
                    return mkdirResult;
                }
                file.open(part.path, std::ios::binary | std::ios::in | std::ios::out);
                if (!file.is_open()) return "Error: no se pudo reabrir el disco";
                sb = Ext2Utils::readSuperblock(file, part.start);
                parentInodeNum = Ext2Utils::traversePath(file, sb, parentPath);
                if (parentInodeNum == -1) {
                    file.close();
                    return "Error: no se pudo crear el directorio padre";
                }
            }
        }

        // Verificar si el archivo ya existe
        int existingInode = Ext2Utils::findInDir(file, sb, parentInodeNum, fileName);
        if (existingInode != -1) {
            file.close();
            return "Error: el archivo '" + path + "' ya existe";
        }

        // Determinar contenido del archivo
        std::string fileContent;
        if (!cont.empty()) {
            fileContent = cont;
        } else if (size > 0) {
            // Llenar con números 0-9 repetidos
            for (int i = 0; i < size; i++) {
                fileContent += (char)('0' + (i % 10));
            }
        }
        // Si size=0 y sin cont, archivo vacío

        // Calcular bloques necesarios
        int blockSize = sb.s_block_size;
        int blocksNeeded = fileContent.empty() ? 0 : ((int)fileContent.size() + blockSize - 1) / blockSize;
        if (blocksNeeded > 12) {
            file.close();
            return "Error: archivo demasiado grande (máximo " + std::to_string(12 * blockSize) + " bytes)";
        }

        // Asignar inodo
        int newInodeNum = Ext2Utils::allocateBitmap(file, sb.s_bm_inode_start, sb.s_inodes_count);
        if (newInodeNum == -1) {
            file.close();
            return "Error: no hay inodos disponibles";
        }
        sb.s_free_inodes_count--;

        // Crear inodo
        Inode newInode;
        newInode.i_uid = sess.uid;
        newInode.i_gid = sess.gid;
        newInode.i_size = (int)fileContent.size();
        newInode.i_atime = time(nullptr);
        newInode.i_ctime = time(nullptr);
        newInode.i_mtime = time(nullptr);
        newInode.i_type = '0';  // Es archivo
        newInode.i_perm = 664;

        // Asignar bloques y escribir contenido
        for (int i = 0; i < blocksNeeded; i++) {
            int blockNum = Ext2Utils::allocateBitmap(file, sb.s_bm_block_start, sb.s_blocks_count);
            if (blockNum == -1) {
                file.close();
                return "Error: no hay bloques disponibles";
            }
            sb.s_free_blocks_count--;
            newInode.i_block[i] = blockNum;

            FileBlock fb;
            int offset = i * blockSize;
            int toCopy = std::min(blockSize, (int)fileContent.size() - offset);
            if (toCopy > 0) {
                memcpy(fb.b_content, fileContent.c_str() + offset, toCopy);
            }
            Ext2Utils::writeFileBlock(file, sb, blockNum, fb);
        }

        Ext2Utils::writeInode(file, sb, newInodeNum, newInode);

        // Agregar al directorio padre
        if (!Ext2Utils::addEntryToDir(file, sb, part.start, parentInodeNum, fileName, newInodeNum)) {
            file.close();
            return "Error: no se pudo agregar al directorio padre";
        }

        Ext2Utils::writeSuperblock(file, part.start, sb);
        file.close();

        return "Archivo '" + path + "' creado exitosamente\n  Tamaño: " + std::to_string(fileContent.size()) + " bytes";
    }

} // namespace CommandFS

#endif // FILESYSTEM_H
