# Monitoramento Ambiental dos Locais de Treinamento — Pelé Academia

**Sprint 3 · Edge Computing & Computer Systems**

Protótipo baseado em **ESP32** que monitora temperatura e umidade dos ambientes de
treinamento da Pelé Academia, **processa os dados na própria borda**, apresenta o
resultado em uma **IHM local (display OLED + LEDs + buzzer)** e publica as
informações consolidadas no **ThingSpeak** para histórico e acompanhamento remoto.

---

## 1. Links da entrega

| Item | Link |
|---|---|
| Repositório GitHub (código-fonte do ESP32) | `https://github.com/khrokihh45lopes/pele-academia-edge-sprint3` |
| Projeto no Wokwi (simulação) | `https://wokwi.com/projects/463726731355187201` |
| Canal público no ThingSpeak | `https://thingspeak.mathworks.com/channels/3502609` |

> ⚠️ **Preencher antes de entregar.** Os três links acima são obrigatórios.
> O passo a passo para gerar o canal ThingSpeak e o projeto Wokwi está nas
> seções 8 e 9 e em [docs/THINGSPEAK.md](docs/THINGSPEAK.md).

---

## 2. Descrição do funcionamento

O ESP32 executa sete tarefas cooperativas, todas **não bloqueantes** (baseadas em
`millis()`, sem `delay()` no laço principal):

1. **Aquisição** — lê temperatura e umidade do DHT22 a cada **2 segundos**.
2. **Validação** — descarta leituras inválidas (`NaN`) ou fora da faixa física
   de medição do DHT22 (−40 °C a 80 °C / 0 % a 100 %), contabilizando as falhas.
3. **Processamento local** — mantém uma **janela móvel das 10 últimas medições**
   (20 s) e calcula média, mínimo, máximo, tendência e o **índice de calor**
   (sensação térmica) a partir de temperatura e umidade.
4. **Classificação** — compara as médias com **limites previamente estabelecidos**
   e classifica o ambiente em quatro níveis; a mudança de nível só é confirmada
   após **3 amostras consecutivas**, o que evita alarme falso por pico isolado.
5. **IHM local** — mostra tudo em três telas rotativas no OLED, sinaliza o estado
   nos LEDs (verde/amarelo/vermelho) e emite aviso sonoro em alerta e em condição
   crítica.
6. **Conectividade** — conecta ao Wi-Fi e se reconecta sozinho a cada 15 s em caso
   de queda, **sem interromper o monitoramento local**.
7. **Publicação na nuvem** — a cada **20 segundos** envia ao ThingSpeak um pacote
   consolidado (valores instantâneos + médias + índice de calor + nível de risco),
   e não as 10 leituras brutas do período.

### Fluxo dos dados

```
   ┌──────────┐   2 s    ┌──────────────────────────────────────────┐
   │  DHT22   ├─────────►│                 ESP32                    │
   │  T / UR  │          │  ┌────────────────────────────────────┐  │
   └──────────┘          │  │ BORDA (Edge)                       │  │
                         │  │ • valida a leitura                 │  │
                         │  │ • janela móvel de 10 amostras      │  │
                         │  │ • média / mín / máx / tendência    │  │
                         │  │ • índice de calor (T + UR)         │  │
                         │  │ • compara com limites definidos    │  │
                         │  │ • classifica em 4 níveis (3x conf.)│  │
                         │  └───────────┬──────────────┬─────────┘  │
                         │              │              │            │
                         └──────────────┼──────────────┼────────────┘
                        imediato        │              │  a cada 20 s
                    ┌───────────────────▼──┐        ┌──▼─────────────────┐
                    │ IHM LOCAL            │        │ NUVEM              │
                    │ OLED 128x64          │        │ ThingSpeak         │
                    │ LED verde/amar./verm.│        │ • armazenamento    │
                    │ buzzer + botão       │        │ • gráficos         │
                    └──────────────────────┘        │ • acesso público   │
                                                    └────────────────────┘
```

---

## 3. Operações na borda × operações na nuvem

