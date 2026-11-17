# Здания игры: размеры, зоны, подключения, потребности

Сгенерировано `tools/gen_docs.py` из `media_soviet/buildings_types`. Полные данные (все точки подключений и мёртвых зон с координатами) — в `data/buildings.json`.

- **Размер** — габарит модели X×Z и высота, м (файл `.bbox`).
- **Зона** — габарит вместе со всеми точками подключений, мёртвыми зонами дорог и стоянками (X×Z, м): столько места здание реально занимает, сюда нельзя ставить соседей.
- **Потребление / производство** — на одного работника в день (т), `eletric` в строке «в секунду» — МВт.

## AIRPLANE_GATE (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `airport_road_connection` | КПП аэропорта | 15×20, в 4 | 16×20 |  |  |  |  |  | дор 1 |

## AIRPLANE_PARKING (7)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `airplaneparking_30` | Стоянка для самолетов (30 м) | 30×31, в -1 | 30×33 |  |  |  |  |  |  |
| `airplaneparking_40` | Стоянка для самолетов (40 м) | 40×41, в -1 | 40×43 |  |  |  |  |  |  |
| `airplaneparking_50` | Стоянка для самолетов (50 м) | 51×52, в -2 | 51×53 |  |  |  |  |  |  |
| `airplaneparking_50_3x` | Тройная стоянка для самолетов (50 м) | 161×51, в -2 | 161×52 |  |  |  |  |  |  |
| `airplaneparking_75` | Стоянка для самолетов (75 м) | 73×75, в -3 | 73×77 |  |  |  |  |  |  |
| `heliport` | Вертолетная площадка | 30×31, в -1 | 30×33 |  |  |  |  |  | дор 1 |
| `heliport_3x` | Тройная вертолетная площадка | 102×30, в -1 | 102×31 |  |  |  |  |  | дор 1 |

## AIRPLANE_TOWER (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `control_tower` | Диспетчерская вышка | 18×18, в 32 | 19×18 |  +20 с высш. |  |  |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.1, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 2, дор 1 |

## ATTRACTION (13)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `co_art_gallery` | Галерея искусств | 35×13, в 19 | 35×13 | 15 |  |  |  | CITIZEN_ABLE_SERVE 6, ATTRACTIVE_SCORE 0.75, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 1, дор 1 |
| `co_beach` | Пляж | 59×83, в 6 | 59×83 | 5 |  |  |  | CITIZEN_ABLE_SERVE 18, ATTRACTIVE_SCORE 4, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0, HEATING_DISABLE | пеш 2 |
| `co_chamomile` | Карусель "Ромашка" | 25×26, в 5 | 26×26 | 2 |  |  |  | CITIZEN_ABLE_SERVE 25, ATTRACTIVE_SCORE 2.8, HEATING_DISABLE | пеш 4 |
| `co_ferris_wheel` | Колесо обозрения | 24×54, в 57 | 28×54 | 2 |  |  |  | CITIZEN_ABLE_SERVE 17, ATTRACTIVE_SCORE 3.6, ELETRIC_WITHOUT_LIGHTING_FACTOR 0, HEATING_DISABLE | пеш 2 |
| `co_museum_of_republic` | Музей Республики | 48×36, в 49 | 48×36 | 15 |  |  |  | CITIZEN_ABLE_SERVE 4, ATTRACTIVE_SCORE 3.2 | пеш 1, дор 1 |
| `co_museum_snp` | Музей Национального восстания | 54×54, в 20 | 55×54 | 8 |  |  |  | CITIZEN_ABLE_SERVE 5, ATTRACTIVE_SCORE 2.4, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2 | пеш 6, дор 2 |
| `co_palace_of_communism` | Дворец коммунизма | 86×57, в 94 | 86×57 | 20 |  |  |  | CITIZEN_ABLE_SERVE 4, ATTRACTIVE_SCORE 3.8, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | дор 1 |
| `co_sight_tower` | Смотровая башня | 30×32, в 53 | 30×32 | 8 |  |  |  | CITIZEN_ABLE_SERVE 8, ATTRACTIVE_SCORE 4.1, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 4, дор 1 |
| `co_sight_tower_2` | Смотровая башня II | 30×35, в 53 | 31×35 | 10 |  |  |  | CITIZEN_ABLE_SERVE 9, ATTRACTIVE_SCORE 3.8, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 2, дор 1 |
| `co_surprise` | Карусель "Сюрприз" | 16×22, в 6 | 16×22 | 2 |  |  |  | CITIZEN_ABLE_SERVE 23, ATTRACTIVE_SCORE 3, HEATING_DISABLE | пеш 2 |
| `co_swimmingpool` | Крытый бассейн | 51×30, в 11 | 51×30 | 10 |  |  |  | CITIZEN_ABLE_SERVE 9, ATTRACTIVE_SCORE 1.8 | пеш 4, дор 1 |
| `co_swing_cage` | Автодром | 16×33, в 5 | 18×33 | 2 |  |  |  | CITIZEN_ABLE_SERVE 17, ATTRACTIVE_SCORE 3.1, ELETRIC_WITHOUT_WORKING_FACTOR 0, ELETRIC_WITHOUT_LIGHTING_FACTOR 0, HEATING_DISABLE | пеш 1 |
| `co_zoo` | Зоопарк | 46×72, в 8 | 46×72 | 15 |  |  |  | CITIZEN_ABLE_SERVE 4, ATTRACTIVE_SCORE 2.7, ELETRIC_WITHOUT_WORKING_FACTOR 0.3 | пеш 3, дор 1 |

## BROADCAST (4)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `rozhlas` | Радиостанция | 133×129, в 64 | 134×148 | 100 +50 с высш. |  |  |  |  | пеш 5, дор 2 |
| `rozhlas_v2` | Радиостанция | 133×129, в 64 | 133×129 | 100 +50 с высш. |  |  |  |  | пеш 4, дор 1 |
| `televizia` | Телестанция | 108×228, в 134 | 108×229 | 120 +70 с высш. |  |  |  |  | пеш 10, дор 2 |
| `televizia_v2` | Телестанция | 108×228, в 134 | 123×228 | 120 +70 с высш. |  |  |  |  | пеш 6, дор 1 |

## CARGO_STATION (43)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `airport_terminal_cargo` | Пассажирский терминал | 106×116, в 21 | 108×116 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1, FUEL:OIL 60 |  |  |  | пеш 3, дор 1 |
| `airport_terminal_cargo2` | Грузовой терминал | 58×95, в 11 | 58×95 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1, FUEL:OIL 60 |  |  |  | дор 1, труба 2 |
| `bulk_rail_station` | Станция погрузки/выгрузки насыпных грузов | 28×93, в 21 | 29×93 |  | CEMENT 1 |  |  |  | дор 1, жд 4 |
| `bulk_road_station` | Автостанция погрузки/выгрузки насыпных грузов | 14×54, в 18 | 14×54 |  | CEMENT 1 |  |  |  | дор 1 |
| `cableway_gravel_loading_big` | Большая станция погрузки навалочных грузов | 17×22, в 23 | 17×22 |  | GRAVEL 1 |  |  |  | дор 1, конвейер 2 |
| `cableway_gravel_loading_small` | Малая станция погрузки навалочных грузов | 16×14, в 23 | 16×15 |  | GRAVEL 1 |  |  |  | дор 1, конвейер 1 |
| `cableway_gravel_unloading_big` | Большая станция выгрузки навалочных грузов | 30×35, в 14 | 30×35 |  | GRAVEL 1 |  |  |  | дор 1, конвейер 2 |
| `cableway_gravel_unloading_small` | Малая станция выгрузки навалочных грузов | 16×20, в 13 | 16×20 |  | GRAVEL 1 |  |  |  | дор 1, конвейер 1 |
| `harbor_cargo_medium` | Средний грузовой порт | 281×175, в 36 | 283×175 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1, OIL 1, FUEL:OIL 100 |  |  |  | дор 1, труба 2 |
| `harbor_cargo_small` | Малый грузовой порт | 191×68, в 36 | 192×68 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1, OIL 1, FUEL:OIL 40 |  |  |  | дор 1, труба 1 |
| `harbor_container` | Терминал для контейнеров и техники | 335×175, в 60 | 335×175 |  | VEHICLES 150, OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1, OIL 1, FUEL:OIL 100 |  |  |  | дор 1, жд 4, труба 2 |
| `harbor_gravel_loading` | Балкерный погрузочный терминал | 285×132, в 21 | 285×132 |  | GRAVEL 1, FUEL:OIL 100 |  |  |  | дор 1, конвейер 3, труба 2 |
| `harbor_gravel_unloading` | Балкерный разгрузочный терминал | 283×132, в 36 | 283×132 |  | GRAVEL 1, FUEL:OIL 100 |  |  |  | дор 1, конвейер 2, труба 2 |
| `harbor_pumping` | Наливной терминал | 286×132, в 22 | 286×132 |  | OIL 1, FUEL:OIL 40 |  |  |  | дор 1, труба 4 |
| `heliport_cargo` | Грузовой вертодром | 30×31, в -1 | 31×31 |  | OPEN 1, GRAVEL 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1, FUEL:OIL 25 |  |  |  | дор 1, конвейер 3 |
| `heliport_cargo_3x` | Тройной грузовой вертодром | 102×30, в -1 | 102×31 |  | OPEN 1, GRAVEL 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1, FUEL:OIL 25 |  |  |  | дор 1, конвейер 2 |
| `rail_pumping_station` | Наливной терминал | 50×28, в 21 | 50×28 |  | OIL 1 |  |  |  | дор 2, жд 4, труба 4 |
| `rail_sewage_station` | Наливной терминал | 50×28, в 21 | 50×28 |  | SEWAGE 1 |  |  |  | дор 2, канал вх 2, канал вых 2, жд 4 |
| `rail_station_cargo` | Товарная железнодорожная станция | 81×39, в 14 | 83×39 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 2, жд 4 |
| `rail_station_cargo1` | Товарная железнодорожная станция | 53×24, в 14 | 55×31 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 2, жд 2 |
| `rail_station_cargo2` | Товарная железнодорожная станция | 91×57, в 14 | 92×57 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 2, жд 8 |
| `rail_station_cargo3` | Товарная железнодорожная станция | 156×38, в 14 | 156×38 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 2, жд 4 |
| `rail_station_rumble_loading` | Станция погрузки навалочных грузов | 100×35, в 33 | 100×37 |  | GRAVEL 1 |  |  |  | дор 1, жд 4, конвейер 3 |
| `rail_station_rumble_loading_v2` | Станция погрузки навалочных грузов | 100×35, в 33 | 100×37 |  | GRAVEL 1 |  |  |  | дор 1, жд 4, конвейер 3 |
| `rail_station_rumble_unloading` | Станция выгрузки навалочных грузов | 149×47, в 12 | 149×47 |  | GRAVEL 1 |  |  |  | дор 1, конвейер 4 |
| `rail_station_rumble_unloading_v2` | Станция выгрузки навалочных грузов | 149×47, в 12 | 149×47 |  | GRAVEL 1 |  |  |  | дор 1, конвейер 4 |
| `rail_water_station` | Наливной терминал | 50×28, в 21 | 50×28 |  | WATER 1 |  |  |  | дор 2, вода вх 2, вода вых 2, жд 4 |
| `road_cargo_rumble_loading` | Автостанция погрузки навалочных грузов | 13×31, в 18 | 13×32 |  | GRAVEL 1 |  |  |  | дор 1, конвейер 2 |
| `road_cargo_rumble_unloading` | Автостанция выгрузки навалочных грузов | 14×37, в 7 | 14×38 |  | GRAVEL 1 |  |  |  | дор 1, конвейер 2 |
| `road_cargo_station_big` | Грузовая автостанция | 44×46, в 6 | 45×46 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 1 |
| `road_cargo_station_small` | Грузовая автостанция | 44×30, в 6 | 44×30 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 1 |
| `road_cargostation` | Грузовая автостанция | 38×34, в 6 | 38×34 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 2 |
| `road_cargostation_deadend_1` | Грузовая автостанция | 42×25, в 6 | 44×28 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 1 |
| `road_cargostation_deadend_2` | Грузовая автостанция | 42×14, в 6 | 43×16 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 1 |
| `road_cargostation_oneway` | Грузовая автостанция | 44×32, в 6 | 46×35 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 2 |
| `road_cargostation_v2` | Грузовая автостанция | 44×38, в 6 | 45×38 |  | OPEN 1, COVERED 1, LIVESTOCK 1, CEMENT 1, COOLER 1 |  |  |  | дор 1 |
| `road_pumping_station` | Наливной терминал | 40×28, в 21 | 40×28 |  | OIL 1 |  |  |  | дор 1, труба 2 |
| `road_pumping_station_v2` | Наливной терминал | 40×28, в 21 | 40×28 |  | OIL 1 |  |  |  | дор 2, труба 2 |
| `road_pumping_station_v3` | Наливной терминал | 40×28, в 21 | 40×28 |  | OIL 1 |  |  |  | дор 2, труба 2 |
| `road_sewage_station` | Наливной терминал | 40×28, в 21 | 40×28 |  | SEWAGE 1 |  |  |  | дор 1, канал вх 1, канал вых 1 |
| `road_water_station` | Наливной терминал | 40×28, в 21 | 40×28 |  | WATER 1 |  |  |  | дор 1, вода вх 1, вода вых 1 |
| `sewage_filling_station` | Ассенизационная станция | 40×33, в 8 | 40×33 |  | SEWAGE 1 |  |  |  | дор 1, канал вх 2, канал вых 2 |
| `water_filling_station` | Водовозная станция | 37×32, в 8 | 37×32 |  | WATER 1 |  |  |  | дор 1, вода вх 2, вода вых 2 |

## CAR_DEALER (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `cardealer` | Автомагазин | 40×32, в 10 | 40×32 | 15 | VEHICLES 5 |  |  |  | пеш 2, дор 1 |

## CHURCH (5)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `civ_dedina7a` | Малая церковь | 10×7, в 16 | 10×7 | 1 +1 с высш. |  |  |  | CITIZEN_ABLE_SERVE 40, CIVIL_BUILDING, HEATING_DISABLE | пеш 1 |
| `civ_kirche1` | Малый собор | 53×18, в 82 | 54×19 | 2 +2 с высш. |  |  |  | CITIZEN_ABLE_SERVE 80, CIVIL_BUILDING, HEATING_DISABLE | пеш 3 |
| `civ_kirche2` | Средний собор | 63×38, в 87 | 64×38 | 4 +4 с высш. |  |  |  | CITIZEN_ABLE_SERVE 80, CIVIL_BUILDING, HEATING_DISABLE | пеш 4 |
| `civ_kirche3` | Большой собор | 106×44, в 102 | 107×44 | 7 +7 с высш. |  |  |  | CITIZEN_ABLE_SERVE 80, CIVIL_BUILDING, HEATING_DISABLE | пеш 4 |
| `civ_kostolik` | Церковь | 9×18, в 23 | 9×18 | 2 +2 с высш. |  |  |  | CITIZEN_ABLE_SERVE 40, CIVIL_BUILDING | пеш 1 |

## CITYHALL (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `cityhall` | Горком | 36×48, в 22 | 42×48 | 25 |  |  |  |  | пеш 6, дор 1 |

## CONSTRUCTION_OFFICE (7)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `construction_office` | Строительная контора | 46×36, в 7 | 46×36 |  |  |  |  |  | дор 1 |
| `construction_office2` | Строительная контора | 45×41, в 7 | 46×41 |  | FUEL:OIL 30 |  |  |  | дор 1 |
| `construction_office3` | Строительная контора | 53×61, в 7 | 54×61 |  | FUEL:OIL 30 |  |  |  | дор 1 |
| `construction_office4` | Строительная контора | 60×105, в 7 | 60×106 |  | FUEL:OIL 30 |  |  |  | дор 1 |
| `construction_office5` | Строительная контора | 31×35, в 7 | 31×36 |  | FUEL:OIL 30 |  |  |  | дор 1 |
| `construction_office_helicopter` | Вертолетная строительная контора | 64×56, в 13 | 64×56 |  | FUEL:OIL 60 |  |  |  | дор 1, труба 1 |
| `muddy_construction` | Строительная контора (бесплатная) | 19×24, в 3 | 26×26 |  |  |  |  | COUNT_LIMIT 15, WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |

## CONSTRUCTION_OFFICE_RAIL (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `rail_construction_office` | Железнодорожная строительная контора | 58×44, в 14 | 60×44 | 60 | SPECIAL:GRAVEL 150, OPEN 250, SPECIAL:COVERED 35 |  |  | HEATING_DISABLE | пеш 2, дор 1, вода вх 2, жд 6, конвейер 2 |
| `rail_construction_office_big` | Железнодорожная строительная контора | 55×103, в 15 | 56×104 | 170 | SPECIAL:GRAVEL 350, OPEN 500, SPECIAL:COVERED 70 |  |  | HEATING_DISABLE | пеш 3, дор 1, вода вх 2, жд 6, конвейер 3 |

## CONTAINER_FACILITY (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `container_packing` | Цех загрузки контейнеров | 59×74, в 15 | 60×74 | 60 | IMPORT:COVERED 350, EXPORT:VEHICLES 15 |  |  | HEATING_DISABLE | пеш 2, дор 1 |
| `container_unpacking` | Цех разгрузки контейнеров | 61×76, в 15 | 62×77 | 60 | EXPORT:COVERED 350, IMPORT:VEHICLES 15 |  |  | HEATING_DISABLE | пеш 2, дор 1 |

## COOLING_TOWER (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `cooling_tower` | Градирня | 96×96, в 116 | 96×96 |  |  |  |  |  | дор 1 |

## COURT_HOUSE (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `court` | Здание суда | 50×112, в 44 | 50×112 | 50 +30 с высш. |  |  |  |  | пеш 6, дор 1 |
| `court_small` | Здание районного суда | 35×34, в 12 | 35×34 | 15 +10 с высш. |  |  |  |  | пеш 3, дор 1 |

## CUSTOMHOUSE (5)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `zoll_air_soviet` | Заграница – соцстраны | 2×2, в 1 | 2×2 |  | GRAVEL 1, OPEN 1, COVERED 1, OIL 1, CEMENT 1, COOLER 1, LIVESTOCK 1, WATER 1, WASTE 1, SPECIAL:WASTE 1 |  |  |  |  |
| `zoll_air_west` | Заграница – капстраны | 2×2, в 1 | 2×2 |  | GRAVEL 1, OPEN 1, COVERED 1, OIL 1, CEMENT 1, COOLER 1, LIVESTOCK 1, WATER 1, WASTE 1, SPECIAL:WASTE 1 |  |  |  |  |
| `zoll_milhost` | Большая таможня | 114×117, в 16 | 120×117 |  | GRAVEL 1, OPEN 1, COVERED 1, OIL 1, CEMENT 1, COOLER 1, LIVESTOCK 1, WATER 1, SEWAGE 1, WASTE 1, SPECIAL:WASTE 1 |  |  |  | дор 3, жд 3 |
| `zoll_sahy` | Малая таможня | 64×30, в 14 | 64×34 |  | GRAVEL 1, OPEN 1, COVERED 1, OIL 1, CEMENT 1, COOLER 1, LIVESTOCK 1, WATER 1, SEWAGE 1, WASTE 1, SPECIAL:WASTE 1 |  |  |  | дор 1 |
| `zoll_siatre` | Средняя таможня | 45×49, в 10 | 45×50 |  | GRAVEL 1, OPEN 1, COVERED 1, OIL 1, CEMENT 1, COOLER 1, LIVESTOCK 1, WATER 1, SEWAGE 1, WASTE 1, SPECIAL:WASTE 1 |  |  |  | дор 1, жд 1 |

## DEMOLITION_OFFICE (5)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `demolition_office` | Строительная контора | 46×40, в 7 | 46×40 |  | FUEL:OIL 30 |  |  |  | дор 1 |
| `demolition_office_big` | Большая демонтажная контора | 75×67, в 28 | 75×68 |  | FUEL:OIL 40 |  |  |  | дор 1 |
| `demolition_office_helicopter` | Вертолетная демонтажная контора | 56×73, в 19 | 56×74 |  | FUEL:OIL 60 |  |  |  | дор 1, труба 1 |
| `demolition_office_small` | Малая демонтажная контора | 52×49, в 19 | 52×50 |  | FUEL:OIL 20 |  |  |  | дор 1 |
| `muddy_demolition` | Демонтажная контора (бесплатная) | 19×24, в 3 | 26×26 |  |  |  |  | COUNT_LIMIT 7, WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |

## DISTRIBUTION_OFFICE (3)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `distribution_office_medium` | Средняя база снабжения | 63×81, в 15 | 63×82 |  | FUEL:OIL 20 |  |  |  | дор 1 |
| `distribution_office_small` | Малая база снабжения | 42×50, в 11 | 42×55 |  | FUEL:OIL 20 |  |  |  | дор 1 |
| `muddy_distribution` | База снабжения (бесплатная) | 13×22, в 2 | 13×28 |  |  |  |  | COUNT_LIMIT 5, WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |

## DISTRIBUTION_OFFICE_RAIL (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `distribution_office_rail` | Железнодорожная база снабжения | 155×65, в 41 | 155×65 |  | VEHICLES 150, FUEL:OIL 50 |  |  |  | дор 1, жд 4 |

## ELETRIC_EXPORT (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `eletric_transformator_customin` | Вставка постоянного тока | 23×40, в 13 | 24×42 |  |  |  |  |  | эл ср вх 4, эл выс вх 3, эл выс вых 1 |
| `foreign_eletric_player` | Вставка постоянного тока | 23×40, в 13 | 24×43 |  |  |  |  |  | дор 1, эл ср вх 3, эл выс вх 1 |

## ELETRIC_IMPORT (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `eletric_transformator_customout` |  | 23×40, в 13 | 24×40 |  |  |  |  |  | эл ср вых 4, эл выс вых 1 |

## ENGINE (34)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `bulk_engine` | Конвейерная башня для насыпных грузов | 6×21, в 19 | 7×22 |  | CEMENT 1 |  |  |  | пеш 1, дор 1 |
| `bulk_engine_3in_e` | Конвейерная башня для насыпных грузов | 7×21, в 19 | 7×21 |  | CEMENT 1 |  |  |  | пеш 1, дор 1 |
| `bulk_engine_3in_f` | Конвейерная башня для насыпных грузов | 7×21, в 19 | 7×21 |  | CEMENT 1 |  |  |  | пеш 1, дор 1 |
| `bulk_engine_3in_g` | Конвейерная башня для насыпных грузов | 7×21, в 30 | 7×21 |  | CEMENT 1 |  |  |  | пеш 1, дор 1 |
| `bulk_engine_3in_h` | Конвейерная башня для насыпных грузов | 7×21, в 30 | 7×21 |  | CEMENT 1 |  |  |  | пеш 1, дор 1 |
| `bulk_engine_overpasse` | Конвейерная эстакада для насыпных грузов | 6×49, в 19 | 14×49 |  | CEMENT 1 |  |  |  | дор 2 |
| `bulk_engine_overpassf` | Конвейерная эстакада для насыпных грузов | 10×49, в 19 | 14×49 |  | CEMENT 1 |  |  |  | дор 2 |
| `bulk_engine_overpassg` | Конвейерная эстакада для насыпных грузов | 10×49, в 19 | 14×49 |  | CEMENT 1 |  |  |  | дор 2 |
| `bulk_enginf` | Конвейерная башня для насыпных грузов | 6×21, в 19 | 7×22 |  | CEMENT 1 |  |  |  | пеш 1, дор 1 |
| `bulk_enging` | Конвейерная башня для насыпных грузов | 6×21, в 30 | 7×22 |  | CEMENT 1 |  |  |  | пеш 1, дор 1 |
| `bulk_enginh` | Конвейерная башня для насыпных грузов | 6×21, в 30 | 7×22 |  | CEMENT 1 |  |  |  | пеш 1, дор 1 |
| `cableway_pole_heavy_1` | Тяжелая опора канатной дороги | 7×9, в 15 | 13×10 |  |  |  |  |  | пеш 1, дор 1 |
| `cableway_pole_heavy_2` | Тяжелая опора канатной дороги | 7×9, в 20 | 13×10 |  |  |  |  |  | пеш 1, дор 1 |
| `cableway_pole_heavy_3` | Тяжелая опора канатной дороги | 7×9, в 26 | 13×10 |  |  |  |  |  | пеш 1, дор 1 |
| `cableway_pole_heavy_4` | Тяжелая опора канатной дороги | 8×9, в 32 | 13×10 |  |  |  |  |  | пеш 1, дор 1 |
| `cableway_pole_light_1` | Легкая опора канатной дороги | 4×3, в 15 | 11×10 |  |  |  |  |  | пеш 1, дор 1 |
| `cableway_pole_light_2` | Легкая опора канатной дороги | 4×4, в 19 | 11×10 |  |  |  |  |  | пеш 1, дор 1 |
| `cableway_pole_light_3` | Легкая опора канатной дороги | 5×5, в 26 | 12×10 |  |  |  |  |  | пеш 1, дор 1 |
| `cableway_pole_light_4` | Легкая опора канатной дороги | 7×7, в 35 | 12×10 |  |  |  |  |  | пеш 1, дор 1 |
| `conveyor_engine` | Конвейерная башня | 8×19, в 24 | 9×19 |  | GRAVEL 1 |  |  |  | пеш 1, дор 1, конвейер 4 |
| `conveyor_engine_3in_e` | Конвейерная башня | 13×19, в 24 | 13×19 |  | GRAVEL 1 |  |  |  | пеш 1, дор 1, конвейер 4 |
| `conveyor_engine_3in_f` | Конвейерная башня | 13×19, в 34 | 13×19 |  | GRAVEL 1 |  |  |  | пеш 1, дор 1, конвейер 4 |
| `conveyor_engine_3in_g` | Конвейерная башня | 13×19, в 24 | 13×19 |  | GRAVEL 1 |  |  |  | пеш 1, дор 1, конвейер 4 |
| `conveyor_engine_3in_h` | Конвейерная башня | 13×19, в 34 | 13×19 |  | GRAVEL 1 |  |  |  | пеш 1, дор 1, конвейер 4 |
| `conveyor_engine_overpasse` | Конвейерная эстакада | 7×39, в 25 | 14×39 |  | GRAVEL 1 |  |  |  | дор 2, конвейер 2 |
| `conveyor_engine_overpassf` | Конвейерная эстакада | 9×40, в 25 | 14×40 |  | GRAVEL 1 |  |  |  | дор 2, конвейер 4 |
| `conveyor_engine_overpassg` | Конвейерная эстакада | 9×40, в 25 | 14×40 |  | GRAVEL 1 |  |  |  | дор 2, конвейер 4 |
| `conveyor_enginf` | Конвейерная башня | 8×19, в 34 | 9×19 |  | GRAVEL 1 |  |  |  | пеш 1, дор 1, конвейер 4 |
| `conveyor_enging` | Конвейерная башня | 8×19, в 24 | 9×19 |  | GRAVEL 1 |  |  |  | пеш 1, дор 1, конвейер 4 |
| `conveyor_enginh` | Конвейерная башня | 8×19, в 34 | 9×19 |  | GRAVEL 1 |  |  |  | пеш 1, дор 1, конвейер 4 |
| `pipe_pumping_station` | Насосная станция | 17×27, в 24 | 18×27 |  | OIL 1 |  |  |  | дор 1, труба 4 |
| `pipe_pumping_station2` | Насосная станция | 17×27, в 24 | 18×27 |  | OIL 1 |  |  |  | дор 1, труба 4 |
| `pipe_pumping_station_underground` | Подземная насосная станция | 17×27, в 10 | 18×27 |  | OIL 1 |  |  |  | дор 1, труба 4 |
| `pipe_pumping_station_underground2` | Подземная насосная станция | 17×27, в 10 | 18×27 |  | OIL 1 |  |  |  | дор 1, труба 4 |

