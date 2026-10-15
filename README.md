# Русификатор Project Silverfish

Неофициальный русский перевод игры [Project Silverfish](https://store.steampowered.com/app/2941710/) от Siris Pendrake.

**Версия игры:** Beta 0.4.11

**[Скачать последнюю версию](../../releases/latest)**

## Установка

1. Скачайте архив из раздела [Releases](../../releases/latest) и распакуйте его.
2. Положите файл `SilverFish-Windows_RU_P.pak` в папку игры:
   `Project Silverfish\SilverFish\Content\Paks\`
3. Запустите игру. Язык переключать не нужно, перевод включится сам.

Если раньше стоял другой русификатор, перед установкой удалите папку
`Project Silverfish\SilverFish\Content\Localization` (если она есть).

**Удаление:** удалите файл `SilverFish-Windows_RU_P.pak`. Оригинальные файлы игры русификатор не меняет.

## Что переведено

- меню, настройки, создание персонажа, режимы сложности;
- предметы, оружие, броня, патроны, артефакты, снаряжение;
- диалоги, квесты и контракты, журнал, фракции, торговля;
- документы, книги, записки, инфокассеты, терминалы;
- сообщения о смерти, подсказки обучения, пассивные атрибуты.

В игровые шрифты добавлена кириллица в похожем стиле (латиница не тронута).

**Остаётся на английском:** экран концовки, часть надписей в терминалах, названия клавиш в настройках управления, некоторые имена NPC на картах и надписи на табличках в мире. Игра хранит этот текст так, что его нельзя перевести стандартным способом.

## О переводе

Перевод выполнен с помощью ИИ и сейчас на стадии проверки в игре. Если нашли ошибку, создайте [Issue](../../issues) со скриншотом или пришлите правку.

### Как поправить перевод

- `source/catalog.json` - все английские фразы игры с номерами (`id`).
- `translation/bNN.txt` - переводы: строка `### номер`, ниже русский текст.
- `translation/glossary.txt` - словарь терминов (фракции, места).

Найдите английскую фразу в `catalog.json`, возьмите её `id`, исправьте текст под `### id` в нужном файле `translation/bNN.txt`.

### Сборка `.pak` (для тех, кто правит перевод)

Нужен Python 3:

```
python tools/build.py SilverFish-Windows_RU_P.pak  ПРЕДЫДУЩИЙ_РЕЛИЗ.pak
```

Второй аргумент - прошлый релизный `.pak`, из него берутся шрифты с кириллицей.

## Благодарности

- Игра: **Siris Pendrake** - [itch.io](https://siris-pendrake.itch.io/project-silverfish), [Steam](https://store.steampowered.com/app/2941710/)
- Кириллица в шрифтах взята из открытых шрифтов (SIL Open Font License): Neucha, Tektur, Oswald, JetBrains Mono, IBM Plex Mono, PT Mono.

Это фанатский проект, он не связан с разработчиком. Все права на игру принадлежат её автору.

---

# Project Silverfish - Russian translation (EN)

Unofficial Russian translation for **Project Silverfish** by Siris Pendrake. Game version: **Beta 0.4.11**.

**Install:** put `SilverFish-Windows_RU_P.pak` into `Project Silverfish/SilverFish/Content/Paks/` and start the game. **Uninstall:** delete the file. No original game files are modified.

Covers UI, items, weapons, quests, dialogue, documents and more; Cyrillic glyphs were added to the game fonts. Translated with AI assistance and checked in-game - corrections are welcome via Issues or pull requests. Fan project, not affiliated with the developer.
