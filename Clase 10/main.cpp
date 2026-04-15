#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>
#include "structures.h"
#include "mkdisk.h"
#include "rmdisk.h"
#include "fdisk.h"
#include "mount.h"
#include "mkfs.h"
#include "ext2_utils.h"
#include "session.h"
#include "users.h"
#include "filesystem.h"
#include "journaling.h"

std::string toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string removeQuotes(const std::string& str) {
    if (str.length() >= 2 &&
        ((str.front() == '"' && str.back() == '"') ||
         (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

std::string parseParameter(const std::string& commandLine, const std::string& paramName) {
    std::string lowerCommandLine = toLowerCase(commandLine);
    std::string lowerParamName = toLowerCase(paramName);

    size_t pos = lowerCommandLine.find(lowerParamName + "=");
    if (pos == std::string::npos) return "";

    size_t valueStart = pos + paramName.length() + 1;
    if (valueStart >= commandLine.length()) return "";

    size_t valueEnd = commandLine.length();

    if (commandLine[valueStart] == '"' || commandLine[valueStart] == '\'') {
        char quote = commandLine[valueStart];
        size_t quoteEnd = commandLine.find(quote, valueStart + 1);
        if (quoteEnd != std::string::npos) {
            return commandLine.substr(valueStart + 1, quoteEnd - valueStart - 1);
        } else {
            size_t spacePos = commandLine.find(' ', valueStart + 1);
            if (spacePos != std::string::npos) valueEnd = spacePos;
            return commandLine.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
    } else {
        size_t spacePos = commandLine.find(' ', valueStart);
        if (spacePos != std::string::npos) valueEnd = spacePos;
        return commandLine.substr(valueStart, valueEnd - valueStart);
    }
}

std::string executeCommand(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string cmd;
    iss >> cmd;
    cmd = toLowerCase(cmd);

    auto isErrorResult = [](const std::string& result) {
        return result.rfind("Error", 0) == 0 || result.rfind("error", 0) == 0;
    };

    if (cmd == "mkdisk") {
        std::string sizeStr = parseParameter(commandLine, "-size");
        std::string unit    = parseParameter(commandLine, "-unit");
        std::string path    = parseParameter(commandLine, "-path");

        if (sizeStr.empty() || path.empty())
            return "Error: mkdisk requiere -size y -path";

        int size;
        try { size = std::stoi(sizeStr); } catch(...) {
            return "Error: size debe ser un entero positivo";
        }
        if (size <= 0) return "Error: el tamaño debe ser positivo";

        if (unit.empty()) unit = "m";
        else unit = toLowerCase(unit);

        if (unit != "k" && unit != "m" && unit != "b")
            return "Error: unit debe ser 'b', 'k' o 'm'";

        return CommandMkdisk::execute(size, unit, path);

    } else if (cmd == "rmdisk") {
        std::string path = parseParameter(commandLine, "-path");
        if (path.empty()) return "Error: rmdisk requiere -path";
        return CommandRmdisk::execute(path);

    } else if (cmd == "fdisk") {
        std::string path       = parseParameter(commandLine, "-path");
        std::string name       = parseParameter(commandLine, "-name");
        std::string deleteName = parseParameter(commandLine, "-delete");

        if (path.empty()) return "Error: fdisk requiere -path";

        if (!deleteName.empty())
            return CommandFdisk::execute(0, "", path, "", "", deleteName, "");

        if (name.empty()) return "Error: fdisk requiere -name o -delete";

        std::string sizeStr = parseParameter(commandLine, "-size");
        if (sizeStr.empty()) return "Error: fdisk requiere -size para crear particiones";

        int size;
        try { size = std::stoi(sizeStr); } catch(...) {
            return "Error: size debe ser un entero positivo";
        }
        if (size <= 0) return "Error: el tamaño debe ser positivo";

        std::string unit = parseParameter(commandLine, "-unit");
        if (unit.empty()) unit = "k";
        else unit = toLowerCase(unit);

        if (unit != "k" && unit != "m" && unit != "b")
            return "Error: unit debe ser 'b', 'k' o 'm'";

        std::string type = parseParameter(commandLine, "-type");
        if (type.empty()) type = "P";
        else type = toLowerCase(type);

        std::string fit = parseParameter(commandLine, "-fit");
        if (fit.empty()) fit = "WF";
        else fit = toLowerCase(fit);

        return CommandFdisk::execute(size, unit, path, type, fit, "", name);

    } else if (cmd == "mount") {
        std::string path = parseParameter(commandLine, "-path");
        std::string name = parseParameter(commandLine, "-name");
        if (path.empty() || name.empty())
            return "Error: mount requiere -path y -name";
        return CommandMount::execute(path, name);

    } else if (cmd == "mounted") {
        return CommandMount::listMountedPartitions();

    } else if (cmd == "mkfs") {
        std::string id   = parseParameter(commandLine, "-id");
        std::string type = parseParameter(commandLine, "-type");
        if (id.empty()) return "Error: mkfs requiere -id";
        std::string result = CommandMkfs::execute(id, type);
        if (!isErrorResult(result)) CommandJournaling::clearFor(id);
        return result;

    } else if (cmd == "login") {
        std::string user = parseParameter(commandLine, "-user");
        std::string pass = parseParameter(commandLine, "-pass");
        std::string id   = parseParameter(commandLine, "-id");
        if (user.empty() || pass.empty() || id.empty())
            return "Error: login requiere -user, -pass e -id";
        return CommandSession::login(user, pass, id);

    } else if (cmd == "logout") {
        return CommandSession::logout();

    } else if (cmd == "mkgrp") {
        std::string name = parseParameter(commandLine, "-name");
        std::string result = CommandUsers::mkgrp(name);
        if (!isErrorResult(result) && CommandSession::isLoggedIn())
            CommandJournaling::add(CommandSession::getSession().mountId, "mkgrp", "/users.txt", name);
        return result;

    } else if (cmd == "rmgrp") {
        std::string name = parseParameter(commandLine, "-name");
        std::string result = CommandUsers::rmgrp(name);
        if (!isErrorResult(result) && CommandSession::isLoggedIn())
            CommandJournaling::add(CommandSession::getSession().mountId, "rmgrp", "/users.txt", name);
        return result;

    } else if (cmd == "mkusr") {
        std::string user = parseParameter(commandLine, "-user");
        std::string pass = parseParameter(commandLine, "-pass");
        std::string grp  = parseParameter(commandLine, "-grp");
        std::string result = CommandUsers::mkusr(user, pass, grp);
        if (!isErrorResult(result) && CommandSession::isLoggedIn())
            CommandJournaling::add(CommandSession::getSession().mountId, "mkusr", "/users.txt", user + "," + grp);
        return result;

    } else if (cmd == "rmusr") {
        std::string user = parseParameter(commandLine, "-user");
        std::string result = CommandUsers::rmusr(user);
        if (!isErrorResult(result) && CommandSession::isLoggedIn())
            CommandJournaling::add(CommandSession::getSession().mountId, "rmusr", "/users.txt", user);
        return result;

    } else if (cmd == "chgrp") {
        std::string user = parseParameter(commandLine, "-user");
        std::string grp  = parseParameter(commandLine, "-grp");
        std::string result = CommandUsers::chgrp(user, grp);
        if (!isErrorResult(result) && CommandSession::isLoggedIn())
            CommandJournaling::add(CommandSession::getSession().mountId, "chgrp", "/users.txt", user + "->" + grp);
        return result;

    } else if (cmd == "cat") {
        std::string filePath = parseParameter(commandLine, "-file1");
        if (filePath.empty()) filePath = parseParameter(commandLine, "-file2");
        if (filePath.empty()) filePath = parseParameter(commandLine, "-path");
        std::string id = parseParameter(commandLine, "-id");
        if (filePath.empty()) return "Error: cat requiere -file1=ruta o -path=ruta";
        return CommandUsers::cat(filePath, id);

    } else if (cmd == "mkdir") {
        std::string dirPath = parseParameter(commandLine, "-path");
        bool hasP = (commandLine.find(" -p") != std::string::npos ||
                     commandLine.find("\t-p") != std::string::npos);
        if (dirPath.empty()) return "Error: mkdir requiere -path";
        std::string result = CommandFS::mkdir_cmd(dirPath, hasP);
        if (!isErrorResult(result) && CommandSession::isLoggedIn())
            CommandJournaling::add(CommandSession::getSession().mountId, "mkdir", dirPath, hasP ? "-p" : "-");
        return result;

    } else if (cmd == "mkfile") {
        std::string filePath = parseParameter(commandLine, "-path");
        bool hasR = (commandLine.find(" -r") != std::string::npos ||
                     commandLine.find("\t-r") != std::string::npos);
        std::string sizeStr = parseParameter(commandLine, "-size");
        std::string cont    = parseParameter(commandLine, "-cont");
        int size = 0;
        if (!sizeStr.empty()) {
            try { size = std::stoi(sizeStr); } catch(...) { size = 0; }
        }
        if (filePath.empty()) return "Error: mkfile requiere -path";
        std::string result = CommandFS::mkfile(filePath, hasR, size, cont);
        if (!isErrorResult(result) && CommandSession::isLoggedIn()) {
            std::string detail = !cont.empty() ? cont : ("size=" + std::to_string(size));
            CommandJournaling::add(CommandSession::getSession().mountId, "mkfile", filePath, detail);
        }
        return result;

    } else if (cmd == "journaling") {
        std::string id = parseParameter(commandLine, "-id");
        if (id.empty()) return "Error: journaling requiere -id";
        return CommandJournaling::execute(id);

    } else if (cmd == "exit" || cmd == "quit") {
        return "EXIT";
    } else if (cmd.empty()) {
        return "";
    } else {
        return "Error: Comando no reconocido: " + cmd;
    }
}

std::string trimLine(const std::string& line) {
    size_t commentPos = line.find('#');
    std::string cleaned = (commentPos != std::string::npos) ? line.substr(0, commentPos) : line;
    size_t start = cleaned.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = cleaned.find_last_not_of(" \t\r\n");
    return cleaned.substr(start, end - start + 1);
}

void executeFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo '" << filename << "'\n";
        return;
    }
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        lineNumber++;
        line = trimLine(line);
        if (line.empty()) continue;
        std::cout << "> " << line << "\n";
        std::string result = executeCommand(line);
        if (result == "EXIT") break;
        if (!result.empty()) std::cout << result << "\n\n";
    }
    file.close();
}

void executeMultipleCommands(const std::string& commands) {
    std::istringstream stream(commands);
    std::string line;
    while (std::getline(stream, line)) {
        line = trimLine(line);
        if (line.empty()) continue;
        std::cout << "> " << line << "\n";
        std::string result = executeCommand(line);
        if (result == "EXIT") break;
        if (!result.empty()) std::cout << result << "\n\n";
    }
}

int main(int argc, char* argv[]) {
    srand(time(nullptr));

    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "-f" && argc > 2) {
            executeFromFile(argv[2]);
            return 0;
        } else if (arg1 == "-e" && argc > 2) {
            executeMultipleCommands(argv[2]);
            return 0;
        } else {
            std::cerr << "Uso: " << argv[0] << " [-f archivo] [-e 'comandos']\n";
            return 1;
        }
    }

    // Modo interactivo: lee comandos de stdin (compatible con el frontend)
    std::string commandLine;
    while (true) {
        if (!std::getline(std::cin, commandLine)) break;
        commandLine = trimLine(commandLine);
        if (commandLine.empty()) continue;

        std::string result = executeCommand(commandLine);
        if (result == "EXIT") break;
        if (!result.empty()) std::cout << result << "\n\n";
        std::cout.flush();
    }

    return 0;
}
