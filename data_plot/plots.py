import pandas as pd
import matplotlib.pyplot as plt

# Carregar os dados do arquivo CSV
dados = pd.read_csv('sensor_data.csv')  # Altere para o nome do seu arquivo

# Verificar as primeiras linhas para confirmar leitura
print(dados.head())

# Criar figura com subplots
plt.figure(figsize=(15, 10))

# Gráfico de aceleração
plt.subplot(2, 1, 1)
plt.plot(dados['time_s'], dados['accel_x'], label='Acel X')
plt.plot(dados['time_s'], dados['accel_y'], label='Acel Y')
plt.plot(dados['time_s'], dados['accel_z'], label='Acel Z')
plt.title('Acelerômetro')
plt.ylabel('Aceleração (m/s²)')
plt.xlabel('Tempo (s)')
plt.legend()
plt.grid(True)

# Gráfico de giroscópio
plt.subplot(2, 1, 2)
plt.plot(dados['time_s'], dados['giro_x'], label='Giro X')
plt.plot(dados['time_s'], dados['giro_y'], label='Giro Y')
plt.plot(dados['time_s'], dados['giro_z'], label='Giro Z')
plt.title('Giroscópio')
plt.ylabel('Velocidade Angular (º/s)')
plt.xlabel('Tempo (s)')
plt.legend()
plt.grid(True)

# Ajustar layout e mostrar gráficos
plt.tight_layout()
plt.savefig('analise_sensores.png')  # Salva a figura em arquivo
plt.show()
