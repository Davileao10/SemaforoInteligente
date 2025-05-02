# 🚦 Semáforo Inteligente com Modos Adaptativos e Acessibilidade

Este projeto implementa um **semáforo inteligente** utilizando o **Raspberry Pi Pico (RP2040)** na placa **BitDog Lab**, com o objetivo de consolidar conhecimentos sobre tarefas no **FreeRTOS**. O sistema é funcional, acessível e inclui **modos adaptativos** para diferentes condições de tráfego, além de **sinalização sonora para pessoas cegas**.

---

## 📋 Descrição do Projeto

O Semáforo Inteligente simula um sistema de semáforo com **quatro modos de operação**:

- **Normal**
- **Noturno**
- **Alto Fluxo**
- **Baixo Fluxo**

Os modos são alternados pelo **Botão A**. O sistema utiliza os seguintes periféricos da **placa BitDog Lab**:

- **Matriz de LEDs WS2812 (8x8)**: exibe as cores do semáforo.
- **LED RGB**: reflete as fases do semáforo.
- **Display SSD1306**: mostra tempo restante, modo atual e barra de progresso.
- **Buzzers**: emitem sinais sonoros para acessibilidade.
- **Botões A e B**: A alterna os modos, B ativa o modo BOOTSEL.

O projeto foi desenvolvido com **tarefas do FreeRTOS**, sem uso de **filas, semáforos ou mutexes**, conforme exigido.

---

## 🚀 Funcionalidades

### 🕹️ Modos de Operação

#### 🔹 Modo Normal
- Ciclo: **Verde (20s) → Amarelo (3s) → Vermelho (20s) → Verde**
- **Sinalização sonora**:
  - **Verde**: 1 beep curto/segundo (*pode atravessar*)
  - **Amarelo**: beep rápido intermitente (*atenção*)
  - **Vermelho**: tom contínuo curto (*pare*)

#### 🔸 Modo Noturno
- Apenas **amarelo piscando** (0.5s ON / 1.5s OFF)
- **Beep lento** a cada 2 segundos

#### 🔶 Modo Alto Fluxo
- Ciclo: **Verde (25s) → Amarelo (3s) → Vermelho (15s)**
- Sons iguais ao **modo normal**

#### 🔽 Modo Baixo Fluxo
- Ciclo: **Vermelho (25s) → Amarelo (3s) → Verde (15s)**
- Sons iguais ao **modo normal**

---

## 💡 Representação Visual

- **Matriz WS2812**: mostra Verde, Amarelo ou Vermelho (pisca no modo noturno)
- **LED RGB**: sincronizado com o estado do semáforo
- **Display SSD1306**:
  - Título: `Semaf. Intelig.`
  - Contador de tempo (ex: `20 s`)
  - Modo atual (ex: `Modo: Normal`)
  - Barra de progresso (exceto no modo noturno)

---

## ♿ Acessibilidade

- **Sinalização sonora específica para cada fase** do semáforo
- Indicação clara para travessia segura, atenção e parada

---

## 🛠️ Como Usar

### ✅ Pré-requisitos

**Hardware**:
- Placa **BitDog Lab com RP2040**
- Display **SSD1306** (128x64 ou 128x32)

**Software**:
- **Pico SDK** instalado
- **FreeRTOS** configurado
- Bibliotecas: `ssd1306.h` e `font.h`

---

### ⚙️ Configuração do Display

Se estiver usando display 128x32, altere no código:

```c
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32 // ou 64 para displays maiores
```

---

### 🔧 Compilação e Upload

```bash
git clone <URL_DO_REPOSITORIO>
cd <REPOSITORIO>
```

- Copie os arquivos para seu ambiente do Pico SDK
- Compile com CMake
- Envie o firmware para a BitDog Lab (pressione Botão B para entrar no modo BOOTSEL)

---

### 🧪 Uso

- **Botão A**: alterna os modos
- **Botão B**: modo BOOTSEL
- Verifique no **display**, **LEDs**, **matriz** e **sinais sonoros** o comportamento correspondente

---

## 🔌 Periféricos Utilizados

| Periférico          | Pino(s)        | Função                                     |
|---------------------|----------------|--------------------------------------------|
| Matriz WS2812       | GPIO 7         | Exibe as cores do semáforo                 |
| LED RGB             | GPIO 13,11,12  | Cores sincronizadas com fases do semáforo  |
| Display SSD1306     | GPIO 14 (SDA), 15 (SCL) | Exibe informações e barra de progresso |
| Buzzers             | GPIO 10, 21    | Sinalização sonora                         |
| Botão A             | GPIO 5         | Alterna os modos de operação               |
| Botão B             | GPIO 6         | Entra no modo BOOTSEL                      |

---

## 📂 Estrutura do Código

```
├── blinkConta.c         # Código principal com as tarefas FreeRTOS
├── ws2812.pio           # Controle da matriz de LEDs WS2812
└── lib/
    ├── ssd1306.h        # Biblioteca do display SSD1306
    └── font.h           # Fonte para o display
```

### 📦 Tarefas FreeRTOS

- `vButtonATask`: alterna modos via botão A
- `vMatrixLedTask`: gerencia matriz WS2812 e tempos de fase
- `vRgbLedTask`: atualiza LED RGB
- `vBuzzerTask`: controla sinalização sonora
- `vDisplayTask`: exibe tempo, modo e barra de progresso

---

## 📸 Demonstração

📹 *[(https://drive.google.com/file/d/1jFHlrV4uuphZW4eyS9nfnmU64hD39Qc9/view?usp=drive_link)]*

