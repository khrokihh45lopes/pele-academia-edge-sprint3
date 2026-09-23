/* ============================================================================
 *  PELE ACADEMIA - MONITORAMENTO AMBIENTAL DOS LOCAIS DE TREINAMENTO
 *  Sprint 3 - Edge Computing & Computer Systems
 * ----------------------------------------------------------------------------
 *  Plataforma : ESP32 DevKit V1 (framework Arduino)
 *  Sensor     : DHT22 (temperatura + umidade relativa)
 *  IHM local  : Display OLED SSD1306 128x64 (I2C) + 3 LEDs + buzzer + botao
 *  Nuvem      : ThingSpeak (armazenamento, historico e graficos)
 *
 *  ARQUITETURA DE EDGE COMPUTING
 *  -----------------------------
 *  NA BORDA (dentro deste ESP32):
 *    1. Aquisicao periodica das medicoes (a cada 2 s);
 *    2. Validacao/descarte de leituras invalidas (NaN ou fora da faixa fisica);
 *    3. Janela movel com as ultimas 10 medicoes (media, minimo, maximo);
 *    4. Calculo do indice de calor (sensacao termica) a partir de T e UR;
 *    5. Comparacao com limites previamente estabelecidos para pratica esportiva;
 *    6. Classificacao do ambiente em 4 niveis, com confirmacao por 3 amostras
 *       consecutivas (evita alarme falso causado por pico isolado);
 *    7. Apresentacao imediata na IHM (OLED + LEDs + buzzer), sem depender da rede;
 *    8. Reducao de trafego: envia 1 pacote consolidado a cada 20 s em vez das
 *       10 leituras brutas do periodo.
 *
 *  NA NUVEM (ThingSpeak):
 *    - Armazenamento do historico, graficos, visualizacao publica e acesso remoto.
 *
 *  O sistema continua medindo, classificando e avisando localmente mesmo com o
 *  Wi-Fi fora do ar: a nuvem e complemento, nao dependencia.
 * ==========================================================================*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

/* ===========================================================================
 *  1) CONFIGURACAO - ajuste esta secao antes de usar
 * ========================================================================= */

/* --- Rede Wi-Fi (no Wokwi use sempre "Wokwi-GUEST", sem senha, canal 6) --- */
const char *WIFI_SSID  = "Wokwi-GUEST";
const char *WIFI_SENHA = "";
const int   WIFI_CANAL = 6;

/* --- ThingSpeak ---------------------------------------------------------- */
/* Substitua pela "Write API Key" do seu canal (aba API Keys do ThingSpeak).  */
const char *TS_WRITE_API_KEY = "7CJ8UFWQY468B6X1";
const char *TS_URL_UPDATE    = "http://api.thingspeak.com/update";

/* --- Pinagem ------------------------------------------------------------- */
#define PINO_DHT           4    /* Dado do DHT22                             */
#define TIPO_DHT           DHT22
#define PINO_I2C_SDA      21    /* OLED SSD1306                              */
#define PINO_I2C_SCL      22
#define PINO_LED_VERDE    25    /* Ambiente adequado                         */
#define PINO_LED_AMARELO  26    /* Atencao                                   */
#define PINO_LED_VERMELHO 27    /* Alerta / critico                          */
#define PINO_BUZZER       14    /* Aviso sonoro                              */
#define PINO_BOTAO        15    /* Troca de tela (curto) / silenciar (longo)  */

/* --- Display ------------------------------------------------------------- */
#define OLED_LARGURA  128
#define OLED_ALTURA    64
#define OLED_ENDERECO 0x3C

/* --- Temporizacao (tudo nao bloqueante, baseado em millis) --------------- */
const uint32_t INTERVALO_LEITURA_MS = 2000;   /* DHT22 aceita no maximo 0,5 Hz */
const uint32_t INTERVALO_TELA_MS    = 4000;   /* rotacao automatica das telas  */
const uint32_t INTERVALO_REDESENHO  = 500;    /* refresh do OLED               */
const uint32_t INTERVALO_ENVIO_MS   = 20000;  /* ThingSpeak free: min. 15 s    */
const uint32_t INTERVALO_RECONEXAO  = 15000;  /* nova tentativa de Wi-Fi       */

