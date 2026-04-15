#ifndef SESSION_H
#define SESSION_H

/**
 * session.h - Manejo de sesiones de usuario
 * Comandos: login, logout
 */

#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstring>
#include "structures.h"
#include "mount.h"
#include "ext2_utils.h"

namespace CommandSession {

    // Estado de sesión global
    struct SessionInfo {
        bool active = false;
        std::string username;
        std::string groupName;
        int uid = -1;
        int gid = -1;
        std::string mountId;  // ID de la partición donde está logueado
    };

    static SessionInfo currentSession;

    // Verificar si hay sesión activa
    inline bool isLoggedIn() {
        return currentSession.active;
    }

    // Obtener sesión actual
    inline const SessionInfo& getSession() {
        return currentSession;
    }

    // Parsear una línea de users.txt
    // Formato: ID,Tipo,Grupo[,Usuario,Contraseña]
    struct UserEntry {
        int id;
        char type;         // 'G' o 'U'
        std::string group;
        std::string username;
        std::string password;
    };

    inline std::vector<UserEntry> parseUsersFile(const std::string& content) {
        std::vector<UserEntry> entries;
        std::istringstream iss(content);
        std::string line;

        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            std::istringstream lineStream(line);
            std::string token;
            std::vector<std::string> parts;
            while (std::getline(lineStream, token, ',')) {
                parts.push_back(token);
            }
            if (parts.size() < 3) continue;

            UserEntry entry;
            try { entry.id = std::stoi(parts[0]); } catch(...) { continue; }
            entry.type = parts[1][0];
            entry.group = parts[2];

            if (entry.type == 'U' && parts.size() >= 5) {
                entry.username = parts[3];
                entry.password = parts[4];
            }

            entries.push_back(entry);
        }
        return entries;
    }

    // Leer contenido de users.txt desde la partición activa
    inline std::string readUsersContent(std::fstream& file, const Superblock& sb) {
        // users.txt siempre está en inodo 1
        Inode usersInode = Ext2Utils::readInode(file, sb, 1);
        return Ext2Utils::readFileContent(file, sb, usersInode);
    }

    // Comando login
    inline std::string login(const std::string& user, const std::string& pass, const std::string& id) {
        if (currentSession.active) {
            return "Error: ya hay una sesión activa. Use logout primero";
        }

        if (user.empty() || pass.empty() || id.empty()) {
            return "Error: login requiere -user, -pass e -id";
        }

        // Obtener partición montada
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(id, part)) {
            return "Error: la partición '" + id + "' no está montada";
        }

        std::fstream file(part.path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco";
        }

        Superblock sb = Ext2Utils::readSuperblock(file, part.start);
        std::string content = readUsersContent(file, sb);
        file.close();

        auto entries = parseUsersFile(content);

        // Buscar usuario
        for (const auto& entry : entries) {
            if (entry.type == 'U' && entry.id != 0 &&
                entry.username == user && entry.password == pass) {
                // Encontrar su grupo para obtener gid
                int gid = -1;
                for (const auto& ge : entries) {
                    if (ge.type == 'G' && ge.id != 0 && ge.group == entry.group) {
                        gid = ge.id;
                        break;
                    }
                }
                currentSession.active = true;
                currentSession.username = user;
                currentSession.groupName = entry.group;
                currentSession.uid = entry.id;
                currentSession.gid = gid;
                currentSession.mountId = id;

                return "Bienvenido " + user + "\n  Grupo: " + entry.group +
                       "\n  UID: " + std::to_string(entry.id) +
                       "\n  GID: " + std::to_string(gid);
            }
        }

        return "Error: usuario o contraseña incorrectos";
    }

    // Comando logout
    inline std::string logout() {
        if (!currentSession.active) {
            return "Error: no hay sesión activa";
        }
        std::string user = currentSession.username;
        currentSession = SessionInfo{};
        return "Sesión de '" + user + "' cerrada exitosamente";
    }

} // namespace CommandSession

#endif // SESSION_H
