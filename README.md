**[EN]**

# Dock Calendar

A lightweight, native Windows utility that displays a popup calendar directly under your dock/taskbar button, instead of opening the system notification panel.

![Light and Dark themes](themes.png)


![Calendar Views](preview.png)

## 🎯 Problem Solved

When using modern docks like **PowerToys Dock**, clicking on the clock/date widget often opens the Windows Notification Center sidebar. This is inconvenient if you just want to quickly check or select a date.

**Dock Calendar** solves this by showing a clean, native-style calendar popup right under the button you clicked, similar to how system clocks work in some Linux desktop environments.

## ✨ Features

- **Native & Lightweight**: Built with pure Win32 API + GDI+. ~245 KB executable. No .NET Framework, no Visual C++ Runtime required.
- **Windows 10 & 11 Support**: Fully compatible with both OS versions (x64).
- **Auto Theme Detection**: Automatically follows system light/dark theme and updates in real-time without restart.
- **Modern UI**: Rounded corners and shadows on Windows 11, proper DPI scaling, multi-monitor support.
- **Localized**: Month names, day names, and first day of the week follow your system locale (Russian, English, etc.).
- **Smart Positioning**: Appears centered under the cursor/button. If there’s no space below, it appears above.
- **Instant Toggle**: After first launch, the process stays in the system tray for instant open/close on subsequent clicks.
- **Keyboard Navigation**: Full keyboard support for accessibility and power users.

## 🚀 How to Use with PowerToys Dock (or similar)

1. **Place the executable** in a permanent location, e.g., `C:\Tools\DockCalendar\DockCalendar.exe`.
   > ⚠️ Do not run directly from Downloads or archive folders.
2. **Add to Dock**:
   - Attach `DockCalendar.exe` as an icon in your dock, OR
   - Create a `.lnk` shortcut to it and attach the shortcut.
   - The calendar icon is already embedded in the exe.
3. **Click the button**:
   - First click: Opens the calendar under the button.
   - Second click: Closes the calendar.
   - Click outside or press `Esc`: Closes the calendar.
4. **Background Process**: After the first launch, the app stays in the system tray for instant response. To fully exit: right-click tray icon → "Exit" or run `DockCalendar.exe /exit`.

## 📅 Calendar Controls

| Action | Result |
|--------|--------|
| **Arrows ▲ / ▼** in header | Navigate month/year/decade (depending on view) |
| **Mouse Wheel** | Same as arrows |
| **Click "November 2026"** | Switch to Year view |
| **Click "2026"** | Switch to Decade view |
| **Click a month** (in Year view) | Return to that month |
| **Click a year** (in Decade view) | Go to that year |
| **Click a day** (including gray days from adjacent months) | Select day; gray days switch to that month |
| **"Today" button** | Return to current month and highlight today |

### Keyboard Shortcuts

- `Esc` — Close window
- `← → ↑ ↓` — Navigate days (month view) or periods
- `PgUp / PgDn` — Navigate periods
- `Home` — Jump to today
- `Enter` — Go up one level (Month → Year → Decade)

### Visual Indicators

- **Today**: Highlighted with a colored circle
- **Selected day**: Highlighted with a ring
- **Weekends & adjacent month days**: Dimmed for clarity

## 🎨 Theme Support

The app automatically follows your Windows theme:

- **Registry Key**: `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize` → `AppsUseLightTheme`
- **Live Update**: Listens to `WM_SETTINGCHANGE` (`ImmersiveColorSet`) and repaints instantly.
- **Light Palette**: Background `#F3F3F3`, Text `#1B1B1B`, Accent `#0078D4`
- **Dark Palette**: Background `#202020`, Text `#FFFFFF`, Accent `#4CC2FF`
- **Windows 11 DWM Border**: Also adapts to light/dark mode.

## 🔧 Command-Line Arguments

| Argument | Purpose |
|----------|---------|
| `/exit` | Terminate the running instance |
| `/noautohide` | Disable auto-hide on focus loss (for debugging) |
| `/at=X,Y` | Show window at specific screen coordinates (for debugging) |

## 🛠️ Build from Source

The pre-built executable is included. To rebuild:

### Windows (MinGW-w64)
cd src
build.bat

**[RU]**

# Dock Calendar — всплывающий календарь у кнопки панели

Небольшое нативное приложение (**Win32 + GDI+**, ~245 КБ, **без .NET и без
Visual C++ Runtime**), которое решает проблему панелей вроде **PowerToys Dock**:
клик по виджету часов/даты открывает системную панель уведомлений *сбоку*.
Это приложение вместо неё показывает **календарь прямо под кнопкой, на которую
вы нажали** — как всплывающее окно, а не боковую панель.

Поддерживает **Windows 10 и Windows 11**, x64. **Светлая и тёмная тема** в стиле
Windows 11 — выбирается **автоматически вслед за системной** и переключается на
лету, без перезапуска. Скруглённые углы и тень на Windows 11, корректная работа с
масштабом (DPI) и несколькими мониторами. Язык и первый день недели берутся из
локали пользователя (русский, английский и др.).

![Светлая и тёмная тема — автоматически из системы](themes.png)

![Три вида календаря: месяц, год, десятилетие](preview.png)

---

## Как подключить к панели (PowerToys Dock и т.п.)

1. Положите `DockCalendar.exe` в постоянную папку, например
   `C:\Tools\DockCalendar\DockCalendar.exe` (не запускайте прямо из архива/загрузок).
2. В панели прикрепите этот файл как значок **`.exe`** (или создайте `.lnk` на
   него и прикрепите ярлык). Иконка календаря уже «вшита» в exe.