/* --- Janela de processamento local --------------------------------------- */
const uint8_t TAM_JANELA = 10;                /* 10 amostras x 2 s = 20 s      */

/* --- Limites previamente estabelecidos ------------------------------------
 *  Referencias para ambientes de treinamento esportivo: conforto termico
 *  entre 18 C e 26 C, umidade relativa entre 40% e 60%, e faixas de risco do
 *  indice de calor conforme escala NOAA/INMET.
 * ------------------------------------------------------------------------ */
const float LIM_TEMP_ATENCAO    = 28.0;  /* C - reduzir intensidade           */
const float LIM_TEMP_ALERTA     = 32.0;  /* C - encurtar blocos, hidratar     */
const float LIM_UMID_MUITO_SECA = 20.0;  /* % - risco respiratorio            */
const float LIM_UMID_MINIMA     = 30.0;  /* % - ar seco                       */
const float LIM_UMID_MAXIMA     = 70.0;  /* % - abafado, suor nao evapora     */
const float LIM_UMID_CRITICA    = 80.0;  /* % - troca termica prejudicada     */
const float LIM_IC_ATENCAO      = 27.0;  /* C - cautela                       */
const float LIM_IC_ALERTA       = 32.0;  /* C - cautela extrema               */
const float LIM_IC_CRITICO      = 41.0;  /* C - perigo, suspender atividade   */

/* Amostras consecutivas necessarias para confirmar mudanca de condicao.     */
const uint8_t AMOSTRAS_CONFIRMACAO = 3;

/* Falhas seguidas apos as quais o valor exibido deixa de ser considerado atual. */
const uint8_t FALHAS_PARA_INVALIDAR = 3;

/* Faixa de medicao do DHT22: fora disso a leitura e descartada como invalida. */
const float TEMP_MIN_VALIDA = -40.0, TEMP_MAX_VALIDA =  80.0;
const float UMID_MIN_VALIDA =   0.0, UMID_MAX_VALIDA = 100.0;

/* ===========================================================================
 *  2) TIPOS E ESTRUTURAS
 * ========================================================================= */

enum Condicao {
  COND_ADEQUADA = 0,   /* verde                                */
  COND_ATENCAO  = 1,   /* amarelo                              */
  COND_ALERTA   = 2,   /* vermelho                             */
  COND_CRITICA  = 3    /* vermelho piscando + buzzer           */
};

const char *NOME_CONDICAO[4] = { "ADEQUADO", "ATENCAO", "ALERTA", "CRITICO" };

struct Avaliacao {
  Condicao    nivel;
  const char *motivo;
};

/* Janela movel circular usada no processamento local das medicoes.          */
struct JanelaMedicoes {
  float   amostras[TAM_JANELA];
  uint8_t indice     = 0;   /* proxima posicao de escrita       */
  uint8_t quantidade = 0;   /* amostras validas armazenadas     */

  void adicionar(float valor) {
    amostras[indice] = valor;
    indice = (indice + 1) % TAM_JANELA;
    if (quantidade < TAM_JANELA) quantidade++;
  }

  bool vazia() const { return quantidade == 0; }

  /* k = 0 devolve a amostra mais antiga ainda presente na janela            */
  float emOrdem(uint8_t k) const {
    uint8_t inicio = (indice + TAM_JANELA - quantidade) % TAM_JANELA;
    return amostras[(inicio + k) % TAM_JANELA];
  }

  float media() const {
    if (quantidade == 0) return NAN;
    float soma = 0;
    for (uint8_t i = 0; i < quantidade; i++) soma += amostras[i];
    return soma / quantidade;
  }

  float minimo() const {
    if (quantidade == 0) return NAN;
    float m = amostras[0];
    for (uint8_t i = 1; i < quantidade; i++) if (amostras[i] < m) m = amostras[i];
    return m;
  }

  float maximo() const {
    if (quantidade == 0) return NAN;
    float m = amostras[0];
    for (uint8_t i = 1; i < quantidade; i++) if (amostras[i] > m) m = amostras[i];
    return m;
  }

  /* Tendencia: media da metade recente menos media da metade antiga.        */
  float variacao() const {
    if (quantidade < 4) return 0.0;
    uint8_t metade  = quantidade / 2;
    float   antiga  = 0;
    float   recente = 0;
    for (uint8_t i = 0; i < metade; i++) antiga += emOrdem(i);
    for (uint8_t i = quantidade - metade; i < quantidade; i++) recente += emOrdem(i);
    return (recente / metade) - (antiga / metade);
  }

