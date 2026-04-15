#ifndef EXT2_UTILS_H
#define EXT2_UTILS_H

/**
 * Utilidades compartidas para leer/escribir el sistema de archivos EXT2.
 * Usadas por session.h, users.h y filesystem.h.
 */

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include "structures.h"
#include "mount.h"

namespace Ext2Utils {

    // Leer un inodo dado su número
    inline Inode readInode(std::fstream& file, const Superblock& sb, int inodeNum) {
        Inode inode;
        file.seekg(sb.s_inode_start + inodeNum * sizeof(Inode), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
        return inode;
    }

    // Escribir un inodo dado su número
    inline void writeInode(std::fstream& file, const Superblock& sb, int inodeNum, const Inode& inode) {
        file.seekp(sb.s_inode_start + inodeNum * sizeof(Inode), std::ios::beg);
        file.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));
    }

    // Leer un bloque de carpeta
    inline FolderBlock readFolderBlock(std::fstream& file, const Superblock& sb, int blockNum) {
        FolderBlock fb;
        file.seekg(sb.s_block_start + blockNum * sb.s_block_size, std::ios::beg);
        file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
        return fb;
    }

    // Escribir un bloque de carpeta
    inline void writeFolderBlock(std::fstream& file, const Superblock& sb, int blockNum, const FolderBlock& fb) {
        file.seekp(sb.s_block_start + blockNum * sb.s_block_size, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&fb), sizeof(FolderBlock));
    }

    // Leer un bloque de archivo
    inline FileBlock readFileBlock(std::fstream& file, const Superblock& sb, int blockNum) {
        FileBlock fb;
        file.seekg(sb.s_block_start + blockNum * sb.s_block_size, std::ios::beg);
        file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
        return fb;
    }

    // Escribir un bloque de archivo
    inline void writeFileBlock(std::fstream& file, const Superblock& sb, int blockNum, const FileBlock& fb) {
        file.seekp(sb.s_block_start + blockNum * sb.s_block_size, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&fb), sizeof(FileBlock));
    }

    // Leer superbloque
    inline Superblock readSuperblock(std::fstream& file, int partStart) {
        Superblock sb;
        file.seekg(partStart, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
        return sb;
    }

    // Escribir superbloque
    inline void writeSuperblock(std::fstream& file, int partStart, const Superblock& sb) {
        file.seekp(partStart, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
    }

    // Buscar un bitmap libre y marcarlo como usado; retorna índice o -1
    inline int allocateBitmap(std::fstream& file, int bitmapStart, int count) {
        file.seekg(bitmapStart, std::ios::beg);
        for (int i = 0; i < count; i++) {
            char bit;
            file.read(&bit, 1);
            if (bit == '0') {
                char used = '1';
                file.seekp(bitmapStart + i, std::ios::beg);
                file.write(&used, 1);
                return i;
            }
        }
        return -1;
    }

    // Liberar un bitmap (marcar como libre)
    inline void freeBitmap(std::fstream& file, int bitmapStart, int index) {
        char free_ = '0';
        file.seekp(bitmapStart + index, std::ios::beg);
        file.write(&free_, 1);
    }

    // Leer todo el contenido de un archivo dado su inodo
    inline std::string readFileContent(std::fstream& file, const Superblock& sb, const Inode& inode) {
        std::string content;
        // Bloques directos [0..11]
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) break;
            FileBlock fb = readFileBlock(file, sb, inode.i_block[i]);
            content.append(fb.b_content, sb.s_block_size);
        }
        // Bloque indirecto simple [12]
        if (inode.i_block[12] != -1) {
            PointerBlock pb;
            file.seekg(sb.s_block_start + inode.i_block[12] * sb.s_block_size, std::ios::beg);
            file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));
            for (int i = 0; i < 16; i++) {
                if (pb.b_pointers[i] == -1) break;
                FileBlock fb = readFileBlock(file, sb, pb.b_pointers[i]);
                content.append(fb.b_content, sb.s_block_size);
            }
        }
        // Trim at actual size
        if ((int)content.size() > inode.i_size) {
            content.resize(inode.i_size);
        }
        return content;
    }

    // Escribir contenido completo en un archivo (sobreescribe; asigna bloques nuevos)
    // Retorna false si no hay espacio
    inline bool writeFileContent(std::fstream& file, Superblock& sb, int inodeNum,
                                  int partStart, const std::string& content) {
        Inode inode = readInode(file, sb, inodeNum);
        // Liberar bloques actuales
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] != -1) {
                freeBitmap(file, sb.s_bm_block_start, inode.i_block[i]);
                inode.i_block[i] = -1;
                sb.s_free_blocks_count++;
            }
        }

        int blockSize = sb.s_block_size;
        int needed = ((int)content.size() + blockSize - 1) / blockSize;
        if (needed > 12) needed = 12; // limit to direct blocks for now

        for (int i = 0; i < needed; i++) {
            int blockNum = allocateBitmap(file, sb.s_bm_block_start, sb.s_blocks_count);
            if (blockNum == -1) return false;
            sb.s_free_blocks_count--;
            inode.i_block[i] = blockNum;
            FileBlock fb;
            int offset = i * blockSize;
            int toCopy = std::min(blockSize, (int)content.size() - offset);
            memcpy(fb.b_content, content.c_str() + offset, toCopy);
            writeFileBlock(file, sb, blockNum, fb);
        }

        inode.i_size = (int)content.size();
        inode.i_mtime = time(nullptr);
        writeInode(file, sb, inodeNum, inode);
        writeSuperblock(file, partStart, sb);
        return true;
    }

    // Buscar un entry en un directorio (inodo de carpeta) por nombre
    // Retorna inodeNum del entry o -1
    inline int findInDir(std::fstream& file, const Superblock& sb, int dirInodeNum, const std::string& name) {
        Inode dirInode = readInode(file, sb, dirInodeNum);
        for (int i = 0; i < 12; i++) {
            if (dirInode.i_block[i] == -1) break;
            FolderBlock fb = readFolderBlock(file, sb, dirInode.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                std::string entryName(fb.b_content[j].b_name);
                if (entryName == name) {
                    return fb.b_content[j].b_inodo;
                }
            }
        }
        return -1;
    }

    // Recorrer ruta desde la raíz; retorna inodeNum del destino o -1
    // path: "/dir1/dir2/file.txt"
    inline int traversePath(std::fstream& file, const Superblock& sb, const std::string& path) {
        if (path == "/" || path.empty()) return 0;

        std::istringstream iss(path);
        std::string token;
        int currentInode = 0;

        while (std::getline(iss, token, '/')) {
            if (token.empty()) continue;
            int found = findInDir(file, sb, currentInode, token);
            if (found == -1) return -1;
            currentInode = found;
        }
        return currentInode;
    }

    // Obtener directorio padre e hijo de una ruta
    // "/home/user/file.txt" -> parent="/home/user", child="file.txt"
    inline void splitPath(const std::string& path, std::string& parent, std::string& child) {
        size_t pos = path.rfind('/');
        if (pos == std::string::npos || pos == 0) {
            parent = "/";
            child = (pos == 0) ? path.substr(1) : path;
        } else {
            parent = path.substr(0, pos);
            child = path.substr(pos + 1);
        }
    }

    // Agregar un entry a un directorio
    inline bool addEntryToDir(std::fstream& file, Superblock& sb, int partStart,
                               int dirInodeNum, const std::string& name, int childInode) {
        Inode dirInode = readInode(file, sb, dirInodeNum);
        for (int i = 0; i < 12; i++) {
            int blockIdx;
            FolderBlock fb;
            if (dirInode.i_block[i] == -1) {
                // Asignar nuevo bloque
                int blockNum = allocateBitmap(file, sb.s_bm_block_start, sb.s_blocks_count);
                if (blockNum == -1) return false;
                sb.s_free_blocks_count--;
                dirInode.i_block[i] = blockNum;
                blockIdx = blockNum;
            } else {
                blockIdx = dirInode.i_block[i];
                fb = readFolderBlock(file, sb, blockIdx);
            }

            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) {
                    strncpy(fb.b_content[j].b_name, name.c_str(), 12);
                    fb.b_content[j].b_inodo = childInode;
                    writeFolderBlock(file, sb, blockIdx, fb);
                    dirInode.i_mtime = time(nullptr);
                    writeInode(file, sb, dirInodeNum, dirInode);
                    writeSuperblock(file, partStart, sb);
                    return true;
                }
            }
        }
        return false; // No hay espacio
    }

    // Abrir disco y leer superbloque por ID de partición montada
    inline bool openPartition(const std::string& mountId, std::fstream& file, Superblock& sb) {
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(mountId, part)) {
            return false;
        }
        file.open(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return false;
        sb = readSuperblock(file, part.start);
        return true;
    }

} // namespace Ext2Utils

#endif // EXT2_UTILS_H