3. **Клик по кнопке** → календарь появляется **рядом с курсором, под панелью**
   (по центру кнопки). Если снизу не хватает места — окно появится над курсором.
4. **Повторный клик по той же кнопке** → календарь закрывается (как системные
   часы Windows).
5. **Клик мимо окна** или **Esc** → окно скрывается.

> После первого запуска процесс остаётся работать в фоне (иконка в системном
> трее), поэтому каждый следующий клик открывает/закрывает календарь **мгновенно**,
> без задержки на запуск. Полностью закрыть программу: правый клик по иконке в
> трее → **«Выход»** (или `DockCalendar.exe /exit`).

---

## Управление календарём

| Действие | Результат |
|---|---|
| Стрелки **▲ / ▼** справа в заголовке | листать месяц / год / десятилетие (в зависимости от вида) |
| **Колесо мыши** | то же — листать периоды |
| Клик по заголовку **«Ноябрь 2026»** | перейти к виду **года** |
| Клик по заголовку **«2026»** | перейти к виду **десятилетия** |
| Клик по месяцу в виде года | вернуться к этому месяцу |
| Клик по году в виде десятилетия | перейти к этому году |
| Клик по дню (в т.ч. серому соседнего месяца) | выбрать день; соседний месяц — перейти к нему |
| Кнопка **«Сегодня»** | вернуться к текущему месяцу и сегодняшнему дню |

**Клавиатура:** `Esc` — закрыть; `← → ↑ ↓` — выбрать день (в виде месяца) или
листать период; `PgUp / PgDn` — листать период; `Home` — сегодня; `Enter` —
подняться на уровень выше (месяц → год → десятилетие).

Сегодняшний день выделен цветным кружком, выбранный — кольцом, выходные и дни
соседнего месяца приглушены.

---

## Тема оформления (светлая / тёмная)

Приложение **само следует теме системы** — ничего настраивать не нужно:

- Тема читается из реестра:
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize` →
  `AppsUseLightTheme` (`1` = светлая, `0` = тёмная). Это тот самый переключатель
  **Параметры → Персонализация → Цвета → Выбор режима**.
- При смене темы система рассылает сообщение `WM_SETTINGCHANGE`
  (`ImmersiveColorSet`) — открытое окно **перекрашивается сразу**, без закрытия.
  Если тема сменилась, пока календарь был скрыт, она применится при следующем
  открытии.
- Светлая палитра: фон `#F3F3F3`, текст `#1B1B1B`, акцент `#0078D4` (белый текст
  на кружке «сегодня»). Тёмная: фон `#202020`, текст `#FFFFFF`, акцент `#4CC2FF`.
  На Windows 11 рамка DWM тоже переводится в соответствующий режим.

---

## Ключи запуска (необязательные)

| Ключ | Назначение |
|---|---|
| `/exit` | завершить уже работающий экземпляр |
| `/noautohide` | не скрывать окно при потере фокуса (отладка) |
| `/at=X,Y` | показать окно в заданной точке экрана (отладка) |

---

## Сборка из исходников

Готовый exe уже лежит рядом. Если нужно пересобрать:

**Windows (MinGW-w64 в `PATH`, например MSYS2 / winlibs):**
```bat
cd src
build.bat
```

**Linux (кросс-компиляция, пакет `g++-mingw-w64-x86-64`):**
```sh
cd src
./build.sh
```

Иконка (`app.ico`) уже включена в `src`. При желании её можно перегенерировать:
`python3 make_icon.py` (нужен Pillow).

### Состав исходников
- `main.cpp` — всё приложение (окно, отрисовка GDI+, навигация, локаль, тема
  системы, трей, единственный экземпляр, позиционирование у курсора, DPI).
- `resource.rc` / `app.manifest` / `app.ico` — иконка, версия, манифест
  (DPI Per-Monitor V2, уровень `asInvoker`).

### Как это работает (кратко)
- Окно — `WS_POPUP` без рамки, поверх всех (`WS_EX_TOPMOST`), не показывается в
  Alt-Tab и на панели задач (`WS_EX_TOOLWINDOW`). Тень даёт DWM.
- При показе берётся позиция курсора (`GetCursorPos`) и монитор под ним
  (`MonitorFromPoint`); окно центрируется по горизонтали кнопки и ставится чуть
  ниже курсора, с зажимом в границы экрана.
- Единственный экземпляр через именованный мьютекс: повторный запуск находит
  окно первого и шлёт ему сообщение «переключить» (открыть/закрыть).
- Тема — из `AppsUseLightTheme` (реестр), живое обновление по `WM_SETTINGCHANGE`
  (`ImmersiveColorSet`); палитры светлая/тёмная в `SetPalette()`.
- Арифметика дат — через порядковый день (алгоритм Ховарда Хиннанта), без
  переполнений `SYSTEMTIME`; имена месяцев/дней — из `GetLocaleInfoEx`, порядок
  дней недели самовыравнивается под локаль.

---

## Если что-то не так

- **Окно не у кнопки, а в углу экрана** — панель запустила exe без передачи
  позиции курсора; приложение ориентируется на реальное положение курсора в
  момент клика. Убедитесь, что кликаете именно по значку (курсор над кнопкой).
- **Антивирус/SmartScreen ругается на неподписанный exe** — это нормально для
  самосборного файла без цифровой подписи: «Подробнее» → «Выполнить в любом
  случае», либо добавьте папку в исключения.
- **Хочется другой акцент / оттенки** — обе палитры (светлая и тёмная) заданы в
  функции `SetPalette()` в `main.cpp`, пересоберите под себя. Принудительно
  зафиксировать одну тему можно, заменив вызов `SetPalette(SystemIsLight())` на
  `SetPalette(true)` (светлая) или `SetPalette(false)` (тёмная).