| Onde | Operação | Detalhe |
|---|---|---|
| 🟢 **ESP32 (borda)** | Aquisição periódica | DHT22 a cada 2 s |
| 🟢 **ESP32 (borda)** | Validação e descarte de leituras | `NaN` e valores fora da faixa física |
| 🟢 **ESP32 (borda)** | Média móvel das 10 últimas medições | `JanelaMedicoes::media()` |
| 🟢 **ESP32 (borda)** | Mínimo, máximo e tendência da janela | `minimo()`, `maximo()`, `variacao()` |
| 🟢 **ESP32 (borda)** | Cálculo do índice de calor (sensação térmica) | `dht.computeHeatIndex(T, UR)` |
| 🟢 **ESP32 (borda)** | Contagem de amostras acima do limite | `acimaDe(LIM_TEMP_ATENCAO)` |
| 🟢 **ESP32 (borda)** | Classificação do ambiente em 4 níveis | `avaliarAmbiente()` |
| 🟢 **ESP32 (borda)** | Filtro anti-falso-alarme (3 amostras) | `aplicarConfirmacao()` |
| 🟢 **ESP32 (borda)** | Decisão de acionar LEDs e buzzer | resposta em < 1 s, sem rede |
| 🟢 **ESP32 (borda)** | Apresentação na IHM (3 telas) | OLED SSD1306 |
| 🟢 **ESP32 (borda)** | Redução de tráfego (10 leituras → 1 envio) | 1 requisição a cada 20 s |
| 🟢 **ESP32 (borda)** | Gestão da conexão e reconexão Wi-Fi | opera offline sem perder função |
| ☁️ **ThingSpeak (nuvem)** | Armazenamento do histórico | 8 campos por registro |
| ☁️ **ThingSpeak (nuvem)** | Gráficos de acompanhamento | *field charts* + widgets |
| ☁️ **ThingSpeak (nuvem)** | Visualização pública e remota | canal público |
| ☁️ **ThingSpeak (nuvem)** | Análise de série histórica (dias/semanas) | comparação entre turnos |

**Por que é Edge Computing:** a decisão que importa para o treino — *“o ambiente
está adequado agora?”* — é tomada no próprio ESP32, em menos de um segundo e sem
depender da internet. A nuvem entra depois, apenas para guardar e visualizar o
histórico. Se o Wi-Fi cair, o OLED, os LEDs e o buzzer continuam funcionando
normalmente; só o envio é adiado.

---

## 4. Hardware e pinagem

| Componente | Pino do ESP32 | Observação |
|---|---|---|
| DHT22 — dado | `GPIO 4` | alimentação em 3V3 |
| OLED SSD1306 — SDA | `GPIO 21` | I²C, endereço `0x3C` |
| OLED SSD1306 — SCL | `GPIO 22` | I²C |
| LED verde (adequado) | `GPIO 25` | resistor de 220 Ω |
| LED amarelo (atenção) | `GPIO 26` | resistor de 220 Ω |
| LED vermelho (alerta/crítico) | `GPIO 27` | resistor de 220 Ω |
| Buzzer | `GPIO 14` | aviso sonoro |
| Botão | `GPIO 15` | `INPUT_PULLUP`, ligado ao GND |

O circuito completo está em [diagram.json](diagram.json), pronto para o Wokwi.

---

## 5. Processamento local em detalhe

### 5.1 Janela móvel

Estrutura circular de 10 posições (`JanelaMedicoes`) que guarda as últimas
10 medições válidas de temperatura, umidade e índice de calor — 20 segundos de
histórico dentro do próprio microcontrolador. A partir dela o ESP32 calcula:

- **média** — suaviza o ruído do sensor e evita reagir a um valor isolado;
- **mínimo e máximo** — mostra a oscilação real do ambiente no período;
- **tendência** — compara a média da metade mais recente com a da metade mais
  antiga: `SUBINDO`, `CAINDO` ou `ESTÁVEL`;
- **contagem acima do limite** — quantas das 10 amostras ultrapassaram o limite
  de atenção de temperatura.

### 5.2 Índice de calor

A temperatura sozinha não descreve o esforço imposto ao atleta: com umidade alta
o suor não evapora e o corpo perde a capacidade de se resfriar. Por isso o ESP32
calcula localmente o **índice de calor** combinando temperatura e umidade, e é
esse valor que comanda a classificação principal.

### 5.3 Confirmação por amostras consecutivas

Uma mudança de nível só é aceita depois de **3 amostras consecutivas** apontando
para o novo estado (6 segundos). Isso impede que a abertura de uma porta ou uma
pessoa passando na frente do sensor dispare um alarme indevido.

---

## 6. Limites e condição de atenção

| Variável | Adequado | Atenção | Alerta | Crítico |
|---|---|---|---|---|
| Índice de calor | < 27 °C | 27 – 32 °C | 32 – 41 °C | ≥ 41 °C |
| Temperatura | < 28 °C | 28 – 32 °C | ≥ 32 °C | — |
| Umidade relativa | 30 – 70 % | ≤ 30 % ou ≥ 70 % | ≤ 20 % ou ≥ 80 % | — |

Vale sempre o **fator mais restritivo**: basta uma das variáveis atingir a faixa
para o ambiente inteiro ser classificado naquele nível.

| Nível | Código | Sinalização | Recomendação operacional |
|---|---|---|---|
| `ADEQUADO` | 0 | LED verde | treino normal |
| `ATENCAO` | 1 | LED amarelo | reforçar hidratação, observar o grupo |
| `ALERTA` | 2 | LED vermelho + bipe a cada 6 s | reduzir intensidade, encurtar blocos, ampliar pausas |
| `CRITICO` | 3 | LED vermelho piscando + bipe a cada 2 s | suspender ou transferir a atividade |

Os valores ficam todos agrupados no início de [sketch.ino](sketch.ino) e podem ser
ajustados por ambiente (quadra coberta, sala de musculação, campo aberto).

---

## 7. IHM local

**Três telas** alternam automaticamente a cada 4 segundos; o **toque curto no
botão** avança manualmente e o **toque longo (> 1 s)** silencia o buzzer.

```
  TELA 1 — VALORES + ESTADO        TELA 2 — PROCESSAMENTO          TELA 3 — REDE / NUVEM
 ┌────────────────────────┐      ┌────────────────────────┐      ┌────────────────────────┐
 │ PELE ACADEMIA      NET │      │ PROCESSAMENTO      NET │      │ REDE / NUVEM       NET │
 │                        │      │ Amostras: 10/10        │      │ WiFi: -62 dBm          │
 │  28.4 C      61 %      │      │ Media T:28.1C UR:60%   │      │ 192.168.0.42           │
 │                        │      │ T min 27.6 max 28.9    │      │ Envios OK:14 ERR:0     │
 │ Sensacao: 31.2 C       │      │ Tendencia: SUBINDO     │      │ Ultimo envio: 8s       │
 │ ATENCAO                │      │ Acima lim: 7/10        │      │ Alertas: 2             │
 │ Sensacao term. sobe    │      │                        │      │                        │
 └────────────────────────┘      └────────────────────────┘      └────────────────────────┘
```

A tela 1 atende ao requisito mínimo: **valores das variáveis monitoradas** e
**indicação do estado das condições ambientais**, com o motivo da classificação.

---

## 8. ThingSpeak

### Campos do canal

| Campo | Conteúdo | Unidade |
|---|---|---|
| `field1` | Temperatura instantânea | °C |
| `field2` | Umidade instantânea | % |
| `field3` | **Temperatura média** (janela de 10) | °C |
| `field4` | **Umidade média** (janela de 10) | % |
| `field5` | **Índice de calor** (calculado na borda) | °C |
| `field6` | **Nível da condição** (0 a 3) | — |
| `field7` | Amostras acima do limite (0 a 10) | — |
| `field8` | Qualidade do sinal Wi-Fi (RSSI) | dBm |

### Configuração rápida

1. Crie a conta em <https://thingspeak.com> e clique em **Channels → New Channel**.
2. Nomeie o canal (ex.: *Pelé Academia — Quadra Coberta*) e **habilite os 8 campos**
   com os nomes da tabela acima.
3. Em **API Keys**, copie a **Write API Key** e cole em `TS_WRITE_API_KEY`
   no arquivo [sketch.ino](sketch.ino).
4. Em **Sharing**, marque **“Make public”** para liberar a visualização pública.
5. Em **Public View**, adicione os gráficos e widgets do canal.

O passo a passo detalhado, com os gráficos sugeridos e as configurações de cada
um, está em **[docs/THINGSPEAK.md](docs/THINGSPEAK.md)**.

---

## 9. Como executar no Wokwi

**Opção A — pelo site (recomendada):**

1. Acesse <https://wokwi.com/projects/new/esp32>.
2. Substitua o conteúdo de `sketch.ino` pelo deste repositório.
3. Abra a aba `diagram.json` e cole o conteúdo do arquivo deste repositório.
4. Crie a aba `libraries.txt` (botão **+** → *Library Manager* ou nova aba) com as
   quatro bibliotecas listadas em [libraries.txt](libraries.txt).
5. Cole sua **Write API Key** do ThingSpeak em `TS_WRITE_API_KEY`.
6. Clique em **▶ Start** e depois em **Save** para gerar o link público do projeto.

> No Wokwi a rede é sempre `Wokwi-GUEST`, sem senha, canal 6 — já configurado no
> código. A simulação tem acesso real à internet, então os dados chegam mesmo ao
> ThingSpeak.

**Como testar os alertas na simulação:** clique no **DHT22** dentro do simulador e
arraste os controles de temperatura e umidade. Suba a temperatura para 30 °C com
80 % de umidade e observe, em sequência: o índice de calor subindo na tela 1, o
contador `Acima lim` crescendo na tela 2, o LED amarelo e depois o vermelho, o
buzzer e, por fim, o `field6` mudando de valor no gráfico do ThingSpeak.

**Opção B — VS Code:** instale a extensão *Wokwi for VS Code*, compile com
`arduino-cli` e use o [wokwi.toml](wokwi.toml) já incluído.

---

## 10. Relação com os ambientes de treinamento esportivo

O monitoramento existe para responder a perguntas concretas da rotina da academia:

- **A quadra está segura para treino de alta intensidade agora?** O nível exibido
  no OLED responde imediatamente, sem o técnico precisar consultar o celular.
- **Qual o melhor horário para os treinos?** O gráfico do `field5` (índice de
  calor) ao longo do dia mostra a janela em que o ambiente sai da faixa de
  atenção — tipicamente o começo da manhã e o fim da tarde.
- **A ventilação/climatização do ambiente é suficiente?** Se o `field3`
  (temperatura média) sobe continuamente durante o treino e só cai depois que a
  turma sai, a renovação de ar não está acompanhando a ocupação.
- **O ambiente é abafado ou seco demais?** Umidade acima de 70 % indica ambiente
  abafado, em que o suor não evapora e a fadiga chega antes; abaixo de 30 % o ar
  seco irrita as vias respiratórias, o que pesa em modalidades de longa duração.
- **Quantas vezes por semana o limite foi ultrapassado?** O `field6` e o `field7`
  transformam o acompanhamento em evidência para justificar investimento em
  ventilação, sombreamento ou mudança de horário das turmas.

A análise completa, com a leitura recomendada de cada gráfico e as ações
sugeridas para cada faixa, está em
**[docs/ANALISE-AMBIENTES.md](docs/ANALISE-AMBIENTES.md)**.

---

## 11. Estrutura do repositório

```
.
├── sketch.ino                    # Firmware do ESP32 (código-fonte principal)
├── diagram.json                  # Circuito do Wokwi
├── libraries.txt                 # Bibliotecas usadas
├── wokwi.toml                    # Config. da simulação no VS Code
├── README.md                     # Este arquivo
└── docs/
    ├── THINGSPEAK.md             # Criação do canal, gráficos e visualização pública
    └── ANALISE-AMBIENTES.md      # Leitura dos resultados x condições de treino
```

### Bibliotecas

| Biblioteca | Uso |
|---|---|
| `DHT sensor library` (Adafruit) | leitura do DHT22 e índice de calor |
| `Adafruit Unified Sensor` | dependência da anterior |
| `Adafruit GFX Library` | primitivas gráficas do display |
| `Adafruit SSD1306` | driver do OLED 128x64 |

---

## 12. Checklist dos critérios de avaliação

| # | Critério | Onde está atendido |
|---|---|---|
| 1 | Sensor de temperatura e umidade conectado ao ESP32 | DHT22 no `GPIO 4` — [diagram.json](diagram.json) |
| 2 | Aquisição periódica das medições | `INTERVALO_LEITURA_MS = 2000` — [sketch.ino](sketch.ino) |
| 3 | Processamento local no ESP32 | `JanelaMedicoes`, média/mín/máx/tendência, índice de calor |
| 4 | Condição de atenção baseada nos valores | `avaliarAmbiente()` + tabela de limites (seção 6) |
| 5 | Apresentação local por IHM | OLED SSD1306 com 3 telas + LEDs + buzzer (seção 7) |
| 6 | Conexão Wi-Fi | `conectarWifi()` e `cuidarDaConexao()` |
| 7 | Envio para canal do ThingSpeak | `enviarParaThingSpeak()` — 8 campos |
| 8 | Gráficos de acompanhamento | [docs/THINGSPEAK.md](docs/THINGSPEAK.md) |
| 9 | Canal público | **Sharing → Make public** (seção 8) |
| 10 | Desenvolvido e testado no Wokwi | [diagram.json](diagram.json) + seção 9 |
| 11 | Código-fonte no GitHub | este repositório |
| 12 | Resultados relacionados ao treinamento esportivo | seção 10 + [docs/ANALISE-AMBIENTES.md](docs/ANALISE-AMBIENTES.md) |
