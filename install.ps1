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
Set-Location "C:/Users/Administrator/Desktop/odin/apiodin"
pm2 start apiodin

# Configura PM2 para reiniciar após reboot
pm2 startup
pm2 save

Write-Host "Instalação e configuração concluídas!"