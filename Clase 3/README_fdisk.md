# Clase 3: Comando FDISK

## Descripción
Implementación del comando **fdisk** para gestión de particiones en discos virtuales.

## Compilación
```bash
g++ -std=c++17 -Wall -I. main.cpp -o disk_manager
```

## Comandos Disponibles

### 1. MKDISK - Crear disco virtual
```bash
./disk_manager mkdisk -size=N -unit=[k|m] -path=ruta
```
**Parámetros:**
- `-size`: Tamaño del disco (requerido)
- `-unit`: k (KB) o m (MB) - Default: m
- `-path`: Ruta del archivo del disco (requerido)

**Ejemplo:**
```bash
./disk_manager mkdisk -size=10 -unit=m -path=~/disks/mydisk.dsk
```

### 2. RMDISK - Eliminar disco virtual
```bash
./disk_manager rmdisk -path=ruta
```
**Parámetros:**
- `-path`: Ruta del archivo del disco (requerido)

**Ejemplo:**
```bash
./disk_manager rmdisk -path=~/disks/mydisk.dsk
```

### 3. FDISK - Gestión de particiones

#### Crear partición
```bash
./disk_manager fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre
```

**Parámetros:**
- `-size`: Tamaño de la partición en KB o MB (requerido)
- `-unit`: k (KB) o m (MB) - Default: k
- `-path`: Ruta del disco (requerido)
- `-type`: Tipo de partición (opcional, default: P)
  - `P`: Partición Primaria
  - `E`: Partición Extendida
  - `L`: Partición Lógica (debe existir una extendida)
- `-fit`: Algoritmo de ajuste (opcional, default: WF)
  - `BF`: Best Fit (mejor ajuste)
  - `FF`: First Fit (primer ajuste)
  - `WF`: Worst Fit (peor ajuste)
- `-name`: Nombre de la partición (requerido, máx. 16 caracteres)

**Ejemplos:**
```bash
# Crear partición primaria de 3MB
./disk_manager fdisk -size=3000 -unit=k -path=~/disks/test.dsk -type=P -name=part1

# Crear partición extendida de 5MB con First Fit
./disk_manager fdisk -size=5000 -unit=k -path=~/disks/test.dsk -type=E -fit=FF -name=extended1

# Crear partición lógica de 1MB (requiere extendida)
./disk_manager fdisk -size=1000 -unit=k -path=~/disks/test.dsk -type=L -name=logical1
```

#### Eliminar partición
```bash
./disk_manager fdisk -delete=nombre -path=ruta
```

**Parámetros:**
- `-delete`: Nombre de la partición a eliminar (requerido)
- `-path`: Ruta del disco (requerido)

**Ejemplo:**
```bash
./disk_manager fdisk -delete=part1 -path=~/disks/test.dsk
```

### 4. INFO - Ver información del disco
```bash
./disk_manager info -path=ruta
```
**Parámetros:**
- `-path`: Ruta del disco (requerido)

**Ejemplo:**
```bash
./disk_manager info -path=~/disks/test.dsk
```

## Características Implementadas

### Particiones
- ✅ Máximo 4 particiones primarias/extendidas
- ✅ Solo 1 partición extendida por disco
- ✅ Particiones lógicas ilimitadas dentro de la extendida
- ✅ Validación de espacios disponibles
- ✅ Validación de nombres únicos
- ✅ Algoritmos de ajuste: Best Fit, First Fit, Worst Fit

### Estructuras de Datos
- **MBR (Master Boot Record)**: Almacena información del disco y 4 particiones
- **Partition**: Representa una partición primaria o extendida
- **EBR (Extended Boot Record)**: Lista enlazada de particiones lógicas

### Validaciones
- ✅ No solapamiento de particiones
- ✅ Espacio suficiente en disco
- ✅ Nombres únicos de particiones
- ✅ Solo una partición extendida por disco
- ✅ Particiones lógicas solo dentro de extendida

## Ejemplos de Uso Completos

### Script de prueba automatizado
```bash
# Ejecutar el script de ejemplos
./ejemplos_fdisk.sh
```

### Uso interactivo
```bash
# 1. Crear disco
./disk_manager mkdisk -size=20 -unit=m -path=~/disks/test.dsk

# 2. Crear partición primaria
./disk_manager fdisk -size=5000 -unit=k -path=~/disks/test.dsk -name=primary1 -type=P

# 3. Crear partición extendida
./disk_manager fdisk -size=10000 -unit=k -path=~/disks/test.dsk -name=extended -type=E

# 4. Crear particiones lógicas
./disk_manager fdisk -size=2000 -unit=k -path=~/disks/test.dsk -name=logical1 -type=L
./disk_manager fdisk -size=3000 -unit=k -path=~/disks/test.dsk -name=logical2 -type=L

# 5. Ver información
./disk_manager info -path=~/disks/test.dsk

# 6. Eliminar partición
./disk_manager fdisk -delete=logical1 -path=~/disks/test.dsk

# 7. Eliminar disco
./disk_manager rmdisk -path=~/disks/test.dsk
```

## Notas Importantes

1. **Orden de parámetros**: Los parámetros pueden especificarse en cualquier orden
2. **Expandir rutas**: Soporta `~/` para el directorio home
3. **Creación automática de carpetas**: Si no existen las carpetas en el path, se crean automáticamente
4. **Case-insensitive**: Los comandos y parámetros no distinguen mayúsculas/minúsculas
5. **Comillas opcionales**: Los valores pueden llevar comillas o no: `-path="~/disk.dsk"` o `-path=~/disk.dsk`

## Estructura de Archivos

```
Clase 3/
├── disk_manager.h          # Implementación de mkdisk, rmdisk, fdisk
├── structures.h            # Definición de MBR, Partition, EBR
├── main.cpp               # Parser de comandos y entrada/salida
├── ejemplos_fdisk.sh      # Script de pruebas automatizado
├── README_fdisk.md        # Esta documentación
└── disk_manager           # Ejecutable compilado
```

## Diferencias con Clase 2

- ✅ **Nuevo**: Comando `fdisk` para gestión de particiones
- ✅ **Nuevo**: Soporte para particiones primarias, extendidas y lógicas
- ✅ **Nuevo**: Algoritmos de ajuste (BF, FF, WF)
- ✅ **Nuevo**: Estructura EBR para particiones lógicas
- ✅ **Mantiene**: Comandos mkdisk, rmdisk, info
- ✅ **Mantiene**: Soporte para ~ y creación automática de carpetas

## Próximos Comandos (Proyecto Completo)

- `mount`: Montar particiones
- `mkfs`: Crear sistema de archivos
- `login`: Iniciar sesión
- `mkdir/mkfile`: Crear carpetas y archivos
- `cat/edit`: Ver y editar archivos
- `rep`: Generar reportes