## FACTORY (52)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `alumina_plant` | Глиноземный завод | 122×204, в 95 | 122×204 | 370 | IMPORT:COVERED 35, IMPORT:WATER 65, EXPORT:CEMENT 220, EXPORT:SEWAGE 65 | bauxite 0.21, coal 0.08, chemicals 0.007, water 0.08 | alumina 0.09 | CONSUMPTION_WATER_REQUIRED_QUALITY 0.75 | пеш 5, дор 1, вода вх 2, канал вых 2, конвейер 4 |
| `aluminium_plant` | Алюминиевый завод | 205×181, в 67 | 206×182 | 350 | IMPORT:COVERED 45, IMPORT:CEMENT 150, EXPORT:OPEN 280 | alumina 0.15, chemicals 0.0072; в секунду: eletric 2.35 | aluminium 0.087 |  | пеш 4, дор 1, эл выс вых 1 |
| `animal_farm` | Мясной совхоз | 61×83, в 41 | 61×84 | 50 | IMPORT:COVERED 75, EXPORT:LIVESTOCK 35 | plants 0.4, water 0.25 | livestock 0.2 | CONSUMPTION_WATER_REQUIRED_QUALITY 0.93, ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 2, дор 1 |
| `asphalt_plant` | Асфальтовый завод | 27×33, в 39 | 28×33 | 5 | IMPORT:OIL 15, IMPORT:GRAVEL 30, EXPORT:GRAVEL 1 | gravel 25, bitumen 4, eletric 3 | asphalt 29 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 2, дор 1, конвейер 1, труба 1 |
| `bauxite_processing` | Бокситообогатительный комбинат | 55×58, в 22 | 55×58 | 25 | IMPORT:GRAVEL 50, EXPORT:GRAVEL 50 | rawbauxite 5 | bauxite 3 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3 | пеш 3, дор 1, конвейер 8 |
| `brick_factory` | Кирпичный завод | 71×58, в 55 | 71×58 | 75 | EXPORT:OPEN 70 | coal 0.45 | bricks 0.68 | OBSOLETE, HEATING_DISABLE | пеш 4, дор 1, конвейер 1 |
| `brick_factory_v2` | Кирпичный завод | 71×58, в 55 | 72×59 | 75 | EXPORT:OPEN 70 | coal 0.45 | bricks 0.68 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 6, дор 1, конвейер 1 |
| `cement_plant` | Цементный завод | 129×73, в 76 | 131×73 | 30 | EXPORT:CEMENT 70 | coal 0.75, gravel 7; в секунду: eletric 0.1 | cement 2.7 | OBSOLETE, HEATING_DISABLE | пеш 3, дор 1, конвейер 4 |
| `cement_plant_v2` | Цементный завод | 129×73, в 76 | 131×73 | 30 | EXPORT:CEMENT 70 | coal 0.75, gravel 7; в секунду: eletric 0.1 | cement 2.7 | OBSOLETE, HEATING_DISABLE | пеш 3, дор 1, конвейер 4 |
| `cement_plant_v3` | Цементный завод | 129×73, в 76 | 141×73 | 30 | EXPORT:CEMENT 70 | coal 0.75, gravel 7; в секунду: eletric 0.1 | cement 2.7 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 3, дор 1, конвейер 4 |
| `chemical_plant` | Химический завод | 80×50, в 41 | 80×50 | 60 | IMPORT:COVERED 10, IMPORT:OIL 20, IMPORT:GRAVEL 10, IMPORT:OPEN 10, IMPORT:WATER 20, EXPORT:COVERED 10, EXPORT:SEWAGE 20 | gravel 0.012, wood 0.014, plants 0.013, oil 0.02, water 0.17; в секунду: eletric 0.24 | chemicals 0.0135 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, CONSUMPTION_WATER_REQUIRED_QUALITY 0.67, HEATING_DISABLE | пеш 2, дор 1, вода вх 1, канал вых 1, конвейер 1, труба 1 |
| `clothing_factory` | Швейная фабрика | 68×40, в 22 | 68×40 | 80 | IMPORT:COVERED 30, EXPORT:COVERED 15 | fabric 0.03 | clothes 0.015 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 4, дор 1 |
| `coal_processing` | Угольный комбинат | 77×79, в 56 | 90×79 | 30 | IMPORT:GRAVEL 100, EXPORT:GRAVEL 100 | rawcoal 14; в секунду: eletric 0.25 | coal 8 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 7, дор 1, конвейер 9 |
| `concrete_plant` | Бетонный завод | 32×51, в 27 | 32×52 | 5 | IMPORT:GRAVEL 80, IMPORT:CEMENT 40, IMPORT:WATER 60, EXPORT:CONCRETE 0.5 | gravel 27, cement 6, water 17; в секунду: eletric 0.2 | concrete 35 | CONSUMPTION_WATER_REQUIRED_QUALITY 0.55, OBSOLETE, HEATING_DISABLE | пеш 2, дор 1, конвейер 1 |
| `concrete_plant_v2` | Бетонный завод | 33×51, в 27 | 34×52 | 5 | IMPORT:GRAVEL 80, IMPORT:CEMENT 40, IMPORT:WATER 60, EXPORT:CONCRETE 1 | gravel 27, cement 6, water 17; в секунду: eletric 0.2 | concrete 35 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, CONSUMPTION_WATER_REQUIRED_QUALITY 0.55, HEATING_DISABLE | пеш 4, дор 1, вода вх 2, конвейер 3 |
| `distillery` | Ликеро-водочный завод | 33×35, в 50 | 33×37 | 100 | IMPORT:COVERED 80, EXPORT:COVERED 30 | plants 0.3, water 0.135; в секунду: eletric 0.17 | alcohol 0.06 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, CONSUMPTION_WATER_REQUIRED_QUALITY 0.95 | пеш 6, дор 1 |
| `eletronic_components_factory` | Завод электродеталей | 52×65, в 21 | 52×65 | 150 | EXPORT:COVERED 20 | plastics 0.01, steel 0.01, chemicals 0.008 | ecomponents 0.025 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 2, дор 1 |
| `eletronic_factory` | Завод электроники | 75×54, в 9 | 75×55 | 150 | EXPORT:COVERED 70 | ecomponents 0.01, plastics 0.015, mcomponents 0.01 | eletronics 0.03 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 5, дор 1 |
| `explosive_factory` | Завод взрывчатых веществ | 76×75, в 54 | 76×75 | 75 | IMPORT:COVERED 35, IMPORT:GRAVEL 50, IMPORT:OPEN 20, EXPORT:COVERED 20 | chemicals 0.01, gravel 0.03, wood 0.02 | explosives 0.015 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3 | пеш 6, дор 1, конвейер 1 |
| `fabric_factory` | Ткацкая фабрика | 55×47, в 37 | 55×47 | 100 | IMPORT:COVERED 40, IMPORT:WATER 20, EXPORT:COVERED 15, EXPORT:SEWAGE 20 | plants 0.2, chemicals 0.005, water 0.11; в секунду: eletric 0.18 | fabric 0.05 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, CONSUMPTION_WATER_REQUIRED_QUALITY 0.85, HEATING_DISABLE | пеш 4, дор 1, вода вх 2, канал вых 2 |
| `fertilizer` | Завод органических удобрений | 34×38, в 22 | 36×39 | 25 |  | waste_bio 1.2, chemicals 0.003 | fertiliser 1 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 2, дор 1 |
| `fertilizer_liquid` | Завод синтетических удобрений | 42×36, в 25 | 42×37 | 25 | EXPORT:OIL 75, IMPORT:COVERED 15 | chemicals 0.0125, water 0.6 | fertiliser_liquid 0.7 | CONSUMPTION_WATER_REQUIRED_QUALITY 0.85, HEATING_DISABLE | пеш 2, дор 1 |
| `fertilizer_liquid_v2` | Завод синтетических удобрений | 42×36, в 25 | 43×37 | 25 | IMPORT:COVERED 15, IMPORT:WATER 15, EXPORT:OIL 75, EXPORT:SEWAGE 15 | chemicals 0.0125, water 0.6 | fertiliser_liquid 0.7 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, CONSUMPTION_WATER_REQUIRED_QUALITY 0.85, HEATING_DISABLE | пеш 2, дор 1, вода вх 1, канал вых 1, труба 2 |
| `food_factory` | Пищевой завод | 73×25, в 49 | 81×39 | 170 | IMPORT:COVERED 90, EXPORT:COVERED 50 | plants 0.25, water 0.05 | food 0.12 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, CONSUMPTION_WATER_REQUIRED_QUALITY 0.97, HEATING_DISABLE | пеш 8, дор 1 |
| `gravel_processing` | Гравийно-сортировочный завод | 117×59, в 37 | 147×60 | 15 | IMPORT:GRAVEL 100, EXPORT:GRAVEL 380 | rawgravel 8; в секунду: eletric 0.4 | gravel 5.5 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 4, дор 1, конвейер 7 |
| `gravel_processing_small` | Гравийно-сортировочный завод | 37×60, в 8 | 37×60 | 5 | IMPORT:GRAVEL 10, EXPORT:GRAVEL 145 | rawgravel 8; в секунду: eletric 0.15 | gravel 5.5 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 2, дор 1, конвейер 5 |
| `incinerator_big` | Большой мусоросжигательный завод | 82×66, в 71 | 82×66 | 25 | IMPORT:WASTE 95 | waste 2.35 |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 5, дор 1 |
| `incinerator_small` | Малый мусоросжигательный завод | 52×41, в 71 | 52×41 | 15 | IMPORT:WASTE 55 | waste 2 |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 6, дор 1 |
| `iron_processing` | Железорудный комбинат | 77×79, в 56 | 90×79 | 15 | IMPORT:GRAVEL 100, EXPORT:GRAVEL 100 | rawiron 15; в секунду: eletric 0.28 | iron 7 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 7, дор 1, конвейер 9 |
| `mechanical_components_factory` | Завод механических деталей | 93×65, в 52 | 95×67 | 150 | IMPORT:OPEN 40, EXPORT:COVERED 35 | steel 0.15 | mcomponents 0.1 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 3, дор 1 |
| `nuclear_fuel_plant` | Завод ядерного топлива | 98×128, в 27 | 113×128 | 50 +70 с высш. | IMPORT:COVERED 30, IMPORT:NUCLEAR1 4, IMPORT:VEHICLES 15, EXPORT:NUCLEAR2 2, EXPORT:VEHICLES 15 | uf6 0.008, chemicals 0.015, vehicles 1; в секунду: eletric 0.7 | nuclearfuel 0.0019, vehicles 1 | HEATING_DISABLE | пеш 5, дор 1 |
| `oil_rafinery` | Нефтеперерабатывающий завод | 112×136, в 52 | 114×137 | 500 | IMPORT:OIL 450 | oil 0.5; в секунду: eletric 0.21 | fuel 0.25, bitumen 0.15 | OBSOLETE, HEATING_DISABLE | пеш 3, дор 1, труба 6 |
| `oil_rafinery_v2` | Нефтеперерабатывающий завод | 112×136, в 52 | 114×137 | 500 | IMPORT:OIL 450 | oil 0.5; в секунду: eletric 0.21 | fuel 0.25, bitumen 0.15 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 3, дор 1, труба 6 |
| `panels_factory` | Завод бетонных панелей | 80×67, в 32 | 82×69 | 65 | IMPORT:CEMENT 25, IMPORT:GRAVEL 70, EXPORT:OPEN 120 | cement 0.15, gravel 1; в секунду: eletric 0.1 | prefabpanels 1.1 | OBSOLETE, HEATING_DISABLE | пеш 3, дор 1, конвейер 1 |
| `panels_factory_v2` | Завод бетонных панелей | 80×67, в 32 | 82×69 | 65 | IMPORT:CEMENT 25, IMPORT:GRAVEL 70, EXPORT:OPEN 120 | cement 0.15, gravel 1; в секунду: eletric 0.1 | prefabpanels 1.1 | OBSOLETE, HEATING_DISABLE | пеш 3, дор 1, конвейер 1 |
| `panels_factory_v3` | Завод бетонных панелей | 80×67, в 32 | 81×67 | 65 | IMPORT:CEMENT 25, IMPORT:GRAVEL 70, EXPORT:OPEN 120 | cement 0.15, gravel 1; в секунду: eletric 0.1 | prefabpanels 1.1 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 4, дор 1, конвейер 1 |
| `plastics_factory` | Завод пластмасс | 52×41, в 34 | 53×41 | 60 | IMPORT:COVERED 30, IMPORT:OIL 50, EXPORT:COVERED 50 | chemicals 0.05, oil 0.45; в секунду: eletric 0.26 | plastics 0.11 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 2, дор 1, труба 1 |
| `sawmill` | Лесопилка | 37×38, в 19 | 37×40 | 20 | IMPORT:OPEN 60, EXPORT:OPEN 50 | wood 9; в секунду: eletric 0.027 | boards 7 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 1, дор 1 |
| `slaughterhouse` | Мясокомбинат | 44×42, в 30 | 45×44 | 50 | IMPORT:LIVESTOCK 15, EXPORT:COOLER 7.5 | livestock 3 | meat 1.2 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 5, дор 1 |
| `steel_mill` | Сталелитейный завод | 100×175, в 66 | 103×176 | 500 | EXPORT:OPEN 150 | coal 0.75, iron 0.4; в секунду: eletric 0.19 | steel 0.086 | OBSOLETE, HEATING_DISABLE | пеш 5, дор 1, эл выс вх 1, жд 6, конвейер 4 |
| `steel_mill_v2` | Сталелитейный завод | 100×175, в 66 | 104×175 | 500 | EXPORT:OPEN 150 | coal 0.75, iron 0.4; в секунду: eletric 0.55 | steel 0.086 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 5, дор 1, эл выс вх 1, жд 6, конвейер 4 |
| `uranium_conversion` | Уранообогатительный комбинат | 61×84, в 41 | 62×84 | 80 +40 с высш. | IMPORT:OPEN 70, IMPORT:COVERED 30, EXPORT:NUCLEAR1 4, EXPORT:VEHICLES 15 | yellowcake 0.025, chemicals 0.0026; в секунду: eletric 0.8 | uf6 0.012, vehicles 1 | HEATING_DISABLE | пеш 2, дор 1 |
| `uranium_processing` | Завод очистки урановой руды | 61×60, в 12 | 61×60 | 60 +15 с высш. | IMPORT:GRAVEL 100, EXPORT:OPEN 50 | uranium 0.6; в секунду: eletric 0.6 | yellowcake 0.0285 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 1, дор 1, конвейер 3 |
| `waste_aluminiumrecycling` | Завод по переработке алюминия | 31×41, в 21 | 31×43 | 25 | IMPORT:GRAVEL 50, EXPORT:OPEN 40 | waste_aluminium 1 | aluminium 0.5 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 3, дор 1, конвейер 1 |
| `waste_aluminiumseparation` | Завод по извлечению алюминиевого лома | 28×57, в 12 | 29×58 | 20 |  | waste 1; в секунду: eletric 0.1 |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 1, дор 1, конвейер 1 |
| `waste_generalseparation` | Мусоросортировочный завод | 72×43, в 12 | 72×43 | 50 |  | waste_mixed 1; в секунду: eletric 0.03 |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 6, дор 1, конвейер 4 |
| `waste_gravelrecycling` | Завод по переработке гравия | 99×32, в 14 | 100×35 | 15 | IMPORT:GRAVEL 40, EXPORT:GRAVEL 270 | waste_gravel 6; в секунду: eletric 0.3 | gravel 4 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 3, дор 2, конвейер 3 |
| `waste_plasticrecycling` | Завод по переработке пластмасс | 57×35, в 15 | 58×37 | 20 | IMPORT:OPEN 30, EXPORT:COVERED 30 | waste_plastic 0.5; в секунду: eletric 0.07 | plastics 0.1 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 3, дор 1 |
| `waste_steelrecycling` | Завод по переработке стали | 43×60, в 25 | 44×60 | 30 | IMPORT:GRAVEL 60, EXPORT:OPEN 50 | waste_steel 1.2 | steel 0.65 | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 2, дор 1, конвейер 1 |
| `waste_steelseparation` | Завод по извлечению железного лома | 43×32, в 10 | 44×33 | 15 |  | waste 2; в секунду: eletric 0.1 |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 2, дор 1, конвейер 1 |
| `waste_treatment_plant` | Завод утилизации опасных отходов | 95×66, в 23 | 95×66 | 30 |  | waste_toxic 0.25, chemicals 0.037 |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 5, дор 1 |
| `waste_treatment_plant_v2` | Завод утилизации опасных отходов | 95×66, в 23 | 97×67 | 50 |  | chemicals 0.037, waste_toxic 0.75; в секунду: eletric 0.03 |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.4, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.3, HEATING_DISABLE | пеш 5, дор 1, конвейер 3 |

