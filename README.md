# Minecraft Installer

Установщик Minecraft Bedrock (UWP) для Windows 10/11 x64 в стиле
[1.16.100-installer](https://github.com/qcountel/1.16.100-installer): wxWidgets, шрифт Monocraft,
вкладки, «каменные» кнопки и полосы прогресса.

По умолчанию ставится **1.16.100.4**; в выборе версии доступны и другие **релизные** сборки.

## Что делает

Кнопка **СКАЧАТЬ** проходит 4 шага:

1. **Загрузка** — получает прямую ссылку на `.appx` через Windows Update (как
   [MCLauncher](https://github.com/MCMrARM/mc-w10-version-launcher): SOAP-запрос
   `GetExtendedUpdateInfo2` по UpdateID из базы версий) и скачивает пакет. Для 1.16.100.4
   дополнительно проверяется размер и SHA-256.
2. **Распаковка** — в `<папка с exe>\imported_versions\Minecraft_1.16.100.04_x64`
   (файл `AppxSignature.p7x` пропускается, чтобы пакет можно было зарегистрировать в режиме разработчика).
3. **Патч Xbox Live** — встроенный KeyPatcher заменяет публичный ключ в `Minecraft.Windows.exe`,
   чтобы работал вход в Xbox Live (можно отключить в настройках).
4. **Регистрация** — регистрирует папку как пакет в Windows (`RegisterPackageAsync`, Development Mode).

После этого кнопка становится **ИГРАТЬ**.

## Требования

- Windows 10/11 x64.
- Включён **режим разработчика** (Параметры → Конфиденциальность и защита → Для разработчиков).
  Программа проверяет это и открывает нужную страницу настроек.
- Запуск от имени администратора (запрашивается автоматически).
- Лицензия Minecraft на учётной записи Microsoft — установщик не обходит проверку лицензии.

## Важно

- Установленный из Microsoft Store Minecraft (`Microsoft.MinecraftUWP`) будет **заменён**:
  в Windows может быть только одна версия этого пакета. Перед удалением миры
  (`LocalState\games\com.mojang`) копируются в `<папка с exe>\backups\com.mojang_<дата>`
  и восстанавливаются после регистрации.
- Перед установкой игру нужно закрыть.
- Список версий кэшируется в `imported_versions\versions_cache.json`.

## Сборка

Нужны Visual Studio 2022 (C++ desktop, Windows SDK) и CMake ≥ 3.20.

```bat
build.bat
```

или вручную:

```bat
cmake -B build -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --config Release
```

wxWidgets 3.2.6 и miniz 3.0.2 скачиваются автоматически через FetchContent.

## Благодарности и лицензия

- [MCLauncher](https://github.com/MCMrARM/mc-w10-version-launcher) — логика загрузки и регистрации, база версий.
- [KeyPatcher](https://github.com/ambiennt/KeyPatcher) (ambiennt) — патч ключа Xbox Live, GPLv3.
- [Monocraft](https://github.com/IdreesInc/Monocraft) — шрифт.
- [miniz](https://github.com/richgel999/miniz), [wxWidgets](https://www.wxwidgets.org/).

Проект распространяется под лицензией **GPLv3** (см. `LICENSE`).
Minecraft — товарный знак Mojang/Microsoft; проект не связан с ними.
