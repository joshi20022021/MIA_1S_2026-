#!/bin/bash

echo "    INSTALANDO CROW C++ WEB FRAMEWORK          "
echo ""

# Verificar si crow_all.h ya existe
if [ -f "crow_all.h" ]; then
    echo "crow_all.h ya existe"
    exit 0
fi

echo "Descargando Crow desde GitHub..."
echo ""

# Versión de Crow a descargar
CROW_VERSION="v1.3.1"
CROW_URL="https://github.com/CrowCpp/Crow/releases/download/${CROW_VERSION}/crow_all.h"

# Intentar descargar con wget primero, luego con curl si falla
if command -v wget &> /dev/null; then
    wget -q --show-progress "$CROW_URL" -O crow_all.h
    DOWNLOAD_SUCCESS=$?
elif command -v curl &> /dev/null; then
    echo " wget no encontrado, usando curl..."
    curl -L "$CROW_URL" -o crow_all.h --progress-bar
    DOWNLOAD_SUCCESS=$?
else
    echo "Error: ni wget ni curl están instalados"
    echo "   Instala uno de ellos:"
    echo "   sudo apt-get install wget  # o curl"
    exit 1
fi

if [ $DOWNLOAD_SUCCESS -eq 0 ]; then
    echo ""
    echo "Crow descargado exitosamente!"
    echo ""
    echo "Instalando dependencias del sistema..."
    echo ""
    
    # Instalar dependencias necesarias
    if command -v apt-get &> /dev/null; then
        echo "Detectado: Sistema Debian/Ubuntu"
        echo "   Ejecuta: sudo apt-get install -y libboost-all-dev"
    elif command -v dnf &> /dev/null; then
        echo "Detectado: Sistema Fedora/RHEL"
        echo "   Ejecuta: sudo dnf install -y boost-devel"
    elif command -v pacman &> /dev/null; then
        echo "Detectado: Sistema Arch Linux"
        echo "   Ejecuta: sudo pacman -S boost"
    else
        echo "Sistema no reconocido. Instala Boost manualmente."
    fi
    
    echo ""
    echo "Ya se puede compilar el servidor."
else
    echo ""
    echo "Error al descargar Crow"
    echo "   Intenta descargar manualmente desde:"
    echo "   ${CROW_URL}"
    echo ""
    echo "   O usa este comando:"
    echo "   curl -L ${CROW_URL} -o crow_all.h"
    exit 1
fi