## FARM (4)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `farm` | Зерновой совхоз | 61×64, в 54 | 61×64 |  | EXPORT:COVERED 170, FUEL:OIL 30 |  | plants 0.01 | OBSOLETE | дор 1 |
| `farm_large` | Большой совхоз | 154×82, в 22 | 154×83 |  | EXPORT:COVERED 180, FUEL:OIL 45 |  | plants 10 |  | дор 1, труба 1 |
| `farm_medium` | Средний совхоз | 101×44, в 22 | 102×46 |  | EXPORT:COVERED 95, FUEL:OIL 25 |  | plants 10 |  | дор 1, труба 1 |
| `farm_small` | Малый совхоз | 72×46, в 22 | 72×47 |  | EXPORT:COVERED 50, FUEL:OIL 12 |  | plants 10 |  | дор 1 |

## FIELD (3)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `field_big` | Большое поле | 286×168, в 4 | 288×168 | 250 | SPECIAL:COVERED 500 |  |  | HEATING_DISABLE | дор 1 |
| `field_medium` | Среднее поле | 163×96, в 4 | 164×96 | 150 | SPECIAL:COVERED 500 |  |  | HEATING_DISABLE | дор 1 |
| `field_small` | Малое поле | 82×48, в 4 | 82×48 | 75 | SPECIAL:COVERED 500 |  |  | HEATING_DISABLE | дор 1 |

## FIRESTATION (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `firestation` | Пожарная часть | 44×31, в 35 | 44×36 | 40 | FUEL:OIL 40 |  |  |  | пеш 9, дор 1 |

## FOREIGN_PIPELINE_EXPORT (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `foreign_pipe_player` | Подключение к иностранному трубопроводу | 11×68, в 10 | 13×68 |  | OIL 1 |  |  | ELECTRIC_DISABLE | дор 1, труба 2 |

## FORKLIFT_GARAGE (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `forklift_crossing` | Перекресток служебных дорог | 14×15, в -0 | 14×15 |  |  |  |  |  |  |
| `forklift_station` | Гараж погрузчиков | 21×22, в 5 | 23×24 |  | FUEL:OIL 25 |  |  |  | дор 1 |

## GARBAGE_OFFICE (4)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `muddy_technical` | База спецтехники (бесплатная) | 12×20, в 1 | 22×23 |  |  |  |  | COUNT_LIMIT 5, WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |
| `technical_services` | База спецтехники | 60×57, в 15 | 60×57 |  | FUEL:OIL 30 |  |  |  | дор 1 |
| `technical_services_big` | База спецтехники | 80×57, в 15 | 80×57 |  | FUEL:OIL 30 |  |  |  | дор 3 |
| `technical_services_small` | База спецтехники | 42×43, в 8 | 42×48 |  | FUEL:OIL 30 |  |  |  | дор 1 |

## GAS_STATION (5)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `gas_station` | Автозаправочная станция | 26×24, в 5 | 26×24 |  | SPECIAL:OIL 70 |  |  |  | дор 1 |
| `gas_station_v2` | Автозаправочная станция | 32×31, в 5 | 32×32 |  | SPECIAL:OIL 70 |  |  |  | дор 1 |
| `gas_station_v3` | Автозаправочная станция | 26×33, в 5 | 26×34 |  | SPECIAL:OIL 70 |  |  |  | дор 2 |
| `muddy_gasstation` | Автозаправочная станция (бесплатная) | 22×9, в 1 | 31×9 |  | FUEL:OIL 20 |  |  | COUNT_LIMIT 12, WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |
| `rail_gas_station` | Железнодорожная заправочная станция | 38×37, в 6 | 39×38 |  | SPECIAL:OIL 150 |  |  |  | дор 2, жд 4, труба 2 |

## HEATING_ENDSTATION (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `heating_endstation_big` | Тепловой пункт | 22×20, в 18 | 22×20 |  |  |  |  |  | дор 1, тепло б 2 |
| `heating_endstation_small` | Малый тепловой пункт | 23×15, в 10 | 23×15 |  |  |  |  |  | пеш 1, дор 1, тепло м 1 |

## HEATING_PLANT (3)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `heating_plant_big` | Теплоцентраль | 51×95, в 91 | 51×96 | 30 |  | coal 0.28 | heat 350 |  | пеш 5, дор 1, тепло б 7, конвейер 3 |
| `heating_plant_small` | Котельная | 59×20, в 25 | 59×20 | 7 |  | coal 0.3 | heat 300 |  | пеш 2, дор 1, тепло м 4, конвейер 1 |
| `incinerator_heat` | Мусоросжигательная теплостанция | 43×56, в 48 | 43×57 | 15 | IMPORT:WASTE 45 | waste 2.5 | heat 450 |  | пеш 3, дор 1, тепло б 3 |

## HEATING_SWITCH (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `heating_pumpingstation` | Теплораспределительная насосная станция | 15×13, в 9 | 15×13 |  |  |  |  |  | пеш 2, дор 1, тепло б 4 |
| `heating_pumpingstation_small` | Малая теплораспределительная насосная станция | 10×11, в 9 | 10×12 |  |  |  |  |  | пеш 1, дор 1, тепло м 3 |

## HOSPITAL (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `hospital` | Больница | 30×50, в 31 | 32×50 | 50 +50 с высш. | FUEL:OIL 40 |  |  | CITIZEN_ABLE_SERVE 3 | пеш 4, дор 1 |
| `hospital_v2` | Больница | 37×50, в 31 | 37×50 | 30 +30 с высш. | FUEL:OIL 40 |  |  | CITIZEN_ABLE_SERVE 3 | пеш 5, дор 1 |

## HOTEL (4)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `hotel` | Горная гостиница (***) | 19×22, в 42 | 19×22 | 20 | DEMAND_HOTEL:COVERED 24, DEMAND_HOTEL:COOLER 2 |  |  | CITIZEN_ABLE_SERVE 5, ELETRIC_WITHOUT_WORKING_FACTOR 0.5, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2, ATTRACTIVE_SCORE 2.7 | пеш 2, дор 1 |
| `hotel_horsky` | Горная гостиница (***) | 32×59, в 24 | 32×60 | 15 | DEMAND_HOTEL:COVERED 18, DEMAND_HOTEL:COOLER 2 |  |  | CITIZEN_ABLE_SERVE 5, ELETRIC_WITHOUT_WORKING_FACTOR 0.5, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2, ATTRACTIVE_SCORE 3.5 | пеш 2, дор 1 |
| `hotel_hutnik_big` | Центральная гостиница (****) | 31×55, в 45 | 34×55 | 50 | DEMAND_HOTEL:COVERED 25, DEMAND_HOTEL:COOLER 3 |  |  | CITIZEN_ABLE_SERVE 3, ELETRIC_WITHOUT_WORKING_FACTOR 0.5, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2, ATTRACTIVE_SCORE 4 | пеш 5, дор 1 |
| `hotel_hutnik_small` | Малая гостиница (**) | 24×43, в 18 | 28×43 | 14 | DEMAND_HOTEL:COVERED 18, DEMAND_HOTEL:COOLER 1 |  |  | CITIZEN_ABLE_SERVE 6, ELETRIC_WITHOUT_WORKING_FACTOR 0.5, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2, ATTRACTIVE_SCORE 2.1 | пеш 2, дор 1 |

## KINDERGARTEN (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `kindergarten` | Детский сад | 32×28, в 8 | 33×28 | 10 |  |  |  | CITIZEN_ABLE_SERVE 10 | пеш 3, дор 1 |

