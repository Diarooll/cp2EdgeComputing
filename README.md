# Vinheria Agnello – Monitor de Adega (CP2)

**Disciplina:** Edge Computing & Computer Systems – FIAP (1ESSRQ-2026)
**Grupo:** MonkeyNerds
**Integrantes:** 
Diego Olalla - rm575447
Victor Santos - rm575508
Diego Ramos - rm574620
Daniel Gomes - rm575813

## Links
- Simulação no Wokwi: (https://wokwi.com/projects/479050121307951105)
- Vídeo de explicação (até 3 min): [COLAR LINK]


## Faixas monitoradas
| Variável | Faixa ideal | Estado crítico |
|---|---|---|
| Temperatura | 10 °C a 16 °C (13 °C ± 3) | abaixo de 10 °C ou acima de 16 °C |
| Umidade | 60 % a 80 % | abaixo de 60 % ou acima de 80 % |
| Luminosidade | até 30 % (escuro) | 60 % ou mais (muito claro). Entre 30 % e 60 % é atenção ("meia luz") |

## Componentes
| Componente | Função | Pino |
|---|---|---|
| Arduino Uno | Microcontrolador | – |
| Protoboard | Distribui 5V e GND (trilhos) e o barramento I2C (colunas 1 e 2) | – |
| DHT22 (no hardware real: DHT11) | Temperatura e umidade | D2 |
| Módulo LDR | Luminosidade | A0 |
| LCD 16x2 I2C (0x27) | Exibição | A4 (SDA), A5 (SCL) |
| RTC DS1307 | Data/hora | A4 (SDA), A5 (SCL) |
| LED verde / amarelo / vermelho | Sinalização | D8 / D9 / D10 (resistor 220 Ω) |
| Buzzer | Alerta sonoro | D11 |


## Como funciona
1. **Abertura:** o LCD mostra o logo da vinheria
2. **Leitura:** a cada 2 s o Arduino lê temperatura, umidade e luz.
3. **Calibração do LDR:** a leitura bruta (0–1023) é convertida em 0–100 %.
4. **Média de 10 s:** a cada 10 s (5 leituras) é calculada a média de todos os valores. Os alertas e o log usam essa média.
5. **Display:** as telas alternam a cada 2 s
6. **Alertas:**
   - LED **verde**: tudo certo
   - LED **amarelo**: atenção (ambiente a meia luz).
   - LED **vermelho** + **buzzer**: temperatura, umidade ou luz em nível crítico.
7. **RTC:** fornece a data/hora de cada log (guardada em UTC).
8. **EEPROM:** guarda o setup (fuso UTC e unidade °C/°F) e os logs (data/hora, temperatura, umidade e luz). O setup continua salvo mesmo depois de desligar.

## Comandos pelo Serial Monitor (9600 baud)
Digite uma letra e dê Enter:

| Comando | O que faz |
|---|---|
| `C` / `F` | Temperatura em Celsius / Fahrenheit (salvo na EEPROM) |
| `+` / `-` | Aumenta / diminui o fuso horário (UTC), salvo na EEPROM |
| `L` | Mostra os logs gravados (com a hora no fuso configurado) |

## Como simular
1. Abra o projeto no Wokwi (link acima) e clique em **Start**.
2. Clique no DHT22 e altere temperatura/umidade para testar os estados OK, ALTA e BAIXA.
3. Clique no módulo LDR e altere a luminosidade (lux) para testar escuro, meia luz e muito claro.
4. Espere os 10 s da média para ver os LEDs e o buzzer reagirem.

