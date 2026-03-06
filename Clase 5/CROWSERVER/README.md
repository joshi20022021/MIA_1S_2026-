# SERVIDOR CROW

---

## Instalación

### 1. Instalar Crow Framework

```bash
./install_crow.sh
```

### 2. Instalar Boost (dependencia requerida)

**Ubuntu/Debian:**
```bash
sudo apt-get install -y libboost-all-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install -y boost-devel
```

**Arch Linux:**
```bash
sudo pacman -S boost
```

---

## Ejecutar Servidor

### Compilar y ejecutar:

```bash
g++ -std=c++17 -pthread main.cpp -o server -lboost_system
./server
```

El servidor se iniciará en `http://localhost:8081`

---

## Uso de la API

### Endpoint: POST /mkdisk

Crea un disco virtual.

**Ejemplo con curl:**
```bash
curl -X POST http://localhost:8081/mkdisk \
  -H "Content-Type: application/json" \
  -d '{
    "size": 10,
    "unit": "m",
    "path": "/home/usuario/disco.mia"
  }'
```

**Parámetros:**
- `size` (int, requerido): Tamaño del disco
- `unit` (string, opcional): "k" para KB, "m" para MB (default: "m")
- `path` (string, requerido): Ruta donde crear el disco

**Respuesta exitosa:**
```json
{
  "success": true,
  "message": "Disco creado exitosamente",
  "details": "Disco creado exitosamente\n  Ruta: /home/usuario/disco.mia\n  Tamaño: 10 m (10485760 bytes)\n  Fecha: 2026-03-05 23:45:00\n  Firma: 123456789",
  "parameters": {
    "size": 10,
    "unit": "m",
    "path": "/home/usuario/disco.mia"
  }
}
```

**Respuesta con error:**
```json
{
  "success": false,
  "error": "Error: El disco ya existe en la ruta especificada"
}
```

---

## Probar el Servidor