## KINO (3)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `co_theatre` | Республиканский театр | 73×77, в 26 | 76×77 | 20 |  |  |  | CITIZEN_ABLE_SERVE 5, ATTRACTIVE_SCORE 2.4 | пеш 3, дор 2 |
| `kino` | Кинотеатр | 37×55, в 12 | 37×56 | 5 |  |  |  | CITIZEN_ABLE_SERVE 30 | пеш 5, дор 1 |
| `kino_druzba` | Кинотеатр | 55×41, в 16 | 55×42 | 6 |  |  |  | CITIZEN_ABLE_SERVE 25 | пеш 4, дор 1 |

## LIVING (54)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `civ_dedina1a` | Сельский дом | 7×6, в 6 | 7×6 |  | PASSANGER 3 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_dedina1b` | Сельский дом | 7×6, в 6 | 7×6 |  | PASSANGER 3 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_dedina2a` | Сельский дом | 8×7, в 8 | 8×8 |  | PASSANGER 6 |  |  | QUALITY_OF_LIVING 1.02, CIVIL_BUILDING | пеш 1 |
| `civ_dedina3a` | Сельский дом | 11×6, в 7 | 11×6 |  | PASSANGER 4 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_dedina3b` | Сельский дом | 11×6, в 7 | 11×6 |  | PASSANGER 4 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_dedina4a` | Сельский дом | 16×8, в 8 | 16×8 |  | PASSANGER 10 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_dedina5a` | Сельский дом | 14×7, в 7 | 14×7 |  | PASSANGER 13 |  |  | QUALITY_OF_LIVING 0.97, CIVIL_BUILDING | пеш 4 |
| `civ_dedina6a` | Сельский дом | 9×6, в 6 | 9×6 |  | PASSANGER 4 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_dedina_drevenica1` | Сельский дом | 7×15, в 9 | 7×15 |  | PASSANGER 3 |  |  | QUALITY_OF_LIVING 0.65, CIVIL_BUILDING | пеш 1 |
| `civ_dedina_drevenica2` | Сельский дом | 7×15, в 9 | 7×15 |  | PASSANGER 3 |  |  | QUALITY_OF_LIVING 0.65, CIVIL_BUILDING | пеш 1 |
| `civ_dedina_drevenica5` | Сельский дом | 6×12, в 7 | 6×12 |  | PASSANGER 3 |  |  | QUALITY_OF_LIVING 0.63, CIVIL_BUILDING | пеш 1 |
| `civ_dedinaru1` | Сельский дом | 12×12, в 7 | 12×12 |  | PASSANGER 5 |  |  | QUALITY_OF_LIVING 0.65, CIVIL_BUILDING | пеш 1 |
| `civ_dedinaru2` | Сельский дом | 9×11, в 8 | 9×11 |  | PASSANGER 5 |  |  | QUALITY_OF_LIVING 0.65, CIVIL_BUILDING | пеш 1 |
| `civ_dedinaru3` | Сельский дом | 8×18, в 6 | 8×18 |  | PASSANGER 5 |  |  | QUALITY_OF_LIVING 0.65, CIVIL_BUILDING | пеш 1 |
| `civ_dedinaru4` | Сельский дом | 9×14, в 7 | 9×14 |  | PASSANGER 5 |  |  | QUALITY_OF_LIVING 0.65, CIVIL_BUILDING | пеш 1 |
| `civ_dedinaru5` | Сельский дом | 10×8, в 5 | 10×8 |  | PASSANGER 5 |  |  | QUALITY_OF_LIVING 0.65, CIVIL_BUILDING | пеш 1 |
| `civ_mesto1` | Старые городские дома | 40×59, в 19 | 40×60 |  | PASSANGER 95 |  |  | CIVIL_BUILDING | пеш 7 |
| `civ_mesto2` | Старые городские дома | 50×79, в 19 | 50×79 |  | PASSANGER 120 |  |  | CIVIL_BUILDING | пеш 11 |
| `civ_mesto3` | Старые городские дома | 20×89, в 19 | 21×89 |  | PASSANGER 75 |  |  | CIVIL_BUILDING | пеш 6 |
| `civ_mesto4` | Старые городские дома | 60×109, в 19 | 61×109 |  | PASSANGER 135 |  |  | CIVIL_BUILDING | пеш 10, дор 1 |
| `civ_mesto5` | Старые городские дома | 20×89, в 19 | 21×89 |  | PASSANGER 85 |  |  | CIVIL_BUILDING | пеш 7 |
| `civ_newoldcity1` | Старые городские дома | 13×8, в 14 | 13×8 |  | PASSANGER 20 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_newoldcity10` | Старые городские дома | 12×7, в 14 | 12×7 |  | PASSANGER 25 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_newoldcity2` | Старые городские дома | 12×7, в 15 | 12×7 |  | PASSANGER 20 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_newoldcity3` | Старые городские дома | 12×7, в 17 | 12×7 |  | PASSANGER 25 |  |  | CIVIL_BUILDING | пеш 2 |
| `civ_newoldcity4` | Старые городские дома | 13×9, в 15 | 13×9 |  | PASSANGER 25 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_newoldcity5` | Старые городские дома | 14×9, в 19 | 14×9 |  | PASSANGER 20 |  |  | CIVIL_BUILDING | пеш 3 |
| `civ_newoldcity6` | Старые городские дома | 14×9, в 16 | 14×9 |  | PASSANGER 30 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_newoldcity7` | Старые городские дома | 12×8, в 14 | 12×8 |  | PASSANGER 30 |  |  | CIVIL_BUILDING | пеш 1 |
| `civ_newoldcity8` | Старые городские дома | 12×7, в 14 | 12×7 |  | PASSANGER 30 |  |  | CIVIL_BUILDING | пеш 2 |
| `civ_newoldcity9` | Старые городские дома | 12×7, в 15 | 12×8 |  | PASSANGER 30 |  |  | CIVIL_BUILDING | пеш 2 |
| `hrusevka` | Панельный многоквартирный дом (серия 464) | 22×55, в 16 | 22×55 |  | PASSANGER 115 |  |  | QUALITY_OF_LIVING 0.6 | пеш 2, дор 1 |
| `internat1` | Университетские общежития | 84×18, в 17 | 85×18 |  | PASSANGER 85 |  |  |  | пеш 2, дор 1 |
| `internat2` | Университетские общежития | 60×39, в 17 | 61×39 |  | PASSANGER 85 |  |  |  | пеш 3, дор 1 |
| `panelak_juh1` | Панельный многоквартирный дом | 40×20, в 32 | 40×20 |  |  |  |  | QUALITY_OF_LIVING 0.83, OBSOLETE | пеш 4, дор 2 |
| `panelak_juh2` | Панельный многоквартирный дом | 17×13, в 24 | 17×13 |  |  |  |  | QUALITY_OF_LIVING 0.83, OBSOLETE | пеш 2, дор 1 |
| `panelak_juh3` | Панельный многоквартирный дом | 22×16, в 32 | 27×16 |  |  |  |  | QUALITY_OF_LIVING 0.83, OBSOLETE | пеш 2, дор 1 |
| `panelak_kvp1` | Панельный многоквартирный дом | 14×18, в 25 | 14×18 |  |  |  |  | QUALITY_OF_LIVING 0.87, OBSOLETE | пеш 2, дор 1 |
| `panelak_kvp2` | Панельный многоквартирный дом | 31×17, в 17 | 31×17 |  |  |  |  | QUALITY_OF_LIVING 0.87, OBSOLETE | пеш 2, дор 1 |
| `panelak_kvp3` | Панельный многоквартирный дом | 40×17, в 26 | 40×17 |  |  |  |  | QUALITY_OF_LIVING 0.87, OBSOLETE | пеш 3, дор 1 |
| `panelak_moderny` | Панельный многоквартирный дом | 28×23, в 48 | 28×23 |  | PASSANGER 180 |  |  | QUALITY_OF_LIVING 0.85 | пеш 2 |
| `panelak_moderny_uzky` | Панельный многоквартирный дом | 22×46, в 48 | 22×46 |  | PASSANGER 220 |  |  | QUALITY_OF_LIVING 0.85 | пеш 4 |
| `panelakl4_dlhyA` | Панельный многоквартирный дом | 36×21, в 32 | 36×21 |  | PASSANGER 105 |  |  | QUALITY_OF_LIVING 0.91 | пеш 4, дор 2 |
| `panelakl4_dlhyB` | Панельный многоквартирный дом | 54×21, в 32 | 54×21 |  | PASSANGER 145 |  |  | QUALITY_OF_LIVING 0.96 | пеш 5, дор 3 |
| `panelakl4_dlhyC` | Панельный многоквартирный дом | 54×21, в 32 | 54×21 |  | PASSANGER 157 |  |  | QUALITY_OF_LIVING 0.93 | пеш 5, дор 3 |
| `panelakl4_kocka` | Панельный многоквартирный дом | 18×21, в 18 | 18×21 |  | PASSANGER 27 |  |  | QUALITY_OF_LIVING 0.91 | пеш 2, дор 1 |
| `panelakl4_veziak` | Панельный многоквартирный дом | 19×22, в 42 | 19×22 |  | PASSANGER 87 |  |  | QUALITY_OF_LIVING 0.94 | пеш 2, дор 1 |
| `residental_brick_1` | Кирпичный многоквартирный дом | 21×21, в 16 | 21×21 |  | PASSANGER 20 |  |  | QUALITY_OF_LIVING 0.87 | пеш 2, дор 1 |
| `residental_brick_2` | Кирпичный многоквартирный дом | 34×23, в 20 | 34×23 |  | PASSANGER 50 |  |  | QUALITY_OF_LIVING 0.78 | пеш 4, дор 2 |
| `residental_brick_3` | Кирпичный многоквартирный дом | 20×88, в 21 | 20×88 |  | PASSANGER 157 |  |  | QUALITY_OF_LIVING 0.8 | пеш 4, дор 1 |
| `residental_brick_4` | Кирпичный многоквартирный дом | 42×15, в 13 | 42×15 |  | PASSANGER 40 |  |  | QUALITY_OF_LIVING 0.75 | пеш 2, дор 1 |
| `residental_brick_5` | Кирпичный многоквартирный дом | 26×55, в 22 | 27×55 |  | PASSANGER 110 |  |  | QUALITY_OF_LIVING 0.82 | пеш 8, дор 1 |
| `residental_brick_6` | Кирпичный многоквартирный дом | 18×23, в 21 | 18×23 |  | PASSANGER 30 |  |  | QUALITY_OF_LIVING 0.82 | пеш 2, дор 1 |
| `residental_brick_7` | Кирпичный многоквартирный дом | 15×13, в 12 | 15×13 |  | PASSANGER 10 |  |  | QUALITY_OF_LIVING 0.89 | пеш 2, дор 1 |

## MINE_BAUXITE (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `bauxite_mine` | Бокситовый рудник | 25×46, в 10 | 26×46 | 45 | EXPORT:GRAVEL 15, FUEL:OIL 15 |  | rawbauxite 0.5 |  | пеш 3, дор 1, конвейер 1 |

## MINE_COAL (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `coal_mine` | Угольная шахта | 37×25, в 46 | 39×25 | 220 | EXPORT:GRAVEL 20 |  | rawcoal 4.2 | HEATING_DISABLE | пеш 4, дор 1, конвейер 1 |

## MINE_GRAVEL (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `gravel_mine_big` | Гравийный карьер | 39×37, в 10 | 39×37 | 100 | EXPORT:GRAVEL 100, FUEL:OIL 30 |  | rawgravel 3.5 | WATERSEWAGE_DISABLE, HEATING_DISABLE | пеш 2, дор 1, конвейер 2, труба 1 |
| `gravelmine` | Гравийный карьер | 15×10, в 4 | 20×12 | 40 | EXPORT:GRAVEL 0.5, FUEL:OIL 10 |  | rawgravel 3.5 | WATERSEWAGE_DISABLE, HEATING_DISABLE | дор 1 |

## MINE_IRON (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `iron_mine` | Железный рудник | 37×25, в 46 | 39×25 | 250 | EXPORT:GRAVEL 20 |  | rawiron 4 | HEATING_DISABLE | пеш 4, дор 1, конвейер 1 |

## MINE_OIL (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `oil_mine` | Нефтяная скважина | 8×30, в 12 | 14×30 |  | EXPORT:OIL 15 | ; в секунду: eletric 0.065 | oil 7 |  | дор 1, труба 1 |

## MINE_URANIUM (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `uranium_mine` | Урановый рудник | 33×44, в 26 | 33×44 | 100 | EXPORT:GRAVEL 20 |  | uranium 0.75 | HEATING_DISABLE | пеш 1, дор 1, конвейер 4 |

## MINE_WATER (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `water_well_big` | Большая водозаборная скважина | 15×26, в 14 | 15×26 | 7 | EXPORT:WATER 500 | ; в секунду: eletric 0.095 | water 215 | HEATING_DISABLE | пеш 1, дор 1, вода вых 6 |
| `water_well_small` | Малая водозаборная скважина | 10×17, в 13 | 10×17 |  | EXPORT:WATER 280 | ; в секунду: eletric 0.035 | water 70 |  | дор 1, вода вых 2 |

## MINE_WATER_SURFACE (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `water_to_water` | Поверхностный водозабор | 11×4, в 2 | 11×4 |  | EXPORT:WATER 3 |  | water 150 | ELECTRIC_DISABLE | пеш 1, вода вых 1 |

## MINE_WOOD (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `woodcutting_post` | Лесозаготовительная база | 27×28, в 7 | 28×36 | 10 | EXPORT:OPEN 50, FUEL:OIL 20 |  | wood 6.25 | HEATING_DISABLE | пеш 5, дор 1 |

## MONUMENT (10)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `co_museum_pyramid` | Музей "Пирамида" | 81×106, в 24 | 105×106 | 20 |  |  |  | CITIZEN_ABLE_SERVE 5, ATTRACTIVE_SCORE 2.8, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2 | пеш 2, дор 1 |
| `fountain1` | Большой фонтан | 44×32, в 6 | 44×33 |  |  |  |  | SEWAGE_DISABLE | пеш 2 |
| `fountain2` | Средний фонтан | 29×26, в 2 | 29×26 |  |  |  |  | SEWAGE_DISABLE | пеш 2 |
| `fountain3` | Малый фонтан | 16×16, в 2 | 16×16 |  |  |  |  | SEWAGE_DISABLE | пеш 4 |
| `monument_emblem` | Советский герб | 8×23, в 20 | 8×23 |  |  |  |  | MONUMENT_GOVERNMENT_LOYALTY_RADIUS 245 | пеш 2 |
| `monument_molotok1` | Серп и молот из бронзы и бетона | 13×4, в 11 | 13×5 |  |  |  |  | MONUMENT_GOVERNMENT_LOYALTY_RADIUS 165 | пеш 2 |
| `monument_molotok2` | Серп и молот из бетона | 26×10, в 25 | 26×10 |  |  |  |  | MONUMENT_GOVERNMENT_LOYALTY_RADIUS 350 | пеш 2 |
| `monument_star` | Красная звезда | 20×10, в 30 | 20×11 |  |  |  |  | MONUMENT_GOVERNMENT_LOYALTY_RADIUS 250 | пеш 2 |
| `monument_statue_big` | Бетонный памятник Ленину | 11×10, в 24 | 11×11 |  |  |  |  | MONUMENT_GOVERNMENT_LOYALTY_RADIUS 190 | пеш 2 |
| `monument_statue_small` | Бронзовый памятник Ленину | 7×7, в 13 | 7×7 |  |  |  |  | MONUMENT_GOVERNMENT_LOYALTY_RADIUS 120 | пеш 4 |

## ORPHANAGE (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `orphanage` | Детский дом | 27×85, в 21 | 30×85 | 40 |  |  |  | CITIZEN_ABLE_SERVE 4, QUALITY_OF_LIVING 0.78 | пеш 5, дор 1 |

## PARKING (8)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `muddy_parking` | Стоянка (бесплатная) | 12×15, в 1 | 13×20 |  |  |  |  | COUNT_LIMIT 5, WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |
| `parking_big` | Большая стоянка | 23×30, в 10 | 23×30 |  |  |  |  |  | пеш 6, дор 1 |
| `parking_medium` | Средняя стоянка | 22×18, в 10 | 24×18 |  |  |  |  |  | пеш 4, дор 2 |
| `parking_small` | Малая стоянка | 17×9, в 7 | 17×9 |  |  |  |  |  | пеш 4, дор 1 |
| `parkingoptimized_big` | Большая стоянка (улучшенное движение) | 41×34, в 10 | 44×34 |  |  |  |  |  | пеш 6, дор 1 |
| `parkingoptimized_medium` | Средняя стоянка (улучшенное движение) | 32×38, в 10 | 32×38 |  |  |  |  |  | дор 1 |
| `parkingoptimized_medium_v2` | Средняя стоянка (улучшенное движение) | 32×32, в 10 | 32×32 |  |  |  |  |  | пеш 7, дор 1 |
| `parkingoptimized_small` | Малая стоянка (улучшенное движение) | 30×18, в 10 | 30×18 |  |  |  |  |  | пеш 2, дор 2 |

## PASSANGER_STATION (25)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `airport_terminal` | Пассажирский терминал | 106×116, в 21 | 106×116 |  | PASSANGER 1200, FUEL:OIL 60 |  |  | HEATING_ENABLE | пеш 3, дор 1, труба 2 |
| `airport_terminal_small` | Пассажирский терминал | 59×30, в 9 | 59×30 |  | PASSANGER 400, FUEL:OIL 20 |  |  | HEATING_ENABLE | пеш 4, дор 1, труба 2 |
| `busstop` | Автобусная остановка | 32×20, в 3 | 32×21 |  | PASSANGER 200 |  |  |  | пеш 4, дор 2 |
| `busstop_t` | Троллейбусная остановка | 33×20, в 11 | 33×21 |  | PASSANGER 200 |  |  |  | пеш 4, дор 2 |
| `cableway_station_big` | Большая станция канатной дороги | 60×38, в 14 | 60×38 |  | PASSANGER 500, GENERAL 1, COOLER 1 |  |  |  | пеш 5, дор 2 |
| `cableway_station_small` | Малая станция канатной дороги | 26×26, в 18 | 26×33 |  | PASSANGER 250, GENERAL 1, COOLER 1 |  |  |  | пеш 2, дор 1 |
| `harbor_passanger_medium` | Средний пассажирский порт | 149×107, в 4 | 149×107 |  | PASSANGER 1000, FUEL:OIL 60 |  |  |  | пеш 2, дор 1, труба 1 |
| `harbor_passanger_small` | Малый пассажирский порт | 72×54, в 4 | 72×54 |  | PASSANGER 1000, FUEL:OIL 40 |  |  |  | пеш 2, дор 1, труба 1 |
| `metro_strasicka` | Станция метро с вестибюлем | 36×99, в 14 | 36×99 |  | PASSANGER 600 |  |  |  | пеш 7, дор 1 |
| `metro_zelivskeho` | Станция метро без вестибюля | 22×102, в 2 | 22×103 |  | PASSANGER 800 |  |  |  | пеш 4, дор 2 |
| `metro_zelivskehp` | Станция метро без вестибюля | 22×102, в 2 | 22×103 |  | PASSANGER 800 |  |  |  | пеш 4, дор 2 |
| `metro_zelivskehq` | Станция метро без вестибюля | 22×102, в 2 | 22×103 |  | PASSANGER 800 |  |  |  | пеш 4, дор 2 |
| `muddy_busstop` | Автобусная остановка (бесплатная) | 34×22, в 2 | 53×22 |  | PASSANGER 100 |  |  | COUNT_LIMIT 15, WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | пеш 4, дор 2 |
| `passengerheliport` | Пассажирский вертодром | 63×56, в 13 | 63×56 |  | PASSANGER 250, FUEL:OIL 30 |  |  | HEATING_ENABLE | пеш 4, дор 1, труба 1 |
| `rail_station_passanger` | Железнодорожная платформа | 85×36, в 6 | 85×37 |  | PASSANGER 1200 |  |  |  | пеш 4, дор 2, жд 4 |
| `rail_station_passanger_small` | Малая железнодорожная платформа | 38×36, в 6 | 40×37 |  | PASSANGER 500 |  |  |  | пеш 2, дор 2, жд 4 |
| `railstation_aboveground` | Малая наземная станция метро | 20×40, в 16 | 20×40 |  | PASSANGER 400 |  |  |  | пеш 6 |
| `railstation_aboveground_big` | Большая наземная станция метро | 20×80, в 16 | 21×80 |  | PASSANGER 900 |  |  |  | пеш 6, дор 4 |
| `railstation_groundlevel` | Малая наземная станция метро | 20×40, в 4 | 20×40 |  | PASSANGER 400 |  |  |  | пеш 2, жд 4 |
| `railstation_groundlevel_big` | Большая наземная станция метро | 20×80, в 4 | 20×80 |  | PASSANGER 900 |  |  |  | пеш 2, жд 4 |
| `road_busstation` | Автостанция | 38×34, в 6 | 38×34 |  | PASSANGER 500 |  |  |  | пеш 2, дор 2 |
| `road_busstation_small` | Малая автостанция | 20×34, в 6 | 22×34 |  | PASSANGER 230 |  |  |  | пеш 2, дор 2 |
| `road_busstation_v2` | Автостанция | 40×55, в 6 | 41×55 |  | PASSANGER 500 |  |  |  | пеш 6, дор 1 |
| `tram_stop_big` | Большая трамвайная остановка | 42×21, в 4 | 55×24 |  | PASSANGER 400 |  |  |  | пеш 4, дор 2 |
| `tram_stop_small` | Малая трамвайная остановка | 27×11, в 3 | 37×11 |  | PASSANGER 250 |  |  |  | пеш 4, дор 2 |

## PEDESTRIAN_BRIDGE (18)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `fence_gate` | КПП | 15×20, в 4 | 16×22 |  |  |  |  |  | дор 2 |
| `fence_gate_park_vases` | Парковые ворота с вазами | 5×20, в 7 | 8×20 |  |  |  |  |  | дор 2 |
| `fence_gate_rail` | Железнодорожные ворота | 11×28, в 11 | 12×28 |  |  |  |  |  | пеш 1, дор 2, жд 4 |
| `gate_brick` | КПП | 3×6, в 4 | 6×6 |  |  |  |  |  | пеш 2 |
| `gate_iron` | КПП | 2×6, в 3 | 6×6 |  |  |  |  |  | пеш 2 |
| `gate_mesh` | КПП | 2×5, в 2 | 6×5 |  |  |  |  |  | пеш 2 |
| `gate_plank` | КПП | 2×5, в 2 | 6×5 |  |  |  |  |  | пеш 2 |
| `metro_vchod_eskalator` | Вход на станцию метро (с эскалатором) | 13×18, в 3 | 13×18 |  |  |  |  |  | пеш 4 |
| `metro_vchod_eskalator_new` | Вход на станцию метро (с эскалатором) | 7×18, в 3 | 7×18 |  |  |  |  |  | пеш 4 |
| `metro_vchod_eskalator_nex` | Вход на станцию метро (с эскалатором) | 7×22, в 3 | 7×22 |  |  |  |  |  | пеш 4 |
| `metro_vchod_eskalator_ney` | Вход на станцию метро (с эскалатором) | 7×27, в 3 | 7×27 |  |  |  |  |  | пеш 4 |
| `metro_vchod_schody` | Вход на станцию метро (с лестницей) | 10×17, в 3 | 10×17 |  |  |  |  |  | пеш 4 |
| `metro_vchod_schody_new` | Вход на станцию метро (с лестницей) | 5×17, в 3 | 5×17 |  |  |  |  |  | пеш 4 |
| `metro_vchod_schody_nex` | Вход на станцию метро (с лестницей) | 5×22, в 3 | 5×22 |  |  |  |  |  | пеш 4 |
| `metro_vchod_schody_ney` | Вход на станцию метро (с лестницей) | 5×26, в 3 | 5×26 |  |  |  |  |  | пеш 4 |
| `pedestrian_bridge_concrete` | Бетонный надземный переход | 2×13, в 6 | 3×18 |  |  |  |  |  | пеш 4 |
| `pedestrian_bridge_steel` | Стальной надземный переход | 2×14, в 7 | 3×18 |  |  |  |  |  | пеш 4 |
| `pedestrian_tunel_entry` | Подземный переход | 7×16, в 3 | 7×16 |  |  |  |  |  | пеш 4 |

## POLICE_STATION (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `police` | Отделение милиции | 55×47, в 41 | 56×51 | 40 +40 с высш. | FUEL:OIL 40 |  |  |  | пеш 4, дор 1 |
| `police_small` | Опорный пункт милиции | 29×27, в 11 | 29×27 | 12 +12 с высш. | FUEL:OIL 15 |  |  |  | пеш 3, дор 1 |

## POLLUTION_METER (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `pollution_meter` | Станция контроля загрязнения | 8×7, в 16 | 8×7 |  |  |  |  |  | пеш 1 |

## POWERPLANT (9)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `incinerator_powerplant` | Мусоросжигательная электростанция | 50×67, в 50 | 53×67 | 20 | IMPORT:WASTE 30 | waste 3 | eletric 33 | HEATING_DISABLE | пеш 5, дор 1, эл ср вых 3, эл выс вых 1 |
| `powerplant_coal` | Тепловая электростанция | 133×86, в 89 | 136×86 | 20 |  | coal 1.2 | eletric 70 | OBSOLETE, HEATING_DISABLE | дор 1, эл ср вых 4, эл выс вых 2, конвейер 3 |
| `powerplant_coal_v2` | Тепловая электростанция | 133×86, в 89 | 133×86 | 20 |  | coal 1.2 | eletric 70 | HEATING_DISABLE | пеш 3, дор 1, эл ср вых 4, эл выс вых 2, конвейер 3 |
| `powerplant_gas` | Газотурбинная электростанция | 108×112, в 53 | 109×112 | 15 |  | oil 0.59 | eletric 70 | HEATING_DISABLE | пеш 4, дор 1, эл ср вых 4, эл выс вых 2, труба 6 |
| `powerplant_nuclear_double` | Двухблочная атомная электростанция | 210×190, в 127 | 211×192 | 80 +40 с высш. | IMPORT:NUCLEAR2 3, IMPORT:VEHICLES 15, EXPORT:NUCLEAR2 3, EXPORT:VEHICLES 15, IMPORT:WATER 50, EXPORT:SEWAGE 50 | nuclearfuel 0.001, vehicles 1, water 0.17 | eletric 117, nuclearfuelburned 0.0005, vehicles 1 | CONSUMPTION_WATER_REQUIRED_QUALITY 0.6, HEATING_DISABLE | пеш 3, дор 1, вода вх 2, канал вых 2, эл ср вых 6, эл выс вых 9 |
| `powerplant_nuclear_single` | Одноблочная атомная электростанция | 165×173, в 132 | 181×174 | 40 +20 с высш. | IMPORT:NUCLEAR2 3, IMPORT:VEHICLES 15, EXPORT:NUCLEAR2 3, EXPORT:VEHICLES 15, IMPORT:WATER 25, EXPORT:SEWAGE 25 | nuclearfuel 0.001, vehicles 1, water 0.17 | eletric 117, nuclearfuelburned 0.0005, vehicles 1 | CONSUMPTION_WATER_REQUIRED_QUALITY 0.6, HEATING_DISABLE | пеш 2, дор 1, вода вх 2, канал вых 1, эл ср вых 6, эл выс вых 7 |
| `powerplant_solar` | Солнечная электростанция | 271×243, в 126 | 271×243 | 8 |  |  | eletric 70 | HEATING_DISABLE | пеш 2, дор 1, эл ср вых 10, эл выс вых 1 |
| `powerplant_wind1` | Малый ветрогенератор | 22×5, в 25 | 22×7 |  |  |  | eletric 15 |  | дор 1, эл ср вых 1 |
| `powerplant_wind2` | Большой ветрогенератор | 48×8, в 49 | 48×10 |  |  |  | eletric 35 |  | дор 1, эл ср вых 1 |

## PRISON (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `prison` | Тюрьма | 104×98, в 28 | 107×99 | 70 |  |  |  | CITIZEN_ABLE_SERVE 3, QUALITY_OF_LIVING 0.5 | пеш 7, дор 1 |

## PRODUCTION_LINE (5)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `drydock` | Большой сухой док | 66×282, в 47 | 68×282 | 400 +100 с высш. | EXPORT:VEHICLES 15 | steel 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | vehicles 1 |  | дор 1 |
| `production_airplane` | Авиастроительный завод | 214×235, в 66 | 216×235 | 500 +300 с высш. |  | steel 1, aluminium 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | vehicles 1 | HEATING_DISABLE | пеш 7, дор 1, эл выс вых 1 |
| `production_train` | Железнодорожный завод | 238×110, в 66 | 240×114 | 400 +100 с высш. | EXPORT:VEHICLES 120 | steel 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | vehicles 1 | OBSOLETE, HEATING_DISABLE | пеш 5, дор 1, жд 4 |
| `production_train2` | Железнодорожный завод | 209×141, в 66 | 214×141 | 400 +100 с высш. | EXPORT:VEHICLES 120 | steel 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | vehicles 1 | HEATING_DISABLE | пеш 6, дор 1, эл выс вых 1, жд 4 |
| `production_vehicle` | Автозавод | 181×106, в 60 | 182×107 | 400 +100 с высш. | EXPORT:VEHICLES 15 | steel 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | vehicles 1 | HEATING_DISABLE | пеш 6, дор 1, эл выс вых 1 |

## PUB (4)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `co_beach_restaurant` | Пляжный кафе-бар | 27×22, в 5 | 28×28 | 7 | SPECIAL:COVERED 4 |  |  | ATTRACTIVE_SCORE 2.4, ELETRIC_WITHOUT_WORKING_FACTOR 0.5, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.4, CITIZEN_ABLE_SERVE 10 | пеш 5, дор 1 |
| `co_cafe_pearl` | Кафе-бар "Кругозор" | 46×36, в 15 | 47×36 | 8 | SPECIAL:COVERED 15 |  |  | ATTRACTIVE_SCORE 3.6, ELETRIC_WITHOUT_WORKING_FACTOR 0.5, ELETRIC_WITHOUT_LIGHTING_FACTOR 0, CITIZEN_ABLE_SERVE 7 | пеш 5, дор 1 |
| `kiosk_alcohol` | Пивной ларек | 5×4, в 3 | 8×9 | 1 | SPECIAL:COVERED 4 |  |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2, CITIZEN_ABLE_SERVE 12 | пеш 3, дор 1 |
| `pub` | Бар | 20×21, в 8 | 20×23 | 5 | SPECIAL:COVERED 15 |  |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2, CITIZEN_ABLE_SERVE 12 | пеш 4, дор 1 |

## RAILDEPO (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `rail_depo` | Депо | 44×63, в 16 | 52×64 |  |  |  |  |  | дор 1, жд 6 |
| `raildepobig` | Депо для длинных поездов | 110×41, в 17 | 110×51 |  |  |  |  |  | дор 1, жд 6 |

## RAIL_TRAFO (6)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `metro_trafo_1` | Тяговая подстанция метрополитена | 12×25, в 13 | 12×25 |  |  |  |  |  | дор 1, эл ср вх 4 |
| `metro_trafo_2` | Тяговая подстанция метрополитена | 12×25, в 13 | 12×25 |  |  |  |  |  | дор 1, эл ср вх 4 |
| `metro_trafo_3` | Тяговая подстанция метрополитена | 12×25, в 13 | 12×25 |  |  |  |  |  | дор 1, эл ср вх 4 |
| `rail_trafo` | Железнодорожная тяговая подстанция | 21×25, в 13 | 23×25 |  |  |  |  |  | дор 1, эл ср вх 4, жд 4 |
| `road_trafo` | Троллейбусная тяговая подстанция | 21×14, в 12 | 23×14 |  |  |  |  |  | дор 2, эл ср вх 2 |
| `tram_trafo` | Трамвайная тяговая подстанция | 21×14, в 12 | 23×24 |  |  |  |  |  | дор 2, эл ср вх 2 |

## REPAIR_OFFICE (4)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `garage_big` | Большая станция техобслуживания | 73×106, в 9 | 73×107 | 80 | FUEL:OIL 40 |  |  | REPAIR_AREA 25, HEATING_DISABLE | пеш 8, дор 1 |
| `garage_small` | Средняя станция техобслуживания | 59×63, в 9 | 59×64 | 30 | FUEL:OIL 20 |  |  | REPAIR_AREA 25, HEATING_DISABLE | пеш 6, дор 1 |
| `garage_tiny` | Малая станция техобслуживания | 53×45, в 9 | 53×46 | 15 | FUEL:OIL 15 |  |  | REPAIR_AREA 25, HEATING_DISABLE | пеш 5, дор 1 |
| `repair_service_office` | Строительная контора | 46×40, в 7 | 46×40 | 50 | FUEL:OIL 30 |  |  | REPAIR_AREA 25 | дор 1 |

## ROADDEPO (6)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `muddy_depot` | Автопарк (бесплатный) | 19×24, в 2 | 25×26 |  |  |  |  | COUNT_LIMIT 7, WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |
| `road_depo` | Автопарк | 41×39, в 11 | 41×39 |  |  |  |  |  | дор 2 |
| `road_depo_big` | Автопарк | 63×43, в 11 | 63×43 |  |  |  |  |  | дор 1 |
| `tram_depo_big` | Трамвайное депо (8 мест) | 85×49, в 10 | 91×49 |  |  |  |  |  | дор 1 |
| `tram_depo_small` | Трамвайное депо (4 места) | 86×30, в 10 | 96×30 |  |  |  |  |  | дор 1 |
| `trolleybus_depo` | Троллейбусный парк | 45×40, в 11 | 46×40 |  |  |  |  |  | дор 2 |

## SCHOOL (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `school` | Школа | 43×28, в 9 | 43×28 | 10 +15 с высш. |  |  |  | CITIZEN_ABLE_SERVE 12, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 6 |

## SCRAPYARD (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `scrapyard` | Уранообогатительный комбинат | 61×84, в 41 | 76×85 | 80 | IMPORT:VEHICLES 15, EXPORT:WASTE 50 |  |  |  | пеш 2, дор 1, жд 2 |
| `scrapyard_ship` | Уранообогатительный комбинат | 280×182, в 11 | 280×182 | 150 | EXPORT:WASTE 100 |  |  |  | дор 1 |

## SECRET_POLICE (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `secretpolice` | Здание госбезопасности | 69×41, в 32 | 69×42 |  +30 с высш. | FUEL:OIL 40 |  |  |  | пеш 4, дор 1 |

## SEWAGE_DISCHARGE (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `sewage_discharge` | Точка сброса | 15×27, в 7 | 15×27 |  | IMPORT:SEWAGE 10 |  |  |  | пеш 1, дор 1, канал вх 3 |

## SEWAGE_ENDSTATION (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `sewage_substation` | Резервуар для сточных вод | 5×5, в 3 | 8×5 |  | SEWAGE 12 |  |  |  | пеш 1, канал вых 1 |

## SEWAGE_PUMP (5)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `sewage_pump` | Канализационная насосная станция | 1×2, в 0 | 6×10 |  | SEWAGE 1 |  |  |  | пеш 1, канал вх 1, канал вых 1 |
| `sewage_pump_1` | Канализационная насосная станция | 1×2, в 0 | 6×10 |  | SEWAGE 1 |  |  |  | пеш 1, канал вх 1, канал вых 1 |
| `sewage_pump_2` | Канализационная насосная станция | 1×2, в 0 | 6×10 |  | SEWAGE 1 |  |  |  | пеш 1, канал вх 1, канал вых 1 |
| `sewage_pump_3` | Канализационная насосная станция | 1×2, в 0 | 6×10 |  | SEWAGE 1 |  |  |  | пеш 1, канал вх 1, канал вых 1 |
| `sewage_switch` | Канализационный узловой колодец | 2×2, в 0 | 10×10 |  | SEWAGE 1 |  |  |  | пеш 1, канал вх 3, канал вых 1 |

## SEWAGE_TREATMENT (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `sewage_treatment_big` | Большая очистная станция | 192×151, в 18 | 192×151 | 20 | IMPORT:SEWAGE 300, EXPORT:SEWAGE 50 | usagewater 11, chemicals 0.03; в секунду: eletric 0.154 | water 22 | HEATING_DISABLE | пеш 5, дор 1, канал вх 4, канал вых 6 |
| `sewage_treatment_small` | Малая очистная станция | 92×72, в 18 | 93×72 | 10 | IMPORT:SEWAGE 100, EXPORT:SEWAGE 25 | usagewater 8, chemicals 0.032; в секунду: eletric 0.085 | water 16.5 | HEATING_DISABLE | пеш 3, дор 1, канал вх 2, канал вых 2 |

## SHIP_DOCK (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `ship_dock` | Средний док | 280×182, в 11 | 280×182 |  |  |  |  |  | дор 1 |
| `ship_dock_small` | Малый док | 177×114, в 11 | 177×114 |  |  |  |  |  | дор 1 |

## SHOP (8)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `co_look_out_cafe` | Ресторан "Панорама" | 54×32, в 54 | 55×32 | 25 | DEMAND_BASIC:COVERED 10, DEMAND_BASIC:COOLER 4 |  |  | ATTRACTIVE_SCORE 4.8, ELETRIC_WITHOUT_WORKING_FACTOR 0.5, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2, CITIZEN_ABLE_SERVE 2 | пеш 1, дор 1 |
| `kiosk_food` | Бакалейный ларек | 3×4, в 3 | 8×9 | 1 | DEMAND_BASIC:COVERED 4, DEMAND_BASIC:COOLER 0.5 |  |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.2, CITIZEN_ABLE_SERVE 10 | пеш 3, дор 1 |
| `shop_beta` | Малый универмаг | 58×27, в 9 | 58×27 | 30 | DEMAND_ADVANCED:COVERED 30, DEMAND_BASIC:COOLER 5 |  |  | CITIZEN_ABLE_SERVE 6, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 6, дор 1 |
| `shop_clothes` | Малый магазин | 23×23, в 7 | 26×24 | 15 |  |  |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.1, CITIZEN_ABLE_SERVE 7 | пеш 3, дор 1 |
| `shop_grocerystore` | Гастроном | 23×23, в 7 | 26×24 | 15 | DEMAND_BASIC:COVERED 20, DEMAND_BASIC:COOLER 4 |  |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0.1, CITIZEN_ABLE_SERVE 7 | пеш 3, дор 1 |
| `shop_lipa` | Малый универмаг | 38×28, в 13 | 45×30 | 25 | DEMAND_ADVANCED:COVERED 25, DEMAND_BASIC:COOLER 4 |  |  | CITIZEN_ABLE_SERVE 6, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 3, дор 1 |
| `shop_mier` | Малый универмаг | 69×21, в 10 | 69×21 | 25 | DEMAND_ADVANCED:COVERED 25, DEMAND_BASIC:COOLER 4 |  |  | CITIZEN_ABLE_SERVE 6, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 6, дор 1 |
| `shop_prior` | Универмаг | 40×84, в 38 | 46×84 | 45 | DEMAND_ADVANCED:COVERED 70, DEMAND_BASIC:COOLER 12 |  |  | ATTRACTIVE_SCORE 1.5, CITIZEN_ABLE_SERVE 9, ELETRIC_WITHOUT_WORKING_FACTOR 0.7, ELETRIC_WITHOUT_LIGHTING_FACTOR 0 | пеш 4, дор 1 |

## SPORT (3)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `playground_football` | Футбольное поле | 29×19, в 3 | 29×19 | 1 |  |  |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.95, ELETRIC_WITHOUT_LIGHTING_FACTOR 0, CITIZEN_ABLE_SERVE 22, WATERSEWAGE_DISABLE, HEATING_DISABLE | пеш 4 |
| `playground_tenis` | Теннисный корт | 24×49, в 10 | 24×49 | 1 |  |  |  | CITIZEN_ABLE_SERVE 5, WATERSEWAGE_DISABLE, HEATING_DISABLE | пеш 4 |
| `playground_tenis2` | Теннисный корт | 14×29, в 6 | 16×29 | 1 |  |  |  | ELETRIC_WITHOUT_WORKING_FACTOR 0.9, ELETRIC_WITHOUT_LIGHTING_FACTOR 0, CITIZEN_ABLE_SERVE 5, WATERSEWAGE_DISABLE, HEATING_DISABLE | пеш 4 |

## STORAGE (27)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `alumina_storage` | Хранилище насыпных грузов | 31×48, в 26 | 31×48 |  | CEMENT 1150 |  |  |  | дор 1 |
| `containerstorage_big` | Большая мусоропогрузочная площадка | 33×28, в 2 | 36×30 |  |  |  |  |  | дор 1 |
| `containerstorage_small` | Малая мусоропогрузочная площадка | 22×17, в 2 | 24×19 |  |  |  |  |  | дор 1 |
| `livestock_storage` | Загон для скота | 41×52, в 15 | 41×52 |  | LIVESTOCK 300 |  |  | CONSUMPTION_WATER_REQUIRED_QUALITY 0.93 | дор 1 |
| `muddy_gravelstorage` | Хранилище навалочных грузов (бесплатное) | 31×35, в 2 | 38×35 |  | GRAVEL 180 |  |  | WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |
| `muddy_landfill` | Свалка (бесплатная) | 40×40, в 1 | 46×47 |  | WASTE 288 |  |  | ELECTRIC_DISABLE | дор 1 |
| `muddy_openstorage` | Открытое хранилище (бесплатное) | 31×34, в 3 | 38×35 |  | OPEN 95 |  |  | WATERSEWAGE_DISABLE, ELECTRIC_DISABLE, HEATING_DISABLE | дор 1 |
| `oil_storage` | Резервуар | 42×36, в 13 | 42×36 |  | OIL 1500 |  |  |  | дор 1, труба 4 |
| `oil_storage2` | Резервуар | 8×8, в 10 | 8×10 |  | OIL 90 |  |  |  | дор 1, труба 3 |
| `oil_storage3` | Резервуар | 31×9, в 15 | 31×11 |  | OIL 300 |  |  |  | дор 1, труба 6 |
| `spaceforvehicles_big` | Большая площадка для техники/контейнеров | 100×29, в 24 | 100×29 |  | VEHICLES 150 |  |  |  | дор 1, жд 2 |
| `spaceforvehicles_small` | Малая площадка для техники/контейнеров | 60×16, в 8 | 61×17 |  | VEHICLES 15 |  |  |  | дор 1 |
| `storage_cement` | Цементохранилище | 31×42, в 46 | 32×42 |  | CEMENT 1400 |  |  |  | дор 1 |
| `storage_cement_v2` | Цементохранилище | 31×42, в 46 | 32×43 |  | SPECIAL:CEMENT 1400 |  |  |  | дор 1 |
| `storage_grain` | Элеватор | 19×38, в 48 | 19×39 |  | SPECIAL:COVERED 1000 |  |  |  | дор 1 |
| `storage_meat` | Рефрижератор | 14×15, в 11 | 16×24 |  | COOLER 150 |  |  |  | дор 1 |
| `storage_rumble_big` | Большое хранилище навалочных грузов | 92×48, в 16 | 92×48 |  | GRAVEL 1950 |  |  |  | дор 1, конвейер 10 |
| `storage_rumble_small` | Малое хранилище навалочных грузов | 42×43, в 16 | 42×43 |  | GRAVEL 870 |  |  |  | дор 1, конвейер 6 |
| `warehouse_covered_combined` | Склад | 31×75, в 14 | 31×75 |  | COVERED 650 |  |  |  | дор 1, жд 2 |
| `warehouse_open` | Малое открытое хранилище | 41×41, в 18 | 41×41 |  | OPEN 330 |  |  |  | дор 1 |
| `warehouse_open_combined` | Открытое хранилище | 41×41, в 18 | 42×41 |  | OPEN 570 |  |  |  | дор 1, жд 2 |
| `warehouse_open_combined_big` | Среднее открытое хранилище | 82×41, в 18 | 83×41 |  | OPEN 1170 |  |  |  | дор 1, жд 2 |
| `waste_storage_basic_big` | Средняя свалка | 35×35, в 2 | 35×35 |  | WASTE 220 |  |  | ELECTRIC_DISABLE | дор 1 |
| `waste_storage_basic_small` | Малая свалка | 20×20, в 2 | 20×20 |  | WASTE 72 |  |  | ELECTRIC_DISABLE | дор 1 |
| `waste_storage_crane_big` | Большая свалка с грейферным краном | 80×32, в 19 | 80×32 |  | WASTE 360 |  |  |  | дор 1 |
| `waste_storage_crane_big_rail` | Свалка с грейферным краном и ж/д путями | 100×37, в 19 | 100×37 |  | WASTE 450 |  |  |  | дор 1, жд 4 |
| `waste_storage_crane_small` | Малая свалка с грейферным краном | 40×32, в 19 | 40×32 |  | WASTE 180 |  |  |  | дор 1 |

## SUBSTATION (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `eletric_substation` | Трансформаторный пункт | 9×6, в 5 | 9×6 |  |  |  |  |  | дор 1, эл ср вх 3 |

## TRAM_GATE (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `tram_gate` | Трамвайно-железнодорожное соединение | 45×11, в 2 | 45×11 |  |  |  |  |  |  |

## TRANSFORMATOR (7)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `eletric_switch_high` | Распределительное устройство высокого напряжения | 23×26, в 13 | 25×28 |  |  |  |  |  | дор 1, эл выс вх 3 |
| `eletric_switch_high_big` | Распределительное устройство высокого напряжения (6х) | 23×38, в 13 | 23×39 |  |  |  |  |  | дор 1, эл выс вх 6 |
| `eletric_switch_high_priority` | Регулируемое распределительное устройство высокого напряжения | 23×38, в 13 | 23×39 |  |  |  |  |  | дор 1, эл выс вх 3, эл выс вых 3 |
| `eletric_switch_low` | Распределительное устройство среднего напряжения | 17×14, в 13 | 17×15 |  |  |  |  |  | дор 1, эл ср вх 3 |
| `eletric_switch_low_big` | Распределительное устройство среднего напряжения (5х) | 17×26, в 13 | 17×28 |  |  |  |  |  | дор 1, эл ср вх 5 |
| `eletric_switch_low_priority` | Регулируемое распределительное устройство среднего напряжения | 17×26, в 13 | 17×28 |  |  |  |  |  | дор 1, эл ср вх 2, эл ср вых 2 |
| `eletric_transformator` | Подстанция | 23×54, в 33 | 26×54 |  |  |  |  |  | дор 2, эл ср вых 6, эл выс вх 1 |

## TRASH_CONTAINER (6)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `bigcontainerstand` | Малая площадка для больших мусорных контейнеров | 12×12, в 2 | 16×12 |  |  |  |  | ELECTRIC_DISABLE | дор 1 |
| `bigcontainerstand_big` | Большая площадка для больших мусорных контейнеров | 24×24, в 2 | 24×24 |  |  |  |  | ELECTRIC_DISABLE | дор 1 |
| `containerstand_big` | Большая площадка для малых мусорных контейнеров | 12×12, в 2 | 13×12 |  |  |  |  | ELECTRIC_DISABLE | дор 1 |
| `containerstand_big_pede` | Большая площадка для малых мусорных контейнеров | 12×12, в 2 | 12×12 |  |  |  |  | ELECTRIC_DISABLE | пеш 1 |
| `containerstand_small` | Малая площадка для малых мусорных контейнеров | 6×7, в 2 | 16×7 |  |  |  |  | ELECTRIC_DISABLE | дор 1 |
| `containerstand_small_pede` | Малая площадка для малых мусорных контейнеров | 9×7, в 2 | 15×7 |  |  |  |  | ELECTRIC_DISABLE | пеш 1 |

## UNIVERSITY (6)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `university_medical` | Медицинский университет | 117×105, в 61 | 119×105 | 75 +75 с высш. |  |  |  | CITIZEN_ABLE_SERVE 3 | пеш 2, дор 1 |
| `university_medical_v2` | Медицинский университет | 117×105, в 61 | 117×105 | 50 +50 с высш. |  |  |  | CITIZEN_ABLE_SERVE 6 | пеш 2, дор 1 |
| `university_soviet` | Партийная академия | 104×136, в 42 | 104×136 | 100 +100 с высш. |  |  |  | CITIZEN_ABLE_SERVE 3 | пеш 9, дор 2 |
| `university_soviet_v2` | Партийная академия | 104×136, в 42 | 104×136 | 70 +70 с высш. |  |  |  | CITIZEN_ABLE_SERVE 6 | пеш 8, дор 1 |
| `university_technical` | Технический университет | 155×41, в 34 | 159×42 | 75 +75 с высш. |  |  |  | CITIZEN_ABLE_SERVE 3 | пеш 6, дор 1 |
| `university_technical_v2` | Технический университет | 155×41, в 34 | 155×42 | 75 +75 с высш. |  |  |  | CITIZEN_ABLE_SERVE 3 | пеш 7, дор 1 |

## WAITING_STATION (7)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `bus_endstation_big` | Большая конечная автостанция | 40×55, в 5 | 40×55 |  | FUEL:OIL 35 |  |  |  | дор 1 |
| `bus_endstation_big_t` | Конечная троллейбусная остановка | 41×56, в 11 | 41×56 |  |  |  |  |  | дор 1 |
| `bus_endstation_small` | Малая конечная автостанция | 40×29, в 4 | 40×29 |  | FUEL:OIL 35 |  |  |  | дор 1 |
| `metro_endstation` | Конечная станция метро | 16×61, в 4 | 16×61 |  |  |  |  |  | дор 1 |
| `metro_endstation_ground` | Наземная конечная станция метро | 50×15, в 11 | 50×17 |  |  |  |  |  | дор 1, жд 4 |
| `rail_endstation` | Конечная железнодорожная станция | 50×15, в 11 | 50×17 |  | FUEL:OIL 75 |  |  |  | дор 1, жд 4 |
| `tram_endstation` | Конечная трамвайная остановка | 41×58, в 9 | 41×65 |  |  |  |  |  | дор 1 |

## WATER_ENDSTATION (1)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `water_substation` | Водоразборный узел | 5×5, в 5 | 6×6 |  | WATER 20 |  |  |  | пеш 1, вода вх 1 |

## WATER_PUMP (7)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `water_pumping_station_big` | Большая водопроводная насосная станция | 6×8, в 4 | 7×8 |  | WATER 7 |  |  |  | пеш 1, вода вх 3, вода вых 3 |
| `water_pumping_station_small` | Малая водопроводная насосная станция | 5×4, в 4 | 6×8 |  | WATER 2 |  |  |  | пеш 1, вода вх 1, вода вых 1 |
| `water_reservoir_big` | Большая водонапорная башня | 22×22, в 55 | 22×22 |  | WATER 300 |  |  |  | пеш 1, вода вх 2, вода вых 2 |
| `water_reservoir_ground` | Подземный резервуар | 12×12, в 9 | 12×12 |  | WATER 500 |  |  |  | пеш 2, вода вх 2, вода вых 2 |
| `water_reservoir_small` | Малая водонапорная башня | 5×6, в 26 | 6×6 |  | WATER 70 |  |  |  | пеш 1, вода вх 1, вода вых 1 |
| `water_switch` | Водораспределительный узел | 4×4, в 1 | 4×4 |  | WATER 1 |  |  |  | пеш 1, вода вх 1, вода вых 3 |
| `water_switch_test` |  | 2×2, в 0 | 10×10 |  | WATER 1 |  |  |  | пеш 1, вода вх 1, вода вых 3 |

## WATER_TREATMENT (2)

| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |
|---|---|---|---|---|---|---|---|---|---|
| `water_treatment_big` | Большая станция водоподготовки | 47×49, в 18 | 47×49 | 10 | IMPORT:WATER 30, EXPORT:WATER 90 | usagewater 3, chemicals 0.047; в секунду: eletric 0.127 | water 30 | HEATING_DISABLE | пеш 3, дор 1, вода вх 4, вода вых 3 |
| `water_treatment_small` | Малая станция водоподготовки | 29×38, в 13 | 30×38 | 5 | IMPORT:WATER 25, EXPORT:WATER 75 | usagewater 2.6, chemicals 0.05; в секунду: eletric 0.065 | water 24 | HEATING_DISABLE | пеш 2, дор 1, вода вх 2, вода вых 2 |