  /* Quantas amostras da janela ultrapassaram o limite estabelecido.         */
  uint8_t acimaDe(float limite) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < quantidade; i++) if (amostras[i] > limite) n++;
    return n;
  }
};

/*  Prototipos declarados explicitamente: funcoes que recebem ou devolvem
 *  tipos proprios precisam ser conhecidas antes que o pre-processador do
 *  Arduino gere os prototipos automaticos.                                 */
Avaliacao avaliarAmbiente(float tMedia, float uMedia, float icMedia);
void      aplicarConfirmacao(const Avaliacao &aval);

/* ===========================================================================
 *  3) OBJETOS E ESTADO GLOBAL
 * ========================================================================= */

DHT dht(PINO_DHT, TIPO_DHT);
Adafruit_SSD1306 display(OLED_LARGURA, OLED_ALTURA, &Wire, -1);

JanelaMedicoes janelaTemp;
JanelaMedicoes janelaUmid;
JanelaMedicoes janelaIC;      /* indice de calor calculado na borda          */

/* Ultima medicao valida                                                     */
float tempAtual = NAN, umidAtual = NAN, icAtual = NAN;

/* Resultado do processamento local                                          */
Condicao    condicaoAtual     = COND_ADEQUADA;
Condicao    condicaoCandidata = COND_ADEQUADA;
uint8_t     contagemCandidata = 0;
const char *motivoAtual       = "Aguardando leitura";

/* Estatisticas de operacao                                                  */
uint32_t leiturasValidas = 0, leiturasInvalidas = 0;
uint8_t  falhasSeguidas  = 0;
uint32_t eventosAtencao  = 0;            /* transicoes para ATENCAO ou pior   */
uint32_t enviosOk = 0, enviosFalha = 0;
int32_t  ultimoEntryId  = -1;
uint32_t tUltimoEnvioOk = 0;

/* Controle de tempo das tarefas                                             */
uint32_t tUltimaLeitura = 0, tUltimaTela = 0, tUltimoRedesenho = 0;
uint32_t tUltimoEnvio   = 0, tUltimaTentativaWifi = 0;

/* IHM                                                                       */
uint8_t       telaAtual   = 0;
const uint8_t TOTAL_TELAS = 3;
bool          displayOk   = false;
bool          silenciado  = false;

/* Botao                                                                     */
bool     botaoAnterior     = HIGH;
uint32_t tBotaoPressionado = 0;
bool     longoTratado      = false;

/* Buzzer                                                                    */
bool     buzzerLigado = false;

/* ===========================================================================
 *  4) PROCESSAMENTO LOCAL (EDGE)
 * ========================================================================= */

/*  Classifica o ambiente combinando indice de calor, temperatura e umidade.
 *  Prevalece sempre o fator mais restritivo (pior nivel encontrado).        */
Avaliacao avaliarAmbiente(float tMedia, float uMedia, float icMedia) {
  Avaliacao r = { COND_ADEQUADA, "Condicoes adequadas" };

  /* a) Indice de calor - melhor indicador de estresse termico no esporte    */
  if (icMedia >= LIM_IC_CRITICO) {
    r.nivel = COND_CRITICA; r.motivo = "Indice calor: perigo";
  } else if (icMedia >= LIM_IC_ALERTA) {
    r.nivel = COND_ALERTA;  r.motivo = "Sensacao term. alta";
  } else if (icMedia >= LIM_IC_ATENCAO) {
    r.nivel = COND_ATENCAO; r.motivo = "Sensacao term. sobe";
  }

  /* b) Temperatura do ar                                                    */
  if (tMedia >= LIM_TEMP_ALERTA && r.nivel < COND_ALERTA) {
    r.nivel = COND_ALERTA;  r.motivo = "Temperatura elevada";
  } else if (tMedia >= LIM_TEMP_ATENCAO && r.nivel < COND_ATENCAO) {
    r.nivel = COND_ATENCAO; r.motivo = "Temp. acima do ideal";
  }

  /* c) Umidade relativa                                                     */
  if ((uMedia >= LIM_UMID_CRITICA || uMedia <= LIM_UMID_MUITO_SECA) &&
      r.nivel < COND_ALERTA) {
    r.nivel  = COND_ALERTA;
    r.motivo = (uMedia >= LIM_UMID_CRITICA) ? "Umidade muito alta" : "Ar muito seco";
  } else if ((uMedia >= LIM_UMID_MAXIMA || uMedia <= LIM_UMID_MINIMA) &&
             r.nivel < COND_ATENCAO) {
    r.nivel  = COND_ATENCAO;
    r.motivo = (uMedia >= LIM_UMID_MAXIMA) ? "Umidade alta" : "Umidade baixa";
  }

  return r;
}

