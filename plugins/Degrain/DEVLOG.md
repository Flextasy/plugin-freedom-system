# Degrain — История разработки

Гранулярный дилей-плагин на JUCE 8 (VST3 / AU / Standalone)
Размер окна: 900×480 px | WebView UI | APVTS

---

## Этап 1 — Фундамент (Stage 1)

Создан каркас плагина через систему Plugin Freedom System.

**Сделано:**
- CMakeLists.txt с поддержкой VST3, AU, Standalone
- PluginProcessor / PluginEditor базовые классы
- APVTS с первичным набором параметров
- WebView UI через juce::WebBrowserComponent
- Ресурсный провайдер (BinaryData: index.html, index.js, check_native_interop.js)
- Базовая структура HTML/CSS/JS

---

## Этап 2 — DSP (Stage 2)

Реализован гранулярный движок первого поколения.

**Сделано:**
- Кольцевой буфер задержки: 2^20 сэмплов на канал (kBufferSize = 1048576), битмаскинг
- Hermite cubic интерполяция (readBufferHermite — относительное чтение)
- Пул из 64 грейнов (kMaxGrains = 64), структура Grain
- Оконные огибающие: Hann, Triangle, Square (с cosine краями), Ramp
- Свободный аккумулятор для запуска грейнов (grainAccumulator)
- Линейный dry/wet миксер (SmoothedValue)
- Кэшированные указатели на параметры APVTS

**Параметры Phase 4.1:**
delay_time, grain_size, grain_rate, grain_shape, dry_wet

---

## Этап 3 — Расширенный DSP и GUI (Stage 3)

Добавлен полный набор параметров, характер и продвинутый GUI.

**DSP добавлено:**

*Phase 4.2 — Feedback & Character:*
- DC Blocker (1-pole HPF, R=0.995) — блокирует постоянную составляющую в петле
- OnePoleFilter (6 дБ/окт) → TwoPoleFilter (12 дБ/окт) → FourPoleFilter (24 дБ/окт)
- feedbackHP / feedbackLP — 24 дБ/окт фильтры в цепи обратной связи
- wetHP / wetLP — 24 дБ/окт фильтры на мокром выходе
- Schroeder Allpass Network — 4 стадии на канал, простые числа задержек (113, 337, 601, 1009 сэмплов @ 48kHz)
- Стерео spread: равномощное панорамирование через хэш (Knuth multiplicative hash)
- Детюн: случайное отклонение питча в центах через hash

*Phase 4.3 — Advanced Features:*
- Reverse mix: случайный реверс грейнов по hash
- Grain Mutation: сначала passCount-based (позже переработан)
- Frequency-Dependent Grain Size: BandSplitter (OnePoleFilter @ 250 Гц и 2500 Гц), BandEnergy с экспоненциальным сглаживанием
- Freeze система: freezeGain (crossfade 10 мс), freezeSnapPos, MIDI-триггер (note-on/off)
- Tempo Sync: PPQ через AudioPlayHead, kDivisionMultipliers[18] (от 1/1 до 1/32T)
- Pass Count Buffer: параллельный буфер uint8 для отслеживания проходов feedback

**Параметры Phase 4.2:**
feedback (0–120%), diffusion (0–100%), stereo_spread, detune (±50 ct), low_cut, high_cut

**Параметры Phase 4.3:**
reverse_mix, grain_mutation, freq_spread, freeze, delay_sync_toggle, delay_sync_division, grain_rate_sync_toggle, grain_rate_sync_division

**GUI (Stage 3):**
- 5 секций: DELAY / GRAIN / CHARACTER / DEGRAIN / MIX
- SVG-кнобы с arc-отображением, bipolar режим для Detune
- Горизонтальные слайдеры Lo Cut / Hi Cut
- Segmented selector для Grain Shape
- Sync кнопки + combo-box дропдауны (открываются вверх)
- Freeze кнопка в хедере с pulse-анимацией
- Всё связано с APVTS через WebSliderRelay / WebToggleButtonRelay / WebComboBoxRelay

