/*
  CP2 - Edge Computing & Computer Systems | FIAP

  O que o programa faz, em ordem:
    1) Mostra o logo da vinheria animado no LCD
    2) A cada 2 segundos le temperatura, umidade (DHT) e luz (LDR)
    3) A cada 10 segundos calcula a MEDIA das leituras
    4) Mostra os valores no LCD (uma tela de cada vez)
    5) Acende LEDs e toca o buzzer se algo estiver fora do ideal
    6) Guarda um log na EEPROM com a hora do RTC

  Status ideais:
    temperatura: 13 C +- 3 C  -> de 10 a 16 C
    umidade: de 60 % a 80 %
    luz:pouca luz (penumbra)

  Comandos pelo Serial Monitor (digite uma letra e Enter):
    C = temperatura em Celsius
    F = temperatura em Fahrenheit
    + = fuso horario +1
    - = fuso horario -1
    L = mostrar os logs gravados
 */

#include <Wire.h>
#include <EEPROM.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

// Pinos
const int PIN_DHT = 2;
const int PIN_LED_VERDE = 8;
const int PIN_LED_AMARELO = 9;
const int PIN_LED_VERMELHO = 10;
const int PIN_BUZZER = 11;
const int PIN_LDR = A0;

// OBJETOS
DHT dht(PIN_DHT, DHT22);
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

// LIMITES
const float TEMP_MIN = 10.0;
const float TEMP_MAX = 16.0;
const int UMID_MIN = 60;
const int UMID_MAX = 80;
const int LUZ_ESCURO_MAX = 30; // ate 30 %  = ambiente escuro (bom)
const int LUZ_CLARO_MIN = 60;  // 60 % ou mais = muito claro (ruim)
                               // entre 30 e 60 = meia luz (atencao)

//  CALIBRACAO DO LDR
// O LDR devolve um numero de 0 a 1023. Com o map() transformamos em 0 a 100 %.
const int LDR_ESCURO = 1023;
const int LDR_CLARO = 0;

//  EEPROM
// Endereco 0 = fuso (UTC) | 1 = unidade (0=C, 1=F) | 2 = marca de "ja configurado"
// A partir do endereco 16 ficam os logs (cada log ocupa 8 bytes).
const int ADDR_UTC = 0;
const int ADDR_UNIDADE = 1;
const int ADDR_CONFIGURADO = 2;
const int ADDR_LOGS = 16;
const int MAX_LOGS = 100;

struct Registro
{                   // um log
    uint32_t hora;  // hora do RTC (em UTC, em segundos)
    int16_t temp10; // temperatura x 10 (13.5 C vira 135)
    uint8_t umid;   // umidade em %
    uint8_t luz;    // luz em %
};

// variaveis
int fusoUTC = -3;   // lido da EEPROM no setup()
int unidade = 0;    // 0 = Celsius, 1 = Fahrenheit
int posicaoLog = 0; // em qual posicao o proximo log sera gravado

float tempMedia = 0; // resultados da ultima media de 10 s
int umidMedia = 0;
int luzMedia = 0;
bool temMedia = false; // false ate fechar a primeira media

int telaAtual = 0; // 0 = temperatura, 1 = umidade, 2 = luz

// Desenho da taca de vinho (4 quadros: vazia -> cheia) para a animacao
byte copos[4][8] = {
    {B11111, B10001, B10001, B01010, B00100, B00100, B00100, B01110},
    {B11111, B10001, B10001, B01110, B00100, B00100, B00100, B01110},
    {B11111, B10001, B11111, B01110, B00100, B00100, B00100, B01110},
    {B11111, B11111, B11111, B01110, B00100, B00100, B00100, B01110}};

//  ANIMACAO DO LOGO
void animacaoLogo()
{
    lcd.clear();

    // 1) as tacas nos cantos vao enchendo
    for (int quadro = 0; quadro < 4; quadro++)
    {
        lcd.createChar(0, copos[quadro]); // troca o desenho do caractere 0
        lcd.setCursor(0, 0);
        lcd.write((uint8_t)0);
        lcd.setCursor(15, 0);
        lcd.write((uint8_t)0);
        delay(350);
    }

    // 2) o nome aparece letra por letra
    const char *nome1 = "VINHERIA";
    const char *nome2 = "AGNELLO";
    for (int i = 0; i < 8; i++)
    {
        lcd.setCursor(4 + i, 0);
        lcd.print(nome1[i]);
        delay(120);
    }
    for (int i = 0; i < 7; i++)
    {
        lcd.setCursor(4 + i, 1);
        lcd.print(nome2[i]);
        delay(120);
    }

    delay(1500);
    lcd.clear();
}

//  configuração e logs EEPROM
void carregarConfiguracao()
{
    // Primeira vez que o programa roda: grava valores padrao
    if (EEPROM.read(ADDR_CONFIGURADO) != 42)
    {
        EEPROM.update(ADDR_UTC, (byte)-3); // fuso de Brasilia
        EEPROM.update(ADDR_UNIDADE, 0);    // Celsius
        EEPROM.update(ADDR_CONFIGURADO, 42);
    }
    fusoUTC = (int8_t)EEPROM.read(ADDR_UTC);
    unidade = EEPROM.read(ADDR_UNIDADE);
}

void salvarLog()
{
    Registro r;
    r.hora = rtc.now().unixtime(); // hora atual do RTC
    r.temp10 = (int16_t)(tempMedia * 10);
    r.umid = umidMedia;
    r.luz = luzMedia;

    EEPROM.put(ADDR_LOGS + posicaoLog * sizeof(Registro), r);

    posicaoLog++;
    if (posicaoLog >= MAX_LOGS)
        posicaoLog = 0; // volta ao inicio
}

void mostrarLogs()
{
    Serial.println(F("----- LOGS GRAVADOS -----"));
    for (int i = 0; i < MAX_LOGS; i++)
    {
        Registro r;
        EEPROM.get(ADDR_LOGS + i * sizeof(Registro), r);
        if (r.hora == 0xFFFFFFFFUL || r.hora == 0)
            continue; // posicao vazia

        DateTime d(r.hora + fusoUTC * 3600L); // converte UTC para hora local
        char texto[24];
        snprintf(texto, sizeof(texto), "%02d/%02d/%04d %02d:%02d:%02d",
                 d.day(), d.month(), d.year(), d.hour(), d.minute(), d.second());
        Serial.print(texto);
        Serial.print(F(" | Temp="));
        Serial.print(r.temp10 / 10.0, 1);
        Serial.print(F("C Umid="));
        Serial.print(r.umid);
        Serial.print(F("% Luz="));
        Serial.print(r.luz);
        Serial.println(F("%"));
    }
    Serial.println(F("-------------------------"));
}

//  LEITURA DO LDR (usa map para calibrar)
int lerLuz()
{
    int bruto = analogRead(PIN_LDR);                            // 0 a 1023
    int percentual = map(bruto, LDR_ESCURO, LDR_CLARO, 0, 100); // vira 0 a 100 %
    return constrain(percentual, 0, 100);                       // garante 0..100
}

//  alertas
void atualizarAlertas()
{
    bool tempFora = (tempMedia < TEMP_MIN || tempMedia > TEMP_MAX);
    bool umidFora = (umidMedia < UMID_MIN || umidMedia > UMID_MAX);
    bool luzMuitoClara = (luzMedia >= LUZ_CLARO_MIN);
    bool luzMeiaLuz = (luzMedia > LUZ_ESCURO_MAX && luzMedia < LUZ_CLARO_MIN);

    bool critico = tempFora || umidFora || luzMuitoClara;

    if (critico)
    {
        // VERMELHO + buzzer
        digitalWrite(PIN_LED_VERDE, LOW);
        digitalWrite(PIN_LED_AMARELO, LOW);
        digitalWrite(PIN_LED_VERMELHO, HIGH);
        tone(PIN_BUZZER, 1000);
    }
    else if (luzMeiaLuz)
    {
        // AMARELO: atencao
        digitalWrite(PIN_LED_VERDE, LOW);
        digitalWrite(PIN_LED_AMARELO, HIGH);
        digitalWrite(PIN_LED_VERMELHO, LOW);
        noTone(PIN_BUZZER);
    }
    else
    {
        // VERDE: tudo certo
        digitalWrite(PIN_LED_VERDE, HIGH);
        digitalWrite(PIN_LED_AMARELO, LOW);
        digitalWrite(PIN_LED_VERMELHO, LOW);
        noTone(PIN_BUZZER);
    }
}