/*  Confirma a mudanca de condicao apenas apos N amostras consecutivas.
 *  Filtra picos isolados (porta aberta, alguem na frente do sensor, etc).   */
void aplicarConfirmacao(const Avaliacao &aval) {
  if (aval.nivel == condicaoAtual) {
    contagemCandidata = 0;
    motivoAtual = aval.motivo;
    return;
  }

  if (aval.nivel == condicaoCandidata) {
    contagemCandidata++;
  } else {
    condicaoCandidata = aval.nivel;
    contagemCandidata = 1;
  }

  if (contagemCandidata >= AMOSTRAS_CONFIRMACAO) {
    Condicao anterior = condicaoAtual;
    condicaoAtual     = aval.nivel;
    motivoAtual       = aval.motivo;
    contagemCandidata = 0;

    if (condicaoAtual >= COND_ATENCAO && anterior < COND_ATENCAO) eventosAtencao++;

    Serial.printf("[EDGE] Condicao %s -> %s (%s)\n",
                  NOME_CONDICAO[anterior], NOME_CONDICAO[condicaoAtual], motivoAtual);
  }
}

/*  Ciclo completo de aquisicao + processamento na borda.                    */
void executarCicloDeMedicao() {
  float t = dht.readTemperature();
  float u = dht.readHumidity();

  /* 1) Validacao: descarta leitura corrompida ou fora da faixa fisica       */
  if (isnan(t) || isnan(u) ||
      t < TEMP_MIN_VALIDA || t > TEMP_MAX_VALIDA ||
      u < UMID_MIN_VALIDA || u > UMID_MAX_VALIDA) {
    leiturasInvalidas++;
    Serial.printf("[EDGE] Leitura invalida descartada (T=%.1f UR=%.1f)\n", t, u);
    /* Sensor falhando de forma persistente: nao exibir valor desatualizado  */
    if (++falhasSeguidas >= FALHAS_PARA_INVALIDAR) {
      tempAtual = umidAtual = icAtual = NAN;
    }
    return;
  }

  /* 2) Indice de calor calculado localmente (sensacao termica, em Celsius)  */
  float ic = dht.computeHeatIndex(t, u, false);

  tempAtual = t;
  umidAtual = u;
  icAtual   = ic;
  leiturasValidas++;
  falhasSeguidas = 0;

  /* 3) Janela movel com as ultimas TAM_JANELA medicoes                      */
  janelaTemp.adicionar(t);
  janelaUmid.adicionar(u);
  janelaIC.adicionar(ic);

  /* 4) Classificacao sobre as MEDIAS (mais estavel que o valor instantaneo) */
  Avaliacao aval = avaliarAmbiente(janelaTemp.media(), janelaUmid.media(),
                                   janelaIC.media());
  aplicarConfirmacao(aval);

  Serial.printf("[DADO] T=%.1fC UR=%.1f%% IC=%.1fC | media T=%.1f UR=%.1f | "
                "acima do limite=%u/%u | estado=%s\n",
                t, u, ic, janelaTemp.media(), janelaUmid.media(),
                janelaTemp.acimaDe(LIM_TEMP_ATENCAO), janelaTemp.quantidade,
                NOME_CONDICAO[condicaoAtual]);
}

/* ===========================================================================
 *  5) IHM LOCAL - OLED, LEDs E BUZZER
 * ========================================================================= */

void buzzerLigar(uint32_t frequencia) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  tone(PINO_BUZZER, frequencia);
#else
  ledcSetup(0, frequencia, 8);
  ledcAttachPin(PINO_BUZZER, 0);
  ledcWrite(0, 128);
#endif
}

void buzzerDesligar() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  noTone(PINO_BUZZER);
#else
  ledcWrite(0, 0);
