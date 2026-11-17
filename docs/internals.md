# Внутренности SOVIET64.exe 1.1.1.13 (SizeOfImage 0xa9d000)

Все адреса — RVA от базы exe. Получены дизассемблером и журналом собственных вызовов игры (перехваты в `engine.cpp`).

## Движок

- `C3DDLL64.dll` экспортирует около 2200 именованных методов C++ (`C3D_CAMERA`, `C3D_TERRAIN`, `C3D_INPUT`,
  `C3D_BUILDING`, `C3D_GUI`); exe их импортирует, мод перехватывает через таблицу импорта exe.
- Логика игры (постройка, экономика) — внутри exe без символов.
- Вызовы постройки должны идти в потоке обновления игры: мод выполняет их внутри перехвата
  `CommonInterfaceGetButtonPress("TOOL_CANCEL")`.

## Состояние игры: глобальный объект G = база + 0x9d4f10

| Смещение | Что |
|---|---|
| G+0xf6c | мировая позиция курсора (vec3) |
| G+0xf94 | поворот курсора |
| G+0xd428 | указатель на текущий инструмент |
| G+0xd280 / +0xd288 | вектор инструментов, запись 0x2d0 байт, имя в начале (2356 штук; имя инструмента здания = имя ini) |
| G+0x11db0 | флаг зеркала |
| G+0x11a10 | код результата последней постройки (−1 успех, 10 неровная земля) |
| G+0x14000… | буфер копирования области |

## Функции

| RVA | Назначение | Аргументы |
|---|---|---|
| 0x2bad60 | обновление инструмента здания | (G, имя, force, поставить-если-можно, —, шаг поворота, 0, 0) |
| 0x2168f0 / 0x21b400 | вставить / скопировать область | |
| 0x21ba40 | привязка курсора для линии | (G, менеджер, тип, подтип); переписывает G+0xf6c |
| 0x4fbfd0 | начать линию | (менеджер, &pos, подтип, 0, аргумент, 0, &out) |
| 0x4fca80 | обновить линию | (менеджер, &pos, тип, подтип, 0…) |
| 0x518a10 | закрепить линию | (менеджер, 0…) — если менеджер+0x430 ≠ 0 и ≠ менеджер+0x428 |
| 0x520530 | отменить линию | (менеджер) |

Привязка (0x21ba40) рядом с подключением здания читает `[G+0xd428]+0x2b5`; при пустом инструменте — падение по адресу
0x3c7712. Перед вызовом линии мод ставит подходящий инструмент и потом возвращает прежний.

## Типы зданий и обслуживание

- Код типа здания лежит в объекте типа `+0x360` (указатель на тип — `здание+0x318`). Парсер ini — `0x10e1f0`, он
  пишет код в `+0x1e10` своей структуры: LIVING 2, SUBSTATION 0x12, TRANSFORMATOR 0x13, HEATING_ENDSTATION 0x47,
  WATER_ENDSTATION 0x5b, SEWAGE_ENDSTATION 0x5e, WATER_TREATMENT 0x61, SEWAGE_TREATMENT 0x62, TRASH_CONTAINER 0x69.
- Обход всех зданий: вектор указателей по `база+0x9e6a18` / `+0x9e6a20`.
- Проверки «здание ↔ узел» для показа сетей: `0x43e948` вызывает `0x4406d0` (электричество, 250 м),
  `0x440d90` (тепло, 380 м), `0x441340` (вода, 250 м), `0x441600` (канализация, 250 м), `0x4418c0` (мусор, 125 м).
  Расстояние — `C3DCalcDistance2d`.
- Нагрузка: мощность `здание+0x10f8`, тепловой бак `здание+0x1060` (указатели на float); списки потребителей:
  подстанция `+0x10c8`, тепловой пункт `+0x1030`. Проверка перегрузки (порог 0,9 по `0x909f04`): `0x722907`, `0x723605`.
  Отладка: файл `DistrictPlanner.dumpload` пишет в журнал по каждому типу здания эти значения (`LOAD …`).
- Ежетиковая работа узлов: `0x139a70` раздаёт по типам — вода `0x18d650`, канализация `0x18e0b0`, `0x5c` → `0x189050`.

## Загрузчик типов зданий `0x10e1f0`

- Читает `media_soviet/buildings_types/*.ini`; строка сравнивается функцией `0x84f6c0` (strcmp) с ключами, числа —
  `0x52f0` (sscanf). Рабочая структура типа лежит на стеке с `rbp+0x1ab0`; конструктор `0x10dd30(…, rdx = структура)`.
- Поля структуры (от `rbp`): код типа `+0x1e10`; `$WORKERS_NEEDED` `+0x22bc`; `$PROFESORS_NEEDED` `+0x22c0`;
  `$CITIZEN_ABLE_SERVE` `+0x22c4`; `$ENGINE_SPEED` `+0x22c8`; `$VEHICLE_LOADING_FACTOR` `+0x22d4`;
  `$VEHICLE_UNLOADING_FACTOR` `+0x22d8`; `$ELETRIC_CONSUMPTION_LOADING_FIXED` `+0x2224`; `…_UNLOADING_FIXED` `+0x2228`;
  `$ELETRIC_CONSUMPTION_LIGHTING_WORKER_FACTOR` `+0x2234` (по умолчанию 1,0); `…_ABLE_SERVE` `+0x2238` (0,2);
  `$ELETRIC_CONSUMPTION_LIVING_WORKER_FACTOR` `+0x223c` (1,0); `…_ABLE_SERVE` `+0x2240` (0,2); итог тепла `+0x2258`,
  `+0x225c`; вектор подключений `+0x1da8` (первое поле записи — код подключения); склады `+0x1e50` (записи 0xE0,
  ёмкость `+0x8c`, код ресурса `+0x90`).
- Ещё поля: `$ENGINE_SPEED` `+0x22c8`; `$WATER_WITHOUT_WORKING_FACTOR` `+0x224c`;
  `$WATER_CONSUMPTION_LIVING_WORKER_FACTOR` `+0x2250` (по умолчанию 1,0); `$ELETRIC_WITHOUT_WORKING_FACTOR` `+0x2244`;
  `$ELETRIC_WITHOUT_LIGHTING_FACTOR` `+0x2248`; флаг «без воды» `+0x22e0`.
- Склады здания: вектор `здание+0x970`, запись 0xE0 байт, объём `+0x8c` (float), код ресурса `+0x90` (int). Коды:
  0 товары, 3 топливо, 5 продукты, 6 (складской), 7 жители, 9 электричество, 14 тепло, 15 вода, 16 канализация, 17 мусор.
- Ресурсы: определяются в коде `0x2a1d50`; поиск по имени `0x2aa7b0(G, имя)` (списки в `G+0xc3b8`, `G+0xc3c0`); имя —
  строка в начале объекта. Поле `ресурс + код_транспорта × 32 + 0xd8 / +0xdc` — электричество за склад этого ресурса
  (0,05 у нефти, топлива, битума, жидких удобрений, воды, использованной воды). У записи склада здания список ресурсов —
  вектор 16-байтных элементов в начале записи (элемент[0] — объект ресурса).
- Глобальные значения: `exe+0x9d5d84` — темнота суток (float, 1 ночью, 0 днём; читают шейдеры окон и теней);
  `exe+0x9d54c0`…`+0x9d54f0` — правила новой игры, копируются из `+0x9d5c44`…`+0x9d5c70` в `0x28e1ff`…`0x28e290`.
- Скриптовый интерпретатор Pyrois: `0x59f180` (описание команд — `media_soviet/scripts/SOVIETInstructions.txt`).
- Расчёт воды `0x11a46b` (код 15), канализации `0x11a725` (код 16), мусора `0x11a9cb` (код 17); глобальный флаг
  `exe+0x9d54c4` включает воду и канализацию.
- Расчёт автоскладов: тепло `0x1196ea`…`0x119984` (запись склада тепла: код 0xE, ёмкость `+0x181c`), электричество
  `0x119a00`…`0x11a2f0` (код 9, ёмкость `+0x149c`). Глобальные флаги расчёта: `exe+0x9d54d4` (удваивает электричество
  жилья), `exe+0x9d5d84` (включает часть «освещение»), `exe+0x9d54c0`/`+0x9d54d0` (включают расчёт тепла).
- У построенного здания: склады — вектор `+0x970`; `+0x10f8` → ёмкость склада электричества; `+0x1060` → склада тепла;
  `+0x1058` — соседнее поле склада тепла.

### Коды типов (`+0x1e10` в загрузчике, `тип+0x360` у здания)

