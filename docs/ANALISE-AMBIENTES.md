# Leitura dos resultados × condições dos ambientes de treinamento

Este documento relaciona o que o protótipo mede ao que acontece na prática
esportiva da Pelé Academia — atendendo ao critério 12 da avaliação.

---

## 1. Por que temperatura e umidade juntas

Durante o exercício o corpo produz calor e precisa dissipá-lo, principalmente
pela evaporação do suor. Dois fatores atrapalham esse processo:

- **temperatura alta** reduz a diferença térmica entre o corpo e o ar;
- **umidade alta** satura o ar e impede a evaporação do suor — o atleta transpira
  muito, mas se resfria pouco.

Por isso o ESP32 não olha a temperatura isolada: ele calcula localmente o
**índice de calor**, que combina as duas variáveis e representa a temperatura
que o corpo efetivamente “sente”. É esse valor que comanda a classificação.

> A 30 °C com 40 % de umidade o índice de calor fica próximo dos 30 °C. A mesma
> 30 °C com 80 % de umidade sobe para cerca de 38 °C. O termômetro é o mesmo, o
> risco para o atleta não.

---

## 2. Como ler cada faixa

| Nível | Índice de calor | O que acontece com o atleta | Ação recomendada |
|---|---|---|---|
| **ADEQUADO** | < 27 °C | Termorregulação normal | Treino conforme planejado |
| **ATENÇÃO** | 27 – 32 °C | Fadiga antecipada, mais sede, queda de rendimento no fim do treino | Pausas para hidratação a cada 15–20 min; observar iniciantes e crianças |
| **ALERTA** | 32 – 41 °C | Risco de cãibras e exaustão pelo calor | Reduzir a intensidade, encurtar os blocos, ampliar as pausas, priorizar sombra |
| **CRÍTICO** | ≥ 41 °C | Risco de intermação (*heat stroke*) | Suspender a atividade ou transferir para ambiente climatizado |

E para a umidade isoladamente:

| Umidade relativa | Efeito no treino |
|---|---|
| **< 20 %** | Ar muito seco: irritação das vias aéreas, desidratação silenciosa (o suor evapora sem ser percebido) |
| **20 – 30 %** | Seco: reforçar hidratação, atenção em modalidades de longa duração |
| **30 – 70 %** | Faixa adequada para a prática esportiva |
| **70 – 80 %** | Abafado: o suor escorre em vez de evaporar, sensação térmica sobe |
| **> 80 %** | Troca térmica muito prejudicada, mesmo com temperatura moderada; piso pode ficar escorregadio em quadras |

---

## 3. O que cada gráfico do ThingSpeak revela

### `field1` × `field3` — temperatura instantânea × média

A curva instantânea oscila com o ruído do sensor e com eventos pontuais (porta
aberta, alguém passando). A média móvel calculada na borda entrega a tendência
real do ambiente. **Comparar as duas curvas evidencia o ganho do processamento
local:** o alerta dispara pela condição sustentada, não por um pico de 2 s.

### `field5` — índice de calor ao longo do dia

É o gráfico de planejamento. A curva típica de um ambiente sem climatização
mostra o pico entre 13 h e 16 h. As faixas de 27 / 32 / 41 °C identificam a
**janela segura para treino de alta intensidade** — em geral antes das 10 h e
depois das 17 h.

### `field4` — umidade média

Interpretada junto com a temperatura. Uma queda de umidade acompanhada de
aumento de temperatura indica aquecimento do ar; **umidade subindo com muitas
pessoas no ambiente indica ventilação insuficiente para a ocupação**.

### `field6` — nível da condição

Histórico auditável do ambiente. Permite contar, em uma semana, quantas horas
cada local ficou fora da faixa adequada — o argumento objetivo para justificar
investimento em ventilação, climatização ou sombreamento.

### `field7` — amostras acima do limite

Mede a **persistência** do problema dentro de cada janela de 20 s. Valores
próximos de 10 significam condição sustentada; valores baixos e intermitentes
indicam oscilação, não um problema estrutural do ambiente.

---

## 4. Aplicação por tipo de ambiente

| Ambiente | O que observar | Limite a ajustar no código |
|---|---|---|
| **Quadra coberta** | Calor acumulado sob a cobertura no fim da tarde; ventilação cruzada | Padrão do firmware |
| **Sala de musculação climatizada** | Eficiência do ar-condicionado sob ocupação alta; umidade baixa demais | Reduzir `LIM_TEMP_ATENCAO` para 26 °C |
| **Campo aberto** | Pico de índice de calor no meio do dia; exposição solar direta | Reduzir `LIM_IC_ATENCAO` para 26 °C (não há sombra) |
| **Piscina / área úmida** | Umidade quase sempre alta; o critério de umidade perde sentido | Elevar `LIM_UMID_MAXIMA` e guiar pelo índice de calor |
| **Sala de lutas / tatame** | Ambiente fechado, ocupação alta, pouca renovação de ar | Reduzir `LIM_UMID_MAXIMA` para 65 % |

Todos esses valores estão agrupados no início do `sketch.ino`, na seção
*“Limites previamente estabelecidos”*, e podem ser alterados sem mexer no resto
do código.

---

## 5. Conclusão

O protótipo entrega duas camadas complementares de informação:

- **na borda**, uma resposta imediata e independente de internet — o técnico olha
  o OLED ou o LED e sabe, em menos de um segundo, se pode manter o treino como
  planejado;
- **na nuvem**, o histórico que permite decisões de médio prazo — mudar horários
  de turmas, revisar a ventilação de um ambiente ou comprovar a necessidade de
  climatização.

É exatamente a divisão que caracteriza uma aplicação de **Edge Computing**:
a decisão urgente acontece onde o dado nasce, e a nuvem cuida da memória e da
visão de conjunto.

---

### Referências de faixa utilizadas

- Escala de índice de calor do **NOAA / National Weather Service**, adotada
  também pelo **INMET** (cautela a partir de 27 °C, cautela extrema a partir de
  32 °C, perigo a partir de 41 °C).
- Faixas de conforto térmico para ambientes de atividade física: 18 – 26 °C de
  temperatura e 40 – 60 % de umidade relativa.