#endif
  digitalWrite(PINO_BUZZER, LOW);
}

/*  LEDs indicam o estado mesmo a distancia; buzzer apita em alerta/critico. */
void atualizarIndicadores(uint32_t agora) {
  bool piscar = (agora / 400) % 2;

  digitalWrite(PINO_LED_VERDE,    condicaoAtual == COND_ADEQUADA);
  digitalWrite(PINO_LED_AMARELO,  condicaoAtual == COND_ATENCAO);
  digitalWrite(PINO_LED_VERMELHO, condicaoAtual == COND_ALERTA ||
                                  (condicaoAtual == COND_CRITICA && piscar));

  /* Padrao sonoro nao bloqueante                                            */
  uint32_t periodo = 0, duracao = 0, freq = 0;
  if (!silenciado && condicaoAtual == COND_CRITICA) {
    periodo = 2000; duracao = 200; freq = 2000;
  } else if (!silenciado && condicaoAtual == COND_ALERTA) {
    periodo = 6000; duracao = 120; freq = 1500;
  }

  if (periodo == 0) {
    if (buzzerLigado) { buzzerDesligar(); buzzerLigado = false; }
    return;
  }

  bool deveApitar = (agora % periodo) < duracao;
  if (deveApitar && !buzzerLigado) {
    buzzerLigar(freq); buzzerLigado = true;
  } else if (!deveApitar && buzzerLigado) {
    buzzerDesligar();  buzzerLigado = false;
  }
}

void desenharCabecalho(const char *titulo) {
  display.fillRect(0, 0, OLED_LARGURA, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 2);
  display.print(titulo);

  /* Indicador de rede no canto do cabecalho                                 */
  display.setCursor(OLED_LARGURA - 24, 2);
  display.print(WiFi.status() == WL_CONNECTED ? "NET" : "---");
  display.setTextColor(SSD1306_WHITE);
}

/*  TELA 1 - valores monitorados + estado do ambiente (minimo exigido da IHM) */
void telaPrincipal() {
  desenharCabecalho("PELE ACADEMIA");

  display.setTextSize(2);
  display.setCursor(0, 16);
  if (isnan(tempAtual)) display.print("--.-"); else display.print(tempAtual, 1);
  display.setTextSize(1);
  display.print(" C");

  display.setTextSize(2);
  display.setCursor(74, 16);
  if (isnan(umidAtual)) display.print("--"); else display.print(umidAtual, 0);
  display.setTextSize(1);
  display.print(" %");

  display.setCursor(0, 34);
  display.print("Sensacao: ");
  if (isnan(icAtual)) display.print("--.-"); else display.print(icAtual, 1);
  display.print(" C");

  /* Faixa invertida com o estado -> leitura rapida pelo tecnico/professor   */
  display.fillRect(0, 44, OLED_LARGURA, 20, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 46);
  display.print(NOME_CONDICAO[condicaoAtual]);
  display.setCursor(2, 56);
  display.print(motivoAtual);
  display.setTextColor(SSD1306_WHITE);
}

/*  TELA 2 - resultado do processamento local (medias, extremos, tendencia)  */
void telaProcessamento() {
  desenharCabecalho("PROCESSAMENTO");

  display.setCursor(0, 14);
  display.printf("Amostras: %u/%u", janelaTemp.quantidade, TAM_JANELA);

  display.setCursor(0, 24);
  if (janelaTemp.vazia()) display.print("Media: --");
  else display.printf("Media T:%.1fC UR:%.0f%%", janelaTemp.media(), janelaUmid.media());

  display.setCursor(0, 34);
  if (janelaTemp.vazia()) display.print("Min/Max: --");
  else display.printf("T min %.1f max %.1f", janelaTemp.minimo(), janelaTemp.maximo());

  display.setCursor(0, 44);
  float var = janelaTemp.variacao();
  const char *tendencia = (var > 0.3) ? "SUBINDO" : (var < -0.3 ? "CAINDO" : "ESTAVEL");
  display.printf("Tendencia: %s", tendencia);

  display.setCursor(0, 54);
  display.printf("Acima lim: %u/%u", janelaTemp.acimaDe(LIM_TEMP_ATENCAO),
                 janelaTemp.quantidade);
}