- `0x2` TYPE_LIVING
- `0x3` TYPE_MINE_GRAVEL
- `0x5` TYPE_STORAGE
- `0x6` TYPE_FACTORY
- `0x7` TYPE_MINE_OIL
- `0x8` TYPE_FIELD
- `0x9` TYPE_FARM
- `0xb` TYPE_ENGINE
- `0xc` TYPE_CONSTRUCTION_OFFICE
- `0xe` TYPE_ROADDEPO
- `0xf` TYPE_RAILDEPO
- `0x11` TYPE_POWERPLANT
- `0x12` TYPE_SUBSTATION
- `0x13` TYPE_TRANSFORMATOR
- `0x14` TYPE_CUSTOMHOUSE
- `0x15` TYPE_CHURCH
- `0x16` TYPE_KINO
- `0x17` TYPE_PUB
- `0x18` TYPE_SPORT
- `0x19` TYPE_HOSPITAL
- `0x1a` TYPE_FIRESTATION
- `0x1b` TYPE_UNIVERSITY
- `0x1c` TYPE_CONSTRUCTION_OFFICE_RAIL
- `0x1f` TYPE_ELETRIC_EXPORT
- `0x20` TYPE_ELETRIC_IMPORT
- `0x21` TYPE_MONUMENT
- `0x22` TYPE_GAS_STATION
- `0x23` TYPE_RAIL_TRAFO
- `0x24` TYPE_POLLUTION_METER
- `0x25` TYPE_BROADCAST
- `0x26` TYPE_PARKING
- `0x27` TYPE_CITYHALL
- `0x28` TYPE_PRODUCTION_LINE
- `0x29` TYPE_CAR_DEALER
- `0x2a` TYPE_SHIP_DOCK
- `0x2b` TYPE_DISTRIBUTION_OFFICE
- `0x2c` TYPE_CONTAINER_FACILITY
- `0x2d` TYPE_FORKLIFT_GARAGE
- `0x2e` TYPE_AIRPLANE_GATE
- `0x2f` TYPE_AIRPLANE_PARKING
- `0x30` TYPE_AIRPLANE_TOWER
- `0x31` TYPE_GARBAGE_OFFICE
- `0x32` TYPE_HOTEL
- `0x33` TYPE_ATTRACTION
- `0x34` TYPE_DISTRIBUTION_OFFICE_RAIL
- `0x3c` TYPE_COOLING_TOWER
- `0x46` TYPE_HEATING_PLANT
- `0x47` TYPE_HEATING_ENDSTATION
- `0x48` TYPE_HEATING_SWITCH
- `0x49` TYPE_POLICE_STATION
- `0x4a` TYPE_ORPHANAGE
- `0x4b` TYPE_COURT_HOUSE
- `0x4c` TYPE_PRISON
- `0x4d` TYPE_SECRET_POLICE
- `0x50` TYPE_PEDESTRIAN_BRIDGE
- `0x5a` TYPE_WATER_PUMP
- `0x5b` TYPE_WATER_ENDSTATION
- `0x5c` TYPE_MINE_WATER
- `0x5d` TYPE_SEWAGE_PUMP
- `0x5e` TYPE_SEWAGE_ENDSTATION
- `0x5f` TYPE_SEWAGE_DISCHARGE
- `0x60` TYPE_WAITING_STATION
- `0x61` TYPE_WATER_TREATMENT
- `0x62` TYPE_SEWAGE_TREATMENT
- `0x69` TYPE_TRASH_CONTAINER
- `0x6a` TYPE_DEMOLITION_OFFICE
- `0x6b` TYPE_FOREIGN_PIPELINE_EXPORT
- `0x6c` TYPE_REPAIR_OFFICE
- `0x6d` TYPE_SCRAPYARD
- `0x12c` TYPE_TRAM_GATE

### Коды подключений (первое поле записи в векторе подключений)

- 7 `CONNECTION_ELETRIC_HIGH_OUTPUT_ONLY`
- 8 `CONNECTION_ELETRIC_LOW_OUTPUT_ONLY`
- 9 `CONNECTION_CABLEWAY`
- 11 `CONNECTION_HEATING_SMALL`
- 12 `CONNECTION_HEATING_BIG`
- 15 `CONNECTION_STEAM_OUTPUT`
- 16 `CONNECTION_BULK_OUTPUT`
- 17 `CONNECTION_FENCE`
- 19 `CONNECTION_WATERPIPE_OUTPUT`
- 20 `CONNECTION_SEWAGE_OUTPUT`
- Остальные коды подключений задаются другим способом (не через `[rsp+0x60]`) и пока не выписаны.

## Менеджеры сетей и коды

