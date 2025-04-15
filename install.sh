#!/bin/bash

# Função para instalar Node.js e PM2 no Ubuntu
install_on_ubuntu() {
    echo "Atualizando pacotes no Ubuntu..."
    sudo apt-get update -y || { echo "Erro ao atualizar pacotes."; exit 1; }

    echo "Instalando Node.js..."
    sudo apt-get install -y nodejs npm || { echo "Erro ao instalar Node.js."; exit 1; }

    echo "Instalando PM2 globalmente..."
    sudo npm install -g pm2 || { echo "Erro ao instalar PM2."; exit 1; }

    echo "Liberando portas 9001 e 8050 no firewall..."
    sudo ufw allow 9001 || { echo "Erro ao liberar porta 9001."; exit 1; }
    sudo ufw allow 8050 || { echo "Erro ao liberar porta 8050."; exit 1; }

    echo "Configuração no Ubuntu concluída!"
}

# Função para instalar Node.js e PM2 no CentOS
install_on_centos() {
    echo "Atualizando pacotes no CentOS..."
    sudo yum update -y || { echo "Erro ao atualizar pacotes."; exit 1; }

    echo "Instalando Node.js..."
    sudo yum install -y epel-release || { echo "Erro ao instalar epel-release."; exit 1; }
    sudo yum install -y nodejs npm || { echo "Erro ao instalar Node.js."; exit 1; }

    echo "Instalando PM2 globalmente..."
    sudo npm install -g pm2 || { echo "Erro ao instalar PM2."; exit 1; }

    echo "Liberando portas 9001 e 8050 no firewall..."
    sudo firewall-cmd --permanent --add-port=9001/tcp || { echo "Erro ao liberar porta 9001."; exit 1; }
    sudo firewall-cmd --permanent --add-port=8050/tcp || { echo "Erro ao liberar porta 8050."; exit 1; }
    sudo firewall-cmd --reload || { echo "Erro ao recarregar firewall."; exit 1; }

    echo "Configuração no CentOS concluída!"
}

# Função para iniciar o app apiodin usando PM2
start_apiodin() {
    echo "Iniciando aplicação apiodin com PM2..."
    chmod +x apiodin || { echo "Erro ao configurar permissões para apiodin."; exit 1; }
    pm2 start apiodin || { echo "Erro ao iniciar apiodin."; exit 1; }

    echo "Definindo PM2 para reiniciar automaticamente após reboot..."
    pm2 startup || { echo "Erro ao configurar PM2 startup."; exit 1; }
    pm2 save || { echo "Erro ao salvar configuração do PM2."; exit 1; }
}

# Detectar o sistema operacional
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    echo "Sistema operacional não suportado."
    exit 1
fi

# Instalar pacotes dependendo do sistema operacional
case "$OS" in
    ubuntu)
        install_on_ubuntu
        ;;
    centos)
        install_on_centos
        ;;
    debian)
        install_on_ubuntu # Debian usa apt-get, então a instalação é similar ao Ubuntu
        ;;
    rhel)
        install_on_centos # RHEL usa yum, então a instalação é similar ao CentOS
        ;;
    *)
        echo "Sistema operacional não suportado: $OS"
        exit 1
        ;;
esac

# Rodar o app apiodin
start_apiodin

echo "Instalação e configuração concluídas!"

# Verifica se o Node.js está instalado
if (-Not (Get-Command node -ErrorAction SilentlyContinue)) {
    Write-Host "Node.js não encontrado. Baixe e instale do site oficial: https://nodejs.org/"
    Exit
}

# Instala PM2 globalmente
Write-Host "Instalando PM2..."
npm install -g pm2

# Verifica se a instalação foi bem-sucedida
if (-Not (Get-Command pm2 -ErrorAction SilentlyContinue)) {
    Write-Host "Erro ao instalar PM2."
    Exit
}

# Inicia o app
Write-Host "Iniciando a aplicação apiodin com PM2..."
cd "C:\caminho\do\apiodin"
pm2 start apiodin

# Configura PM2 para reiniciar após reboot
pm2 startup
pm2 save

Write-Host "Instalação e configuração concluídas!"