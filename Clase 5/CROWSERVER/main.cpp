#include "crow_all.h"
#include "mkdisk.h"
#include <iostream>
#include <string>
#include <cstdlib>

int main() {
    // Inicializar generador de números aleatorios
    srand(time(nullptr));
    
    // Crear aplicación Crow
    crow::SimpleApp app;
    
    // ========== ENDPOINT: GET ==========
    CROW_ROUTE(app, "/")
    ([]() {
        crow::json::wvalue response;
        response["message"] = " Servidor MIA - Disk Manager API";
        response["version"] = "1.0.0";
        response["endpoints"] = crow::json::wvalue::list({
            "/mkdisk (POST) - Crear un disco virtual"
        });
        response["status"] = "running";
        return crow::response(200, response);
    });
    
    // ========== ENDPOINT: POST /mkdisk (Crear disco) ==========
    CROW_ROUTE(app, "/mkdisk")
    .methods("POST"_method)
    ([](const crow::request& req) {
        crow::json::wvalue response;
        
        try {
            // Parsear JSON del body
            auto body = crow::json::load(req.body);
            
            if (!body) {
                response["success"] = false;
                response["error"] = "JSON inválido en el body";
                return crow::response(400, response);
            }
            
            // Extraer parámetros (con valores por defecto)
            int size = body.has("size") ? body["size"].i() : 10;
            std::string unit = body.has("unit") ? std::string(body["unit"].s()) : std::string("m");
            std::string path = body.has("path") ? std::string(body["path"].s()) : std::string("");
            
            // Validar path (obligatorio)
            if (path.empty()) {
                response["success"] = false;
                response["error"] = "El parámetro 'path' es obligatorio";
                return crow::response(400, response);
            }
            
            // Ejecutar comando mkdisk
            std::string result = CommandMkdisk::execute(size, unit, path);
            
            // Verificar si hubo error
            if (result.find("Error:") != std::string::npos || 
                result.find("error:") != std::string::npos) {
                response["success"] = false;
                response["error"] = result;
                return crow::response(400, response);
            }
            
            // Éxito
            response["success"] = true;
            response["message"] = "Disco creado exitosamente";
            response["details"] = result;
            response["parameters"] = crow::json::wvalue{
                {"size", size},
                {"unit", unit},
                {"path", path}
            };
            
            return crow::response(200, response);
            
        } catch (const std::exception& e) {
            response["success"] = false;
            response["error"] = std::string("Excepción: ") + e.what();
            return crow::response(500, response);
        }
    });
    
    // ========== ENDPOINT: GET /health ==========
    CROW_ROUTE(app, "/health")
    ([]() {
        crow::json::wvalue response;
        response["status"] = "healthy";
        response["uptime"] = "N/A";
        return crow::response(200, response);
    });
    
    // Configurar puerto y arrancar servido
    std::cout << "C++ DISK\n";
    std::cout << "MIA Proyecto 1 - 2026\n";
    std::cout << "\n🔥 Servidor corriendo... Presiona Ctrl+C para detener\n\n";
    
    app.port(8081)
       .multithreaded()
       .run();
    
    return 0;
}