---

## Раунд 1 — Исправления после первого DAW-тестирования

**Баги из .continue-here.md:**

### 1. delay_time не имел слышимого эффекта
**Причина:** грейны читали буфер относительно текущей позиции записи (writePos), а не фиксированного снимка.
**Решение:** Введена архитектура fixed-snapshot. При спавне грейна вычисляется `startReadPos = writePos - delaySamples` (абсолютная позиция в буфере). Грейны читают от этой точки вперёд/назад на протяжении своей длины.
- Добавлен `readBufferHermiteAbs()` — чтение из абсолютной позиции буфера
- `spawnGrainAdvanced()` принимает `int startReadPos` как первый параметр

### 2. Feedback не слышен, самоосцилляция отсутствует
**Решение:** Прямая петля обратной связи в стиле Valhalla Delay: буфер → softClip → DC block → фильтры → gain → запись обратно в буфер. Независимо от грейнов.

### 3. Freeze работал неправильно
**Решение:** `freezeSnapPos` обновляется каждый сэмпл когда не заморожен. При заморозке — блокируется. Все грейны читают один и тот же зафиксированный момент.

### 4. HP/LP фильтры не были слышны
**Решение:** Апгрейд с TwoPoleFilter (12 дБ/окт) до FourPoleFilter (24 дБ/окт). Добавлены отдельные wetHP/wetLP — применяются к мокрому сигналу после диффузии.

### 5. Несовпадение с темпом проекта
**Решение:** Чтение `blockStartPPQ` через `AudioPlayHead::getPosition()`. PPQ-based spawning грейнов при включённой синхронизации.

### 6. Дропдаун меню открывались вниз (выходили за экран)
**Решение:** CSS: `top: 100%` → `bottom: 100%`, `margin-top` → `margin-bottom`, тень инвертирована вверх.

### 7. Reverse/Mutation/FreqSpread были практически неслышны
**Решение:**
- Mutation: shrink до 20% длины (было 40%), питч-дрифт ±200 центов (было ±50)
- FreqSpread: scale низких частот до 6× (было 2×), высоких до 0.1× (было 0.5×)

---

## Раунд 2 — Исправления после второго DAW-тестирования

**Фидбэк пользователя:**

### 1. Dry сигнал не замораживался при нажатии Freeze
**Решение:** Dry сигнал умножается на `freezeGain`. При заморозке — fades to 0 за 10 мс.

### 2. Feedback слишком быстро затухал, самоосцилляция слишком ранняя (~80%)
**Причина:** softClip применялся ко всему сигналу включая уровни ниже ±1.0, что давало ненужное затухание.
**Решение:** Новый softClip — линейный ниже ±1.0 (без затухания), tanh только выше:
```
если x > 1.0:  return 1.0 + tanh((x - 1.0) * 0.5) * 0.4
если x < -1.0: return -1.0 - tanh((-x - 1.0) * 0.5) * 0.4
иначе: return x
```
Добавлен `outputLimiter` — мягкий лимитер на финальном выходе.

### 3. Tempo sync всё ещё расходился
**Решение:** PPQ-based spawning через `blockStartPPQ + samp * ppqPerSample`. Улучшена точность привязки к транспорту.

### 4. Добавлена ручка Output Gain
- Новый параметр `output_gain` (−24…+12 дБ)
- WebSliderRelay + WebSliderParameterAttachment
- Кноб в секции MIX под Dry/Wet

---

## Раунд 3 — Финальная архитектура и новые функции

**Фидбэк пользователя (8 пунктов):**

