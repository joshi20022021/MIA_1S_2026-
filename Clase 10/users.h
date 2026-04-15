#ifndef USERS_H
#define USERS_H

/**
 * users.h - Gestión de usuarios y grupos
 * Comandos: mkgrp, rmgrp, mkusr, rmusr, chgrp, cat
 */

#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstring>
#include "structures.h"
#include "mount.h"
#include "ext2_utils.h"
#include "session.h"

namespace CommandUsers {

    using UserEntry = CommandSession::UserEntry;

    // Leer contenido de users.txt
    inline std::string readUsers(std::fstream& file, const Superblock& sb) {
        Inode inode = Ext2Utils::readInode(file, sb, 1);
        return Ext2Utils::readFileContent(file, sb, inode);
    }

    // Escribir contenido de users.txt
    inline bool writeUsers(std::fstream& file, Superblock& sb, int partStart, const std::string& content) {
        return Ext2Utils::writeFileContent(file, sb, 1, partStart, content);
    }

    // Parsear entries de users.txt
    inline std::vector<UserEntry> parseUsers(const std::string& content) {
        return CommandSession::parseUsersFile(content);
    }

    // Serializar entries a string
    inline std::string serializeUsers(const std::vector<UserEntry>& entries) {
        std::ostringstream oss;
        for (const auto& e : entries) {
            if (e.type == 'G') {
                oss << e.id << ",G," << e.group << "\n";
            } else {
                oss << e.id << ",U," << e.group << "," << e.username << "," << e.password << "\n";
            }
        }
        return oss.str();
    }

    // Verificar que el usuario actual es root
    inline bool isRoot() {
        return CommandSession::isLoggedIn() &&
               CommandSession::getSession().username == "root";
    }