// telas
void mostrarTela(int tela)
{
    lcd.clear();

    // Antes de fechar a primeira media ainda nao temos valores
    if (!temMedia)
    {
        lcd.setCursor(0, 0);
        lcd.print("Coletando dados");
        lcd.setCursor(0, 1);
        lcd.print("aguarde 10s...");
        return;
    }

    if (tela == 0)
    {
        // temperatura
        lcd.setCursor(0, 0);
        if (tempMedia > TEMP_MAX)
            lcd.print("Temp. ALTA");
        else if (tempMedia < TEMP_MIN)
            lcd.print("Temp. BAIXA");
        else
            lcd.print("Temperatura OK");

        float valor = tempMedia;
        if (unidade == 1)
            valor = tempMedia * 1.8 + 32; // converte para fahrenheit
        lcd.setCursor(0, 1);
        lcd.print("Temp. = ");
        lcd.print(valor, 1);
        lcd.print(unidade == 1 ? "F" : "C");
    }
    else if (tela == 1)
    {
        // umidade
        lcd.setCursor(0, 0);
        if (umidMedia > UMID_MAX)
            lcd.print("Umidade ALTA");
        else if (umidMedia < UMID_MIN)
            lcd.print("Umidade BAIXA");
        else
            lcd.print("Umidade OK");

        lcd.setCursor(0, 1);
        lcd.print("Umidade = ");
        lcd.print(umidMedia);
        lcd.print("%");
    }
    else
    {
        // luminosidade
        if (luzMedia >= LUZ_CLARO_MIN)
        {
            lcd.setCursor(0, 0);
            lcd.print("Ambiente muito");
            lcd.setCursor(0, 1);
            lcd.print("CLARO");
        }
        else if (luzMedia > LUZ_ESCURO_MAX)
        {
            lcd.setCursor(0, 0);
            lcd.print("Ambiente a meia");
            lcd.setCursor(0, 1);
            lcd.print("luz");
        }
        else
        {
            lcd.setCursor(0, 0);
            lcd.print("Ambiente escuro");
            lcd.setCursor(0, 1);
            lcd.print("Luz = ");
            lcd.print(luzMedia);
            lcd.print("%");
        }
    }
}

//  comandos
void lerComandosSerial()
{
    while (Serial.available() > 0)
    {
        char c = toupper(Serial.read());

        if (c == 'C')
        {
            unidade = 0;
            EEPROM.update(ADDR_UNIDADE, unidade);
            Serial.println(F("Unidade: Celsius"));
        }
        else if (c == 'F')
        {
            unidade = 1;
            EEPROM.update(ADDR_UNIDADE, unidade);
            Serial.println(F("Unidade: Fahrenheit"));
        }
        else if (c == '+')
        {
            if (fusoUTC < 14)
                fusoUTC++;
            EEPROM.update(ADDR_UTC, (byte)fusoUTC);
            Serial.print(F("Fuso: UTC"));
            Serial.println(fusoUTC);
        }
        else if (c == '-')
        {
            if (fusoUTC > -12)
                fusoUTC--;
            EEPROM.update(ADDR_UTC, (byte)fusoUTC);
            Serial.print(F("Fuso: UTC"));
            Serial.println(fusoUTC);
        }
        else if (c == 'L')
        {
            mostrarLogs();
        }
    }
}

void setup()
{
    Serial.begin(9600);

    pinMode(PIN_LED_VERDE, OUTPUT);
    pinMode(PIN_LED_AMARELO, OUTPUT);
    pinMode(PIN_LED_VERMELHO, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    dht.begin();
    lcd.init();
    lcd.backlight();

    rtc.begin();
    if (!rtc.isrunning())
    {
        // se o relogio estiver parado, acerta com a hora da compilacao
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    carregarConfiguracao();
    animacaoLogo();

    Serial.println(F("Vinheria Agnello - monitor iniciado"));
    Serial.println(F("Comandos: C, F, +, -, L"));
}

// cada volta dura 10 segundos (5 leituras de 2 s)
void loop()
{
    float somaTemp = 0;
    float somaUmid = 0;
    long somaLuz = 0;
    int leituras = 0;

    // ---- 5 leituras, uma a cada 2 s = 10 s ----
    for (int i = 0; i < 5; i++)
    {
        lerComandosSerial();

        float t = dht.readTemperature(); // sempre em Celsius
        float h = dht.readHumidity();
        int luz = lerLuz();

        // so conta a leitura se o DHT respondeu direito (isnan = "nao e numero")
        if (!isnan(t) && !isnan(h))
        {
            somaTemp += t;
            somaUmid += h;
            somaLuz += luz;
            leituras++;
        }

        mostrarTela(telaAtual);          // mostra a tela da vez
        telaAtual = (telaAtual + 1) % 3; // 0 -> 1 -> 2 -> 0 ...
        delay(2000);
    }

    // fim dos 10 s: calcula as medias
    if (leituras > 0)
    {
        tempMedia = somaTemp / leituras;
        umidMedia = (int)(somaUmid / leituras + 0.5); // +0.5 arredonda
        luzMedia = (int)(somaLuz / leituras);
        temMedia = true;

        atualizarAlertas();
        salvarLog();

        Serial.print(F("[MEDIA 10s] Temp="));
        Serial.print(tempMedia, 1);
        Serial.print(F("C Umid="));
        Serial.print(umidMedia);
        Serial.print(F("% Luz="));
        Serial.print(luzMedia);
        Serial.println(F("%"));
    }
    else
    {
        // o DHT nao respondeu nenhuma vez
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ERRO no sensor");
        lcd.setCursor(0, 1);
        lcd.print("DHT (verifique)");
    }
}