### 1. При вставке плагина падала громкость dry сигнала
**Причина:** Формула `dry * (1-mix) + wet * mix` при mix=50% давала −6 дБ на dry.
**Решение:** Equal-power crossfade:
```
dryGain = cos(mix * π/2)
wetGain = sin(mix * π/2)
output = dry * freezeGain * dryGain + wet * wetGain * outputGain
```
- 0% wet → dry без потерь
- 50% wet → оба на −3 дБ (воспринимается как тот же уровень)
- 100% wet → dry = 0, только мокрый

### 2. Soft clipper в конце цепи (без UI)
`outputLimiter` применяется к финальному выходу:
```
outputLimiter(x) = tanh(x * 0.8) / 0.8
```
Защищает от клиппинга при самоосцилляции.

### 3. Freq Spread давал щелчки
**Решение:** Время сглаживания BandEnergy изменено со 100 мс до 300 мс. Более медленная реакция на изменение частотного баланса — без резких скачков размера грейнов.

### 4. Feedback + Freeze сами выключались со временем
**Причина:** softClip в предыдущей версии всё равно давал небольшую деградацию при длительной циркуляции.
**Решение (архитектурный):** Разделение на два буфера:
- `delayBuffer` — только входной сигнал, никакой обратной связи
- `wetDelayBuffer` — выход грейнов + feedback-эхо
Feedback читает из `wetDelayBuffer` → scale → softClip → DC block → фильтры → записывает обратно. Петля полностью изолирована от dry пути.

### 5. Delay перенесён ПОСЛЕ грейнов + кнопка ON/OFF
**Суть:** "Delay" — это не задержка входного сигнала, а задержанная копия выхода грейн-движка с обратной связью.

**Новая цепь сигнала:**
```
Input → delayBuffer (только dry * freezeGain)
             ↓
         Grain Engine (читает delayBuffer по fixed-snapshot)
             ↓
        Grain Output (wetL/wetR)
             ↓
    Diffusion → HP/LP фильтры
             ↓
         wetDelayBuffer
         ↑             ↓
    Feedback ←── read(writePos - delaySamples)
             ↓
    Output: dry*dryGain + (grain + delayEcho)*wetGain*outputGain
             ↓
        outputLimiter
```

Кнопка **Delay ON** в хедере секции DELAY. При OFF: грейны слышны напрямую, без эхо и feedback.

### 6. Самоосцилляция только после 100%
Цепь feedback: `wetDelayBuffer[readPos] * feedbackGain → softClip → ...`
- feedbackGain < 1.0: сигнал затухает
- feedbackGain = 1.0 (100%): сигнал устойчив бесконечно
- feedbackGain > 1.0 (>100%): рост, стабилизируется softClip-ом (самоосцилляция)
Порог точно на 100% благодаря линейному softClip ниже ±1.0.

### 7. Кнопка Grain ON/OFF
Кнопка **Grain ON** в хедере секции GRAIN.
- При OFF: грейны не рождаются, существующие эхо в wetDelayBuffer продолжают затухать.
- При грейн ON + delay ON: полная цепь.
- При грейн OFF + delay ON: только хвост от предыдущих грейнов с feedback.

### 8. Синхронизация ломала real-time работу без транспорта
**Причина:** PPQ-based spawning работал даже при остановленном транспорте (`blockStartPPQ = 0` постоянно → грейны спаунились каждый блок).
**Решение:** Добавлен флаг `isTransportPlaying` из `posInfo->getIsPlaying()`. PPQ-спаун активен только при `isTransportPlaying = true`. Free-running режим работает всегда.

### Дополнительно в Раунде 3:

**Mutation переработан:**
Убрана зависимость от passCountBuffer. Mutation теперь полностью случаен на каждый грейн:
- Размер: случайное отклонение ±50% при 100% Mutation
- Питч: случайный дрифт ±200 центов при 100% Mutation
- Поле `grain.sizeScale` сохраняется в структуре для компенсации энергии (короткие грейны усиливаются пропорционально √(1/sizeScale))