    // mkgrp -name=nombre
    inline std::string mkgrp(const std::string& name) {
        if (!CommandSession::isLoggedIn()) {
            return "Error: debe iniciar sesión primero";
        }
        if (!isRoot()) {
            return "Error: solo root puede crear grupos";
        }
        if (name.empty()) {
            return "Error: mkgrp requiere -name";
        }

        const auto& sess = CommandSession::getSession();
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(sess.mountId, part)) {
            return "Error: partición no montada";
        }

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);
        std::string content = readUsers(file, sb);
        auto entries = parseUsers(content);

        // Verificar que no existe el grupo
        for (const auto& e : entries) {
            if (e.type == 'G' && e.id != 0 && e.group == name) {
                file.close();
                return "Error: ya existe el grupo '" + name + "'";
            }
        }

        // Obtener el mayor GID
        int maxId = 0;
        for (const auto& e : entries) {
            if (e.id > maxId) maxId = e.id;
        }

        UserEntry newGroup;
        newGroup.id = maxId + 1;
        newGroup.type = 'G';
        newGroup.group = name;
        entries.push_back(newGroup);

        std::string newContent = serializeUsers(entries);
        writeUsers(file, sb, part.start, newContent);
        file.close();

        return "Grupo '" + name + "' creado con GID " + std::to_string(newGroup.id);
    }

    // rmgrp -name=nombre
    inline std::string rmgrp(const std::string& name) {
        if (!CommandSession::isLoggedIn()) return "Error: debe iniciar sesión primero";
        if (!isRoot()) return "Error: solo root puede eliminar grupos";
        if (name.empty()) return "Error: rmgrp requiere -name";

        const auto& sess = CommandSession::getSession();
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(sess.mountId, part)) return "Error: partición no montada";

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);
        std::string content = readUsers(file, sb);
        auto entries = parseUsers(content);

        bool found = false;
        for (auto& e : entries) {
            if (e.type == 'G' && e.group == name) {
                e.id = 0;  // Marcar como eliminado
                found = true;
                break;
            }
        }

        if (!found) {
            file.close();
            return "Error: grupo '" + name + "' no encontrado";
        }

        // También marcar como eliminados los usuarios del grupo
        for (auto& e : entries) {
            if (e.type == 'U' && e.group == name) {
                e.id = 0;
            }
        }

        std::string newContent = serializeUsers(entries);
        writeUsers(file, sb, part.start, newContent);
        file.close();

        return "Grupo '" + name + "' eliminado";
    }

    // mkusr -user=nombre -pass=contraseña -grp=grupo
    inline std::string mkusr(const std::string& user, const std::string& pass, const std::string& grp) {
        if (!CommandSession::isLoggedIn()) return "Error: debe iniciar sesión primero";
        if (!isRoot()) return "Error: solo root puede crear usuarios";
        if (user.empty() || pass.empty() || grp.empty()) return "Error: mkusr requiere -user, -pass y -grp";

        const auto& sess = CommandSession::getSession();
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(sess.mountId, part)) return "Error: partición no montada";

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);
        std::string content = readUsers(file, sb);
        auto entries = parseUsers(content);

        // Verificar que el grupo existe
        bool groupExists = false;
        for (const auto& e : entries) {
            if (e.type == 'G' && e.id != 0 && e.group == grp) {
                groupExists = true;
                break;
            }
        }
        if (!groupExists) {
            file.close();
            return "Error: el grupo '" + grp + "' no existe";
        }

        // Verificar que el usuario no existe
        for (const auto& e : entries) {
            if (e.type == 'U' && e.id != 0 && e.username == user) {
                file.close();
                return "Error: ya existe el usuario '" + user + "'";
            }
        }

        int maxId = 0;
        for (const auto& e : entries) {
            if (e.id > maxId) maxId = e.id;
        }

        UserEntry newUser;
        newUser.id = maxId + 1;
        newUser.type = 'U';
        newUser.group = grp;
        newUser.username = user;
        newUser.password = pass;
        entries.push_back(newUser);

        std::string newContent = serializeUsers(entries);
        writeUsers(file, sb, part.start, newContent);
        file.close();

        return "Usuario '" + user + "' creado con UID " + std::to_string(newUser.id);
    }

    // rmusr -user=nombre
    inline std::string rmusr(const std::string& user) {
        if (!CommandSession::isLoggedIn()) return "Error: debe iniciar sesión primero";
        if (!isRoot()) return "Error: solo root puede eliminar usuarios";
        if (user.empty()) return "Error: rmusr requiere -user";

        const auto& sess = CommandSession::getSession();
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(sess.mountId, part)) return "Error: partición no montada";

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);
        std::string content = readUsers(file, sb);
        auto entries = parseUsers(content);

        bool found = false;
        for (auto& e : entries) {
            if (e.type == 'U' && e.username == user) {
                e.id = 0;
                found = true;
                break;
            }
        }

        if (!found) {
            file.close();
            return "Error: usuario '" + user + "' no encontrado";
        }

        std::string newContent = serializeUsers(entries);
        writeUsers(file, sb, part.start, newContent);
        file.close();

        return "Usuario '" + user + "' eliminado";
    }

    // chgrp -user=nombre -grp=grupo
    inline std::string chgrp(const std::string& user, const std::string& grp) {
        if (!CommandSession::isLoggedIn()) return "Error: debe iniciar sesión primero";
        if (!isRoot()) return "Error: solo root puede cambiar grupos";
        if (user.empty() || grp.empty()) return "Error: chgrp requiere -user y -grp";

        const auto& sess = CommandSession::getSession();
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(sess.mountId, part)) return "Error: partición no montada";

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);
        std::string content = readUsers(file, sb);
        auto entries = parseUsers(content);

        // Verificar grupo existe
        bool groupExists = false;
        for (const auto& e : entries) {
            if (e.type == 'G' && e.id != 0 && e.group == grp) {
                groupExists = true;
                break;
            }
        }
        if (!groupExists) {
            file.close();
            return "Error: el grupo '" + grp + "' no existe";
        }

        bool found = false;
        for (auto& e : entries) {
            if (e.type == 'U' && e.id != 0 && e.username == user) {
                e.group = grp;
                found = true;
                break;
            }
        }

        if (!found) {
            file.close();
            return "Error: usuario '" + user + "' no encontrado";
        }

        std::string newContent = serializeUsers(entries);
        writeUsers(file, sb, part.start, newContent);
        file.close();

        return "Usuario '" + user + "' movido al grupo '" + grp + "'";
    }

    // cat -path=ruta
    // Lee el contenido de un archivo desde el EXT2 filesystem
    inline std::string cat(const std::string& filePath, const std::string& mountId) {
        if (!CommandSession::isLoggedIn()) return "Error: debe iniciar sesión primero";

        std::string id = mountId;
        if (id.empty()) id = CommandSession::getSession().mountId;

        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(id, part)) return "Error: partición no montada";

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);

        int inodeNum = Ext2Utils::traversePath(file, sb, filePath);
        if (inodeNum == -1) {
            file.close();
            return "Error: el archivo '" + filePath + "' no existe";
        }

        Inode inode = Ext2Utils::readInode(file, sb, inodeNum);
        if (inode.i_type != '0') {
            file.close();
            return "Error: '" + filePath + "' es un directorio";
        }

        std::string content = Ext2Utils::readFileContent(file, sb, inode);
        file.close();

        return content;
    }

} // namespace CommandUsers

#endif // USERS_H