/*  TELA 3 - conectividade e status do envio para a nuvem                    */
void telaRede() {
  desenharCabecalho("REDE / NUVEM");

  display.setCursor(0, 14);
  if (WiFi.status() == WL_CONNECTED) {
    display.printf("WiFi: %d dBm", WiFi.RSSI());
    display.setCursor(0, 24);
    display.print(WiFi.localIP().toString());
  } else {
    display.print("WiFi: desconectado");
    display.setCursor(0, 24);
    display.print("Modo local ativo");
  }

  display.setCursor(0, 34);
  display.printf("Envios OK:%lu ERR:%lu", (unsigned long)enviosOk,
                 (unsigned long)enviosFalha);

  display.setCursor(0, 44);
  if (tUltimoEnvioOk == 0) display.print("Ultimo envio: --");
  else display.printf("Ultimo envio: %lus",
                      (unsigned long)((millis() - tUltimoEnvioOk) / 1000));

  display.setCursor(0, 54);
  display.printf("Alertas: %lu", (unsigned long)eventosAtencao);
}

void atualizarIHM() {
  if (!displayOk) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  switch (telaAtual) {
    case 0:  telaPrincipal();     break;
    case 1:  telaProcessamento(); break;
    default: telaRede();          break;
  }

  /* Marca discreta no cabecalho quando o buzzer esta silenciado             */
  if (silenciado) display.drawLine(OLED_LARGURA - 6, 2, OLED_LARGURA - 2, 8, SSD1306_BLACK);

  display.display();
}

/*  Botao: toque curto troca de tela, toque longo (>1 s) silencia o buzzer.  */
void tratarBotao(uint32_t agora) {
  bool estado = digitalRead(PINO_BOTAO);

  if (botaoAnterior == HIGH && estado == LOW) {          /* borda de descida  */
    tBotaoPressionado = agora;
    longoTratado = false;
  }

  if (estado == LOW && !longoTratado && (agora - tBotaoPressionado) > 1000) {
    silenciado   = !silenciado;
    longoTratado = true;
    Serial.printf("[IHM] Buzzer %s\n", silenciado ? "silenciado" : "reativado");
  }

  if (botaoAnterior == LOW && estado == HIGH) {          /* borda de subida   */
    if (!longoTratado && (agora - tBotaoPressionado) > 40) {
      telaAtual   = (telaAtual + 1) % TOTAL_TELAS;
      tUltimaTela = agora;                    /* adia a rotacao automatica    */
    }
  }
  botaoAnterior = estado;
}

/* ===========================================================================
 *  6) CONECTIVIDADE E NUVEM
 * ========================================================================= */

void conectarWifi(bool primeiraVez) {
  Serial.printf("[WIFI] Conectando em \"%s\"...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA, WIFI_CANAL);

  if (primeiraVez) {        /* no boot vale a pena aguardar um pouco pela rede */
    uint32_t limite = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < limite) delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("[WIFI] Conectado. IP %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("[WIFI] Sem conexao - sistema segue operando localmente");
}

void cuidarDaConexao(uint32_t agora) {
  if (WiFi.status() == WL_CONNECTED) return;
  if (agora - tUltimaTentativaWifi < INTERVALO_RECONEXAO) return;
  tUltimaTentativaWifi = agora;
  WiFi.disconnect();
  conectarWifi(false);
}

/*  Envia o PACOTE CONSOLIDADO para o ThingSpeak.
 *  Nao vao as 10 leituras brutas, e sim o resultado do processamento feito
 *  na borda: menos trafego e mais informacao util por requisicao.           */
void enviarParaThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[NUVEM] Envio adiado: sem Wi-Fi");
    return;
  }
  if (janelaTemp.vazia()) return;

  String url = String(TS_URL_UPDATE);
  url += "?api_key="; url += TS_WRITE_API_KEY;
  url += "&field1=";  url += String(tempAtual, 1);                          /* T instantanea    */
  url += "&field2=";  url += String(umidAtual, 1);                          /* UR instantanea   */
  url += "&field3=";  url += String(janelaTemp.media(), 2);                 /* T media          */
  url += "&field4=";  url += String(janelaUmid.media(), 2);                 /* UR media         */
  url += "&field5=";  url += String(janelaIC.media(), 2);                   /* indice de calor  */
  url += "&field6=";  url += String((int)condicaoAtual);                    /* nivel 0..3       */
  url += "&field7=";  url += String(janelaTemp.acimaDe(LIM_TEMP_ATENCAO));  /* acima do limite  */
  url += "&field8=";  url += String(WiFi.RSSI());                           /* sinal Wi-Fi      */

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.begin(url);
  int codigo = http.GET();

  if (codigo == HTTP_CODE_OK) {
    int entrada = http.getString().toInt();
    if (entrada > 0) {
      enviosOk++;
      ultimoEntryId  = entrada;
      tUltimoEnvioOk = millis();
      Serial.printf("[NUVEM] Enviado com sucesso (entry #%d)\n", entrada);
    } else {
      enviosFalha++;
      Serial.println("[NUVEM] Resposta 0 - verifique a Write API Key ou o "
                     "intervalo minimo de 15 s");
    }
  } else {
    enviosFalha++;
    Serial.printf("[NUVEM] Falha HTTP: %d\n", codigo);
  }
  http.end();
}

