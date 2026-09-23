# ThingSpeak — criação do canal, gráficos e visualização pública

Guia completo para colocar o canal no ar e deixá-lo público, atendendo aos
critérios 7, 8 e 9 da avaliação.

---

## 1. Criar a conta e o canal

1. Acesse <https://thingspeak.com> e crie uma conta gratuita (MathWorks Account).
2. Menu **Channels → My Channels → New Channel**.
3. Preencha:

| Campo | Valor sugerido |
|---|---|
| **Name** | `Pele Academia - Monitoramento Ambiental` |
| **Description** | `Monitoramento de temperatura, umidade e indice de calor dos locais de treinamento. ESP32 com processamento na borda.` |

4. **Habilite as 8 caixas de campo** e nomeie exatamente assim:

| Campo | Nome no ThingSpeak |
|---|---|
| Field 1 | `Temperatura (C)` |
| Field 2 | `Umidade (%)` |
| Field 3 | `Temperatura Media (C)` |
| Field 4 | `Umidade Media (%)` |
| Field 5 | `Indice de Calor (C)` |
| Field 6 | `Nivel da Condicao (0-3)` |
| Field 7 | `Amostras Acima do Limite` |
| Field 8 | `Sinal WiFi (dBm)` |

5. Clique em **Save Channel**.

> A ordem importa: ela precisa bater com a montagem da URL na função
> `enviarParaThingSpeak()` do `sketch.ino`.

---

## 2. Copiar a Write API Key

1. Abra o canal e vá à aba **API Keys**.
2. Copie o valor de **Write API Key**.
3. No `sketch.ino`, substitua:

```cpp
const char *TS_WRITE_API_KEY = "COLOQUE_AQUI_SUA_WRITE_API_KEY";
```

> **Limite do plano gratuito:** um envio a cada **15 segundos**. O firmware usa
> 20 s (`INTERVALO_ENVIO_MS`), com folga. Se o Serial Monitor mostrar
> `Resposta 0`, quase sempre é envio rápido demais ou API Key incorreta.

---

## 3. Conferir se os dados estão chegando

Com a simulação rodando no Wokwi, o Serial Monitor deve mostrar:

```
[WIFI] Conectado. IP 10.10.0.2
[DADO] T=24.0C UR=55.0% IC=24.6C | media T=24.0 UR=55.0 | acima do limite=0/10 | estado=ADEQUADO
[NUVEM] Enviado com sucesso (entry #1)
```

Na aba **Private View** do canal os gráficos começam a se preencher em seguida.

Para testar manualmente, sem o ESP32, cole no navegador:

```
https://api.thingspeak.com/update?api_key=SUA_WRITE_API_KEY&field1=25.3&field2=58
```

Uma resposta diferente de `0` significa sucesso.

---

## 4. Criar os gráficos de acompanhamento

Na aba **Private View**, use **Add Visualization** / edite cada *Field Chart*
pelo ícone de lápis.

### Gráfico 1 — Temperatura: instantânea × média

| Opção | Valor |
|---|---|
| Field | 1 e 3 (dois gráficos lado a lado) |
| Title | `Temperatura - instantanea vs media` |
| X Axis | `Horario` · **Y Axis** `Temperatura (C)` |
| Type | `line` |
| Results | `60` |
| Y Axis Min / Max | `15` / `45` |

Mostra visualmente o efeito do processamento na borda: a curva do `field3` é
nitidamente mais suave que a do `field1`.

### Gráfico 2 — Umidade relativa

| Opção | Valor |
|---|---|
| Field | 4 (`Umidade Media`) |
| Title | `Umidade relativa media` |
| Type | `line` · **Results** `60` |
| Y Axis Min / Max | `0` / `100` |

### Gráfico 3 — Índice de calor (principal)

| Opção | Valor |
|---|---|
| Field | 5 |
| Title | `Indice de calor - risco para o treino` |
| Type | `line` · **Results** `100` |
| Y Axis Min / Max | `15` / `50` |

É o gráfico mais importante para a academia: as faixas 27 / 32 / 41 °C separam
treino normal, atenção, alerta e suspensão.

### Gráfico 4 — Nível da condição

| Opção | Valor |
|---|---|
| Field | 6 |
| Title | `Nivel da condicao ambiental (0=Adequado ... 3=Critico)` |
| Type | `step` (ou `column`) |
| Y Axis Min / Max | `0` / `3` |
| Results | `100` |

Transforma o histórico em um mapa fácil de auditar: dá para contar quantas vezes
e em que horários o ambiente saiu da faixa adequada.

### Gráfico 5 — Amostras acima do limite

| Opção | Valor |
|---|---|
| Field | 7 |
| Type | `column` · **Y Axis Min / Max** `0` / `10` |

### Widgets recomendados

**Add Widgets → Gauge**, um para cada:

| Widget | Field | Min / Max | Faixas de cor |
|---|---|---|---|
| Temperatura atual | 1 | 10 / 45 | verde até 28, amarelo até 32, vermelho acima |
| Umidade atual | 2 | 0 / 100 | vermelho até 30, verde até 70, vermelho acima |
| Índice de calor | 5 | 15 / 50 | verde até 27, amarelo até 32, vermelho acima |

**Add Widgets → Numeric Display** no `field6` deixa o nível atual em destaque no
topo do painel.

---

## 5. Tornar o canal público

1. Abra o canal e vá à aba **Sharing**.
2. Marque **“Share channel view with everyone”** (*Make public*).
3. Abra a aba **Public View** e confirme que os gráficos aparecem.
4. Copie a URL da barra de endereços — é este o link da entrega:

```
https://thingspeak.mathworks.com/channels/3502609
```

5. Teste em uma **janela anônima**, sem estar logado. Se abrir, está público.

> Os gráficos adicionados na *Private View* não aparecem automaticamente na
> *Public View*: confira a aba pública e, se necessário, adicione ali também.

---

## 6. Problemas comuns

| Sintoma | Causa provável | Solução |
|---|---|---|
| `[NUVEM] Resposta 0` | API Key errada ou envio < 15 s | conferir a Write API Key e `INTERVALO_ENVIO_MS` |
| `[NUVEM] Falha HTTP: -1` | sem internet na simulação | verificar se o Wi-Fi conectou (`NET` no cabeçalho do OLED) |
| Gráfico vazio | canal criado sem habilitar os campos | reabrir *Channel Settings* e marcar os 8 campos |
| Link público não abre deslogado | *Sharing* não salvo | remarcar *Make public* e salvar |
| Dados param após algum tempo | limite do plano gratuito | reduzir a frequência de envio |
