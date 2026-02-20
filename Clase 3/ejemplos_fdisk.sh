#!/bin/bash

# Ejemplos de uso del comando fdisk
# Compilar primero: g++ -std=c++17 -Wall -I. main.cpp -o disk_manager

echo "=== EJEMPLOS DE USO DEL COMANDO FDISK ==="
echo ""

# 1. Crear un disco de prueba
echo "1. Crear disco de 10MB"
echo "./disk_manager mkdisk -size=10 -unit=m -path=~/disks/test.dsk"
./disk_manager mkdisk -size=10 -unit=m -path=~/disks/test.dsk
echo ""

# 2. Crear partición primaria con valores por defecto
echo "2. Crear partición primaria (por defecto)"
echo "./disk_manager fdisk -size=3000 -path=~/disks/test.dsk -name=part1"
./disk_manager fdisk -size=3000 -path=~/disks/test.dsk -name=part1
echo ""

# 3. Crear partición extendida
echo "3. Crear partición extendida"
echo "./disk_manager fdisk -size=5000 -unit=k -path=~/disks/test.dsk -type=E -name=extended1"
./disk_manager fdisk -size=5000 -unit=k -path=~/disks/test.dsk -type=E -name=extended1
echo ""

# 4. Crear particiones lógicas dentro de la extendida
echo "4. Crear partición lógica 1"
echo "./disk_manager fdisk -size=1000 -unit=k -path=~/disks/test.dsk -type=L -name=logical1"
./disk_manager fdisk -size=1000 -unit=k -path=~/disks/test.dsk -type=L -name=logical1
echo ""

echo "5. Crear partición lógica 2"
echo "./disk_manager fdisk -size=1500 -unit=k -path=~/disks/test.dsk -type=L -name=logical2"
./disk_manager fdisk -size=1500 -unit=k -path=~/disks/test.dsk -type=L -name=logical2
echo ""

# 6. Crear otra partición primaria con Best Fit
echo "6. Crear partición primaria con Best Fit"
echo "./disk_manager fdisk -size=500 -unit=k -path=~/disks/test.dsk -type=P -fit=BF -name=part2"
./disk_manager fdisk -size=500 -unit=k -path=~/disks/test.dsk -type=P -fit=BF -name=part2
echo ""

# 7. Ver información del disco
echo "7. Ver información del disco"
echo "./disk_manager info -path=~/disks/test.dsk"
./disk_manager info -path=~/disks/test.dsk
echo ""

# 8. Eliminar una partición lógica
echo "8. Eliminar partición lógica"
echo "./disk_manager fdisk -delete=logical1 -path=~/disks/test.dsk"
./disk_manager fdisk -delete=logical1 -path=~/disks/test.dsk
echo ""

# 9. Ver información después de eliminar
echo "9. Ver información después de eliminar"
echo "./disk_manager info -path=~/disks/test.dsk"
./disk_manager info -path=~/disks/test.dsk
echo ""

# 10. Eliminar una partición primaria
echo "10. Eliminar partición primaria"
echo "./disk_manager fdisk -delete=part1 -path=~/disks/test.dsk"
./disk_manager fdisk -delete=part1 -path=~/disks/test.dsk
echo ""

# 11. Ver información final
echo "11. Ver información final"
echo "./disk_manager info -path=~/disks/test.dsk"
./disk_manager info -path=~/disks/test.dsk
echo ""

echo "=== FIN DE LOS EJEMPLOS ==="
echo ""
echo "Otros comandos disponibles:"
echo "  - fdisk con First Fit: -fit=FF"
echo "  - fdisk con Worst Fit: -fit=WF (por defecto)"
echo "  - fdisk con Best Fit: -fit=BF"
echo "  - Tipos: P=Primaria, E=Extendida, L=Lógica"
echo "  - Unidades: k=Kilobytes, m=Megabytes"
echo ""
echo "Nota: Los parámetros pueden estar en cualquier orden"
echo "Ejemplo: fdisk -name=test -path=~/disk.dsk -size=1000 -type=P"