/* ===========================================================================
 *  7) SETUP E LOOP
 * ========================================================================= */

void mostrarAbertura() {
  if (!displayOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(6, 8);
  display.print("PELE");
  display.setCursor(6, 26);
  display.print("ACADEMIA");
  display.setTextSize(1);
  display.setCursor(6, 48);
  display.print("Monitor ambiental");
  display.setCursor(6, 56);
  display.print("Edge + ThingSpeak");
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Pele Academia | Monitoramento ambiental (ESP32 / Edge) ===");

  pinMode(PINO_LED_VERDE,    OUTPUT);
  pinMode(PINO_LED_AMARELO,  OUTPUT);
  pinMode(PINO_LED_VERMELHO, OUTPUT);
  pinMode(PINO_BUZZER,       OUTPUT);
  pinMode(PINO_BOTAO,        INPUT_PULLUP);

  /* Teste rapido dos LEDs                                                   */
  digitalWrite(PINO_LED_VERDE,    HIGH);
  digitalWrite(PINO_LED_AMARELO,  HIGH);
  digitalWrite(PINO_LED_VERMELHO, HIGH);
  delay(400);
  digitalWrite(PINO_LED_VERDE,    LOW);
  digitalWrite(PINO_LED_AMARELO,  LOW);
  digitalWrite(PINO_LED_VERMELHO, LOW);

  dht.begin();

  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
  displayOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ENDERECO);
  if (!displayOk) Serial.println("[IHM] OLED nao encontrado no endereco 0x3C");
  mostrarAbertura();
  delay(1800);

  conectarWifi(true);

  uint32_t agora = millis();
  tUltimaLeitura       = agora - INTERVALO_LEITURA_MS;  /* 1a leitura imediata */
  tUltimaTela          = agora;
  tUltimoRedesenho     = agora;
  tUltimoEnvio         = agora;                         /* 1o envio em 20 s    */
  tUltimaTentativaWifi = agora;
}

void loop() {
  uint32_t agora = millis();

  /* Tarefa 1 - IHM: leitura do botao (resposta imediata)                    */
  tratarBotao(agora);

  /* Tarefa 2 - aquisicao + processamento local                              */
  if (agora - tUltimaLeitura >= INTERVALO_LEITURA_MS) {
    tUltimaLeitura = agora;
    executarCicloDeMedicao();
  }

  /* Tarefa 3 - rotacao automatica das telas                                 */
  if (agora - tUltimaTela >= INTERVALO_TELA_MS) {
    tUltimaTela = agora;
    telaAtual   = (telaAtual + 1) % TOTAL_TELAS;
  }

  /* Tarefa 4 - redesenho do OLED                                            */
  if (agora - tUltimoRedesenho >= INTERVALO_REDESENHO) {
    tUltimoRedesenho = agora;
    atualizarIHM();
  }

  /* Tarefa 5 - LEDs e buzzer                                                */
  atualizarIndicadores(agora);

  /* Tarefa 6 - manutencao da conexao Wi-Fi                                  */
  cuidarDaConexao(agora);

  /* Tarefa 7 - publicacao na nuvem                                          */
  if (agora - tUltimoEnvio >= INTERVALO_ENVIO_MS) {
    tUltimoEnvio = agora;
    enviarParaThingSpeak();
  }
}
