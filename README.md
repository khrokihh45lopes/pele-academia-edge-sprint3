# Monitoramento Ambiental — Pelé Academia

**Sprint 3 · Edge Computing & Computer Systems**

Um **ESP32** com sensor **DHT22** mede a temperatura e a umidade dos locais de
treino. Ele analisa os dados sozinho, mostra o resultado num **display OLED** e
em **LEDs**, e envia um resumo para o **ThingSpeak**.

## Links

| Item | Link |
|---|---|
| Código no GitHub | <https://github.com/khrokihh45lopes/pele-academia-edge-sprint3> |
| Simulação no Wokwi | <https://wokwi.com/projects/463726731355187201> |
| Canal no ThingSpeak | <https://thingspeak.mathworks.com/channels/3502609> |

## Como funciona

1. A cada **2 s** o ESP32 lê temperatura e umidade.
2. Leituras com erro são descartadas.
3. Ele calcula a **média das últimas 10 leituras** e o **índice de calor**
   (sensação térmica).
4. Compara os valores com os limites abaixo e define o estado do ambiente.
5. Mostra tudo no **OLED** e acende o **LED** da cor do estado.
6. A cada **20 s** envia um resumo ao **ThingSpeak**.

**Por que é Edge Computing:** a decisão *"dá para treinar agora?"* é tomada no
próprio ESP32, sem internet. A nuvem serve só para guardar o histórico e mostrar
gráficos. Se o Wi-Fi cair, o display e os LEDs continuam funcionando.

## Estados do ambiente

| Estado | Quando | Sinal | O que fazer |
|---|---|---|---|
| ADEQUADO | sensação < 27 °C e umidade entre 30 % e 70 % | LED verde | treino normal |
| ATENÇÃO | sensação a partir de 27 °C, ou temperatura a partir de 28 °C, ou umidade fora de 30–70 % | LED amarelo | mais hidratação |
| ALERTA | sensação a partir de 32 °C, ou temperatura a partir de 32 °C, ou umidade ≤ 20 % ou ≥ 80 % | LED vermelho + bipe | reduzir a intensidade |
| CRÍTICO | sensação a partir de 41 °C | LED vermelho piscando + bipe | suspender o treino |

Para evitar alarme falso, o estado só muda depois de **3 leituras seguidas**
confirmando.

## Circuito

| Componente | Pino do ESP32 |
|---|---|
| DHT22 (dado) | GPIO 4 |
| OLED (SDA / SCL) | GPIO 21 / GPIO 22 |
| LED verde / amarelo / vermelho | GPIO 25 / 26 / 27 |
| Buzzer | GPIO 14 |
| Botão (troca de tela) | GPIO 15 |

O display alterna entre 3 telas: **valores e estado**, **médias** e **rede**.
Um toque curto no botão muda a tela. Segurar o botão por mais de 1 s silencia
o buzzer.

## Dados enviados ao ThingSpeak

| Campo | Conteúdo |
|---|---|
| field1 | Temperatura (°C) |
| field2 | Umidade (%) |
| field3 | Temperatura média (°C) |
| field4 | Umidade média (%) |
| field5 | Índice de calor (°C) |
| field6 | Estado (0 = adequado, 3 = crítico) |
| field7 | Leituras acima do limite (de 10) |
| field8 | Sinal do Wi-Fi (dBm) |

## Como testar

1. Abra o [projeto no Wokwi](https://wokwi.com/projects/463726731355187201) e
   clique em **▶ Start**.
2. Clique no **DHT22** e aumente a temperatura e a umidade.
3. Em alguns segundos, o display e os LEDs mudam de estado.
4. Os novos valores aparecem no [canal do ThingSpeak](https://thingspeak.mathworks.com/channels/3502609).

## Aplicação no treino

- **O ambiente está seguro agora?** O OLED responde na hora.
- **Qual o melhor horário para treinar?** O gráfico do índice de calor mostra
  os horários mais frescos.
- **A ventilação é suficiente?** Se a temperatura média sobe durante o treino,
  a ventilação não está dando conta.

## Arquivos

| Arquivo | Conteúdo |
|---|---|
| [sketch.ino](sketch.ino) | código do ESP32 |
| [diagram.json](diagram.json) | circuito do Wokwi |
| [libraries.txt](libraries.txt) | bibliotecas usadas |
| [docs/THINGSPEAK.md](docs/THINGSPEAK.md) | como configurar o canal e os gráficos |
| [docs/ANALISE-AMBIENTES.md](docs/ANALISE-AMBIENTES.md) | análise dos resultados para o treino |