| Сеть | Менеджер | тип | подтип | аргумент начала | инструмент |
|---|---|---|---|---|---|
| Дороги | G+0xf7a0 | −1 грунт, 0 гравий, 1 асфальт, 2 асфальт с тротуарами | 0 | −20 | `road_mud`, `road_gravel`, `road_asphalt`, `road_asphaltside` |
| Дорожки | G+0xf968 | −1 тропа, 0 гравий, 1 асфальт | 0 | −20 | `pedestrianway_mud`, `pedestrianway`, `pedestrianway_asplhalt` |
| Трубы воды и канализации | G+0xfaf0 | 20 | вода 0 / 1, канализация 5 / 6 | −20 | `waterpipe_0`, `waterpipe_1`, `sewagepipe_0`, `sewagepipe_1` |
| Теплотрасса | G+0xfa98 | 10 | 0 малая, 1 большая | −20 | `heating_pipe_small`, `heating_pipe_big` |
| Среднее напряжение | G+0xfa20 | 10 воздушная, 20 кабель | 0 | = тип | `eletric_low0`, `eletric_ulow0` |
| Высокое напряжение | G+0xfa18 | 10 воздушная, 20 кабель | 0 | = тип | `eletric_high`, `eletric_uhigh0` |

Неопознанные менеджеры: G+0xf800, 0xf858, 0xf908, 0xf9c0.

## Поворот и выбор здания

- `C3D_BUILDING::GetRotationStepFromRotation(float)` вызывается каждый кадр, пока показан призрак здания (128 шагов на оборот).
- `GetTypeIndex(char* ini)` вызывается один раз при выборе здания в меню.
- Ось `BUILDING_ROTATE` из `CommonInterfaceGetAxisValue`: 1000 единиц = 1 рад.
- Угол игры = −(угол плана), локальная ось X модели вдоль оси u плана.

## Рельеф

- `EditRaiseLower(pos, inner, outer, amount, limit, selfOpen)` — RVA 0xF8E50 в C3DDLL64: радиусы в текселях карты высот
  (2048² на 20 км ≈ 9,8 м), высоты нормированы: мир = −230 + норма × 800.

## Отладочные флаги (файлы рядом с exe)

`DistrictPlanner.selftest` — тестовый район перед камерой; `.instant` — мгновенная постройка плана; `.tool` (имя
инструмента внутри) — выбрать инструмент и писать вызовы линий в журнал; `.pathtest`, `.autobuild` — старые проверки.
Журнал: `DistrictPlanner.log`.

## Привязка линий: как её вызывает сама игра (разбор 17.11.2026)

Уточнение (v0.17.0): код у `0x2bd8bf` — не инструмент дорожек, а **автоподключение входа при постановке здания**
(внутри `0x2bad60`): игра берёт точку чуть снаружи от каждого пешеходного входа, ищет дорожку привязкой
`0x3d4a80(G, 0, r8b = 1, 0, 0, 0)` и, если нашла, сама строит короткий отвод. Обычный инструмент дорожек при ручном
протягивании вызывает `start` с `r8 = 0` (снято логом). Мод с v0.17.0 привязывает концы своих дорожек вызовом
`0x3d4a80` с `r8b = 1` (как автоподключение), а не через диспетчер; переключатель для сравнения — файл
`DistrictPlanner.snapmode` (0 — как раньше, 1 — новый способ). Поле `+0x313` и `start` с `r8b = 1` не используются.


- `0x21ba40` — только диспетчер по менеджеру, его использует вставка области. Настоящие функции привязки:
  дороги `0x3c4790`, дорожки `0x3d4a80`, трубы `0x3b18c0`, тепло `0x3b45d0`, среднее напряжение `0x3b8b20`,
  высокое `0x3b7de0`.
- Инструмент дорожек (внутри `0x2bad60`, около `0x2bd8bf`) вызывает `0x3d4a80(G, edx, r8b = 1, r9 = 0, 0, 0)`;
  диспетчер передаёт `r8b = 0`. Вероятно, именно этот флаг включает привязку к середине отрезка — проверить первым.
- Привязка кладёт найденное в `G+0xf648`, `G+0xf650`, `G+0xf680`, `G+0xf690` (объекты, к которым пристали);
  тип и подтип для `update` игра берёт из полей `+0x120` / `+0x124` найденного объекта.
- Перед `start` инструмент ставит `менеджер+0x313 = 1`, вызывает `start(mgr, &pos, r8b = 1, 0, -20, 0, &out)`,
  пишет вектор в `менеджер+0x328`, снова привязка, затем `update`, после него `менеджер+0x313 = 0`.
  Мод сейчас передаёт в `start` r8 = 0 и поле `+0x313` не трогает.