**freezeSnapPos скорректирован:**
`freezeSnapPos = (writePos - grainLenSamples + kBufferSize) & kBufferMask`
Грейны смотрят на окно длиной grain_size в прошлом, а не на delay_time в прошлом — это правильная точка "заморозки текущего момента".

---

## Итоговый список параметров (21 параметр)

| ID | Тип | Диапазон | Дефолт |
|---|---|---|---|
| delay_time | float | 1–2000 мс | 250 мс |
| delay_sync_toggle | bool | — | false |
| delay_sync_division | choice | 18 делений | 1/4 |
| delay_on | bool | — | true |
| grain_size | float | 5–500 мс | 80 мс |
| grain_rate | float | 0.1–100 Гц | 10 Гц |
| grain_rate_sync_toggle | bool | — | false |
| grain_rate_sync_division | choice | 18 делений | 1/8 |
| grain_shape | choice | 4 формы | Sine |
| grain_on | bool | — | true |
| feedback | float | 0–120% | 40% |
| diffusion | float | 0–100% | 30% |
| stereo_spread | float | 0–100% | 50% |
| detune | float | ±50 ct | 0 |
| low_cut | float | 20–5000 Гц | 20 Гц |
| high_cut | float | 200–20000 Гц | 20000 Гц |
| reverse_mix | float | 0–100% | 0% |
| grain_mutation | float | 0–100% | 0% |
| freq_spread | float | 0–100% | 0% |
| freeze | bool | — | false |
| dry_wet | float | 0–100% | 50% |
| output_gain | float | −24…+12 дБ | 0 дБ |

---

## Финальная архитектура DSP (краткая)

```
Input
  │
  ├─── delayBuffer[ch][writePos] = dryL * freezeGain
  │           (кольцевой буфер, 2^20 сэмплов, только dry)
  │
  │    Grain Engine (64 грейна):
  │    ├── fixed-snapshot: startReadPos = freezeSnapPos (заморожен или следует за write)
  │    ├── Hermite interpolation из delayBuffer
  │    ├── Hann/Tri/Sqr/Ramp огибающая
  │    ├── Стерео pan + detune (Knuth hash)
  │    ├── Reverse (случайный по hash)
  │    └── Mutation (случайный size + pitch)
  │
  │    Diffusion (4× Schroeder Allpass / канал)
  │    wetHP / wetLP (FourPoleFilter 24 дБ/окт)
  │
  ├─── wetDelayBuffer:
  │    read(writePos - delaySamples) → delayOut
  │    feedback = delayOut * feedbackGain
  │    → softClip → DCBlock → feedbackHP → feedbackLP
  │    wetDelayBuffer[writePos] = grainOut + feedback
  │
  └─── Output:
       dryGain = cos(mix * π/2)
       wetGain = sin(mix * π/2)
       out = dry * freezeGain * dryGain
           + (grain + delayEcho) * wetGain * outputGain
       → outputLimiter(tanh)
```

---

## Раунд 4 — Исправление формулы отображения значений

### 1. Неправильная формула в JS для параметров со скьюом
**Причина:** `normToValue()` использовал натуральный логарифм (`log: true`), но JUCE `NormalisableRange` использует степенную функцию со скьюом (`skewFactor`):
- `value = min + (max - min) * norm^(1/skewFactor)`

Параметры с `skewFactor = 0.3`: `delay_time`, `grain_size`, `grain_rate`
Параметры с `skewFactor = 0.25`: `low_cut`, `high_cut`

При norm=0.5 для delay_time логарифм давал ≈45 мс, правильное значение ≈199 мс.

**Решение:**
- `paramDisplayInfo`: заменено `log: true/false` на `skew: 0.3` / `skew: 0.25`
- `normToValue`: заменён log на `min + (max-min) * norm^(1/skew)` (точное соответствие JUCE API)
- `valueToNorm`: аналогично исправлено на `pow(linear, skew)`

### 2. Двойной пробел в отображении output_gain
**Причина:** `unit: ' dB'` (пробел + 'dB') + форматирование `' ' + unit` → `'-12  dB'` (два пробела)
**Решение:**
- `unit: 'dB'` (без ведущего пробела)
- Добавлен кейс `'dB'` в `formatVal`: `(val > 0 ? '+' : '') + (round/10) + ' dB'` — показывает знак для положительных значений (+6.0 dB)

### 3. Результат
- pluginval strictness 5: **SUCCESS**
- VST3, AU: компилируются чисто

---

## Раунд 5 — Исправление выпаданий звука и цифровых щелчков

### 1. BPM Multiplicative SmoothedValue → NaN (КРИТИЧНО → выпадания звука)
**Причина:** `juce::SmoothedValue<Multiplicative>` требует цель > 0. При остановке DAW некоторые хосты передают BPM = 0. Тогда `setTargetValue(0)` → внутренний множитель `pow(0/120, 1/N) = 0` → на следующем блоке `pow(target/0, 1/N) = NaN`. NaN попадает в `delaySamples` и вызывает `static_cast<int>(NaN)` — undefined behavior, затем чтение мусора из буфера → выпадение звука.
**Решение:**
- `smoothedBPM.setTargetValue(jmax(20.0f, bpm))` — цель никогда не ниже 20 BPM
- Проверка: `if (!(bpm >= 20.0f)) bpm = 120.0f` — ловит NaN (NaN >= 20 == false)

### 2. delaySamples может превысить kBufferSize при BPM sync на медленном темпе
**Причина:** При BPM sync с 1/1D на 20 BPM: `60000/20 * 6 = 18000ms → 1728000 сэмплов > kBufferSize = 1048576`. `static_cast<int>(1728000)` корректен, но `(writePos - 1728000 + kBufferSize) & kBufferMask` читает не тот момент в прошлом.
**Решение:** `delaySamples = jmin(rawDelaySamples, float(kBufferSize - 4))`

### 3. Voice stealing: кража грейна с ненулевой амплитудой → щелчок
**Причина:** Старая логика крала грейн с наибольшей phase (ближе к концу). Для Square window грейн может быть на полной амплитуде (1.0) до последних 5% длины. Внезапное обнуление → щелчок.
**Решение:** Кража грейна с НАИМЕНЬШЕЙ текущей амплитудой огибающей (вычисляется через applyWindow). Минимальная амплитуда = минимальный щелчок. Применено в spawnGrain и spawnGrainAdvanced.

### 4. NaN accumulation в feedback буфере
**Причина:** NaN, попавший в `wetDelayBuffer`, циркулирует бесконечно через feedback loop. DAW обнаруживает NaN → глушит канал или выдаёт silence.
**Решение:** Перед записью в `wetDelayBuffer`: `if (!isfinite(val)) val = 0.0f`

### 5. outputLimiter позволял выход до ±1.25 → DAW-side hard clipping
**Причина:** `tanh(x * 0.8) / 0.8` → max = 1/0.8 = 1.25 при x→∞. Многие DAW-ы hard-клипают на ±1.0, что вносит высокочастотные гармоники → звонкие щелчки.
**Решение:** После tanh добавлен `jlimit(-1.0f, 1.0f, out)` + финальный `isfinite` guard.

### 6. Diffusion без смуфинга → резкий скачок коэффициента allpass → щелчок
**Причина:** Коэффициент allpass = `diffusion * 0.75`. Резкое изменение knob между блоками → скачок в allpass output: `C_new * (x - C_old * delayed) ≠ C_old * (x - C_old * delayed)`.
**Решение:** `smoothedDiffusion` (SmoothedValue Linear, 20мс) — добавлен в .h, init в prepareToPlay, target set per-block, getNextValue per-sample.

### Результат
- pluginval strictness 5: **SUCCESS** ✓
- VST3 + AU: компилируются без ошибок ✓
- Переустановлено + кеши очищены ✓

---

*Последнее обновление: Раунд 5 завершён*
