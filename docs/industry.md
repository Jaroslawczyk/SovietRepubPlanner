# Промышленность: цепочки производства по данным игры
Сгенерировано `tools/gen_industry.py` из `media_soviet/buildings_types/*.ini` (игра 1.1.1.13). Каждое число — строка ini: `$WORKERS_NEEDED`, `$PRODUCTION <ресурс> <число>`, `$CONSUMPTION <ресурс> <число>`, `$CONSUMPTION_PER_SECOND eletric <число>`.
## Как считает мод (вкладка «Промышленность»)
- **Единица.** Числа `$PRODUCTION`/`$CONSUMPTION` мод считает тоннами на одного рабочего в сутки при 100 % эффективности. Это **гипотеза**: она согласуется с тем, что в ini у всех заводов эти числа малы и стоят рядом с `$WORKERS_NEEDED`, но в игре пока не проверена замером (открытый вопрос, см. ниже). Расчёт цепочки от единицы не зависит: выпуск и потребление берутся в одной и той же мере, поэтому соотношения «сырьё на тонну продукта» и число заводов на заданный выпуск верны при любой общей единице времени, а абсолютные т/сутки — только если гипотеза верна.
- **Здание на ресурс.** Из всех зданий с рабочими, выпускающих ресурс, берётся то, у которого выпуск на рабочего больше.
- **Число зданий** = нужный выпуск в сутки / (выпуск на рабочего × рабочие), округление вверх.
- **Сырьё** каждого звена = потребление на рабочего × рабочие × число зданий; дальше то же для каждого входа. Вход, отмеченный «привозим» (своя шахта, ж/д станция, импорт), в цепочку не раскладывается.
- **Электричество** = `$CONSUMPTION_PER_SECOND eletric` × рабочие (МВт; так же гипотеза о единице).
- Фермы и поля в ini не имеют `$PRODUCTION` с рабочими (урожай зависит от техники и сезона) — калькулятор их не считает.

## Открытые вопросы (проверить в игре)
1. Единица `$PRODUCTION`: тонн на рабочего в сутки? Проверка: построить завод, дождаться полной загрузки, сравнить суточный выпуск в окне здания с `число × рабочие`.
2. Влияние эффективности (образование, лояльность, оборудование) на выпуск — в расчёт не входит.
3. Логистика (склады, станции, транспорт) не моделируется: мод показывает потоки в т/сутки, по ним выбираются склады и подвоз.

## Производители по ресурсу
| ресурс | кто производит |
|---|---|
| `alcohol` | `distillery` |
| `alumina` | `alumina_plant` |
| `aluminium` | `aluminium_plant`, `waste_aluminiumrecycling` |
| `asphalt` | `asphalt_plant` |
| `bauxite` | `bauxite_processing` |
| `bitumen` | `oil_rafinery`, `oil_rafinery_v2` |
| `boards` | `sawmill` |
| `bricks` | `brick_factory`, `brick_factory_v2` |
| `cement` | `cement_plant`, `cement_plant_v2`, `cement_plant_v3` |
| `chemicals` | `chemical_plant` |
| `clothes` | `clothing_factory` |
| `coal` | `coal_processing` |
| `concrete` | `concrete_plant`, `concrete_plant_v2` |
| `ecomponents` | `eletronic_components_factory` |
| `eletric` | `incinerator_powerplant`, `powerplant_coal`, `powerplant_coal_v2`, `powerplant_gas`, `powerplant_nuclear_double`, `powerplant_nuclear_single`, `powerplant_solar`, `powerplant_wind1`, `powerplant_wind2` |
| `eletronics` | `eletronic_factory` |
| `explosives` | `explosive_factory` |
| `fabric` | `fabric_factory` |
| `fertiliser` | `fertilizer` |
| `fertiliser_liquid` | `fertilizer_liquid`, `fertilizer_liquid_v2` |
| `food` | `food_factory` |
| `fuel` | `oil_rafinery`, `oil_rafinery_v2` |
| `gravel` | `gravel_processing`, `gravel_processing_small`, `waste_gravelrecycling` |
| `heat` | `heating_plant_big`, `heating_plant_small`, `incinerator_heat` |
| `iron` | `iron_processing` |
| `livestock` | `animal_farm` |
| `mcomponents` | `mechanical_components_factory` |
| `meat` | `slaughterhouse` |
| `nuclearfuel` | `nuclear_fuel_plant` |
| `nuclearfuelburned` | `powerplant_nuclear_double`, `powerplant_nuclear_single` |
| `oil` | `oil_mine` |
| `plants` | `farm`, `farm_large`, `farm_medium`, `farm_small` |
| `plastics` | `plastics_factory`, `waste_plasticrecycling` |
| `prefabpanels` | `panels_factory`, `panels_factory_v2`, `panels_factory_v3` |
| `rawbauxite` | `bauxite_mine` |
| `rawcoal` | `coal_mine` |
| `rawgravel` | `gravel_mine_big`, `gravelmine` |
| `rawiron` | `iron_mine` |
| `steel` | `steel_mill`, `steel_mill_v2`, `waste_steelrecycling` |
| `uf6` | `uranium_conversion` |
| `uranium` | `uranium_mine` |
| `vehicles` | `drydock`, `nuclear_fuel_plant`, `powerplant_nuclear_double`, `powerplant_nuclear_single`, `production_airplane`, `production_train`, `production_train2`, `production_vehicle`, `uranium_conversion` |
| `water` | `sewage_treatment_big`, `sewage_treatment_small`, `water_to_water`, `water_treatment_big`, `water_treatment_small`, `water_well_big`, `water_well_small` |
| `wood` | `woodcutting_post` |
| `yellowcake` | `uranium_processing` |

## Все производственные здания
| здание | тип | рабочие | выпуск на рабочего | выпуск здания, т/сут | сырьё на рабочего | сырьё здания, т/сут | МВт |
|---|---|---|---|---|---|---|---|
| `alumina_plant` | FACTORY | 370 | alumina 0.09 | alumina 33.3 | bauxite 0.21, coal 0.08, chemicals 0.007, water 0.08 | bauxite 77.7, coal 29.6, chemicals 2.6, water 29.6 | 0.00 |
| `aluminium_plant` | FACTORY | 350 | aluminium 0.087 | aluminium 30.4 | alumina 0.15, chemicals 0.0072 | alumina 52.5, chemicals 2.5 | 822.50 |
| `animal_farm` | FACTORY | 50 | livestock 0.2 | livestock 10.0 | plants 0.4, water 0.25 | plants 20.0, water 12.5 | 0.00 |
| `asphalt_plant` | FACTORY | 5 | asphalt 29 | asphalt 145.0 | gravel 25, bitumen 4, eletric 3 | gravel 125.0, bitumen 20.0, eletric 15.0 | 0.00 |
| `bauxite_mine` | MINE_BAUXITE | 45 | rawbauxite 0.5 | rawbauxite 22.5 | - | - | 0.00 |
| `bauxite_processing` | FACTORY | 25 | bauxite 3 | bauxite 75.0 | rawbauxite 5 | rawbauxite 125.0 | 0.00 |
| `brick_factory` | FACTORY | 75 | bricks 0.68 | bricks 51.0 | coal 0.45 | coal 33.8 | 0.00 |
| `brick_factory_v2` | FACTORY | 75 | bricks 0.68 | bricks 51.0 | coal 0.45 | coal 33.8 | 0.00 |
| `cement_plant` | FACTORY | 30 | cement 2.7 | cement 81.0 | coal 0.75, gravel 7 | coal 22.5, gravel 210.0 | 3.00 |
| `cement_plant_v2` | FACTORY | 30 | cement 2.7 | cement 81.0 | coal 0.75, gravel 7 | coal 22.5, gravel 210.0 | 3.00 |
| `cement_plant_v3` | FACTORY | 30 | cement 2.7 | cement 81.0 | coal 0.75, gravel 7 | coal 22.5, gravel 210.0 | 3.00 |
| `chemical_plant` | FACTORY | 60 | chemicals 0.0135 | chemicals 0.8 | gravel 0.012, wood 0.014, plants 0.013, oil 0.02, water 0.17 | gravel 0.7, wood 0.8, plants 0.8, oil 1.2, water 10.2 | 14.40 |
| `clothing_factory` | FACTORY | 80 | clothes 0.015 | clothes 1.2 | fabric 0.03 | fabric 2.4 | 0.00 |
| `coal_mine` | MINE_COAL | 220 | rawcoal 4.2 | rawcoal 924.0 | - | - | 0.00 |
| `coal_processing` | FACTORY | 30 | coal 8 | coal 240.0 | rawcoal 14 | rawcoal 420.0 | 7.50 |
| `concrete_plant` | FACTORY | 5 | concrete 35 | concrete 175.0 | gravel 27, cement 6, water 17 | gravel 135.0, cement 30.0, water 85.0 | 1.00 |
| `concrete_plant_v2` | FACTORY | 5 | concrete 35 | concrete 175.0 | gravel 27, cement 6, water 17 | gravel 135.0, cement 30.0, water 85.0 | 1.00 |
| `distillery` | FACTORY | 100 | alcohol 0.06 | alcohol 6.0 | plants 0.3, water 0.135 | plants 30.0, water 13.5 | 17.00 |
| `drydock` | PRODUCTION_LINE | 400 | vehicles 1 | vehicles 400.0 | steel 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | steel 400.0, plastics 400.0, mcomponents 400.0, ecomponents 400.0, fabric 400.0, eletronics 400.0 | 0.00 |
| `eletronic_components_factory` | FACTORY | 150 | ecomponents 0.025 | ecomponents 3.8 | plastics 0.01, steel 0.01, chemicals 0.008 | plastics 1.5, steel 1.5, chemicals 1.2 | 0.00 |
| `eletronic_factory` | FACTORY | 150 | eletronics 0.03 | eletronics 4.5 | ecomponents 0.01, plastics 0.015, mcomponents 0.01 | ecomponents 1.5, plastics 2.2, mcomponents 1.5 | 0.00 |
| `explosive_factory` | FACTORY | 75 | explosives 0.015 | explosives 1.1 | chemicals 0.01, gravel 0.03, wood 0.02 | chemicals 0.8, gravel 2.2, wood 1.5 | 0.00 |
| `fabric_factory` | FACTORY | 100 | fabric 0.05 | fabric 5.0 | plants 0.2, chemicals 0.005, water 0.11 | plants 20.0, chemicals 0.5, water 11.0 | 18.00 |
| `farm` | FARM | 0 | plants 0.01 | - | - | - | 0.00 |
| `farm_large` | FARM | 0 | plants 10 | - | - | - | 0.00 |
| `farm_medium` | FARM | 0 | plants 10 | - | - | - | 0.00 |
| `farm_small` | FARM | 0 | plants 10 | - | - | - | 0.00 |
| `fertilizer` | FACTORY | 25 | fertiliser 1 | fertiliser 25.0 | waste_bio 1.2, chemicals 0.003 | waste_bio 30.0, chemicals 0.1 | 0.00 |
| `fertilizer_liquid` | FACTORY | 25 | fertiliser_liquid 0.7 | fertiliser_liquid 17.5 | chemicals 0.0125, water 0.6 | chemicals 0.3, water 15.0 | 0.00 |
| `fertilizer_liquid_v2` | FACTORY | 25 | fertiliser_liquid 0.7 | fertiliser_liquid 17.5 | chemicals 0.0125, water 0.6 | chemicals 0.3, water 15.0 | 0.00 |
| `food_factory` | FACTORY | 170 | food 0.12 | food 20.4 | plants 0.25, water 0.05 | plants 42.5, water 8.5 | 0.00 |
| `gravel_mine_big` | MINE_GRAVEL | 100 | rawgravel 3.5 | rawgravel 350.0 | - | - | 0.00 |
| `gravel_processing` | FACTORY | 15 | gravel 5.5 | gravel 82.5 | rawgravel 8 | rawgravel 120.0 | 6.00 |
| `gravel_processing_small` | FACTORY | 5 | gravel 5.5 | gravel 27.5 | rawgravel 8 | rawgravel 40.0 | 0.75 |
| `gravelmine` | MINE_GRAVEL | 40 | rawgravel 3.5 | rawgravel 140.0 | - | - | 0.00 |
| `heating_plant_big` | HEATING_PLANT | 30 | heat 350 | heat 10500.0 | coal 0.28 | coal 8.4 | 0.00 |
| `heating_plant_small` | HEATING_PLANT | 7 | heat 300 | heat 2100.0 | coal 0.3 | coal 2.1 | 0.00 |
| `incinerator_heat` | HEATING_PLANT | 15 | heat 450 | heat 6750.0 | waste 2.5 | waste 37.5 | 0.00 |
| `incinerator_powerplant` | POWERPLANT | 20 | eletric 33 | eletric 660.0 | waste 3 | waste 60.0 | 0.00 |
| `iron_mine` | MINE_IRON | 250 | rawiron 4 | rawiron 1000.0 | - | - | 0.00 |
| `iron_processing` | FACTORY | 15 | iron 7 | iron 105.0 | rawiron 15 | rawiron 225.0 | 4.20 |
| `mechanical_components_factory` | FACTORY | 150 | mcomponents 0.1 | mcomponents 15.0 | steel 0.15 | steel 22.5 | 0.00 |
| `nuclear_fuel_plant` | FACTORY | 50 | nuclearfuel 0.0019, vehicles 1 | nuclearfuel 0.1, vehicles 50.0 | uf6 0.008, chemicals 0.015, vehicles 1 | uf6 0.4, chemicals 0.8, vehicles 50.0 | 35.00 |
| `oil_mine` | MINE_OIL | 0 | oil 7 | - | - | - | 0.00 |
| `oil_rafinery` | FACTORY | 500 | fuel 0.25, bitumen 0.15 | fuel 125.0, bitumen 75.0 | oil 0.5 | oil 250.0 | 105.00 |
| `oil_rafinery_v2` | FACTORY | 500 | fuel 0.25, bitumen 0.15 | fuel 125.0, bitumen 75.0 | oil 0.5 | oil 250.0 | 105.00 |
| `panels_factory` | FACTORY | 65 | prefabpanels 1.1 | prefabpanels 71.5 | cement 0.15, gravel 1 | cement 9.8, gravel 65.0 | 6.50 |
| `panels_factory_v2` | FACTORY | 65 | prefabpanels 1.1 | prefabpanels 71.5 | cement 0.15, gravel 1 | cement 9.8, gravel 65.0 | 6.50 |
| `panels_factory_v3` | FACTORY | 65 | prefabpanels 1.1 | prefabpanels 71.5 | cement 0.15, gravel 1 | cement 9.8, gravel 65.0 | 6.50 |
| `plastics_factory` | FACTORY | 60 | plastics 0.11 | plastics 6.6 | chemicals 0.05, oil 0.45 | chemicals 3.0, oil 27.0 | 15.60 |
| `powerplant_coal` | POWERPLANT | 20 | eletric 70 | eletric 1400.0 | coal 1.2 | coal 24.0 | 0.00 |
| `powerplant_coal_v2` | POWERPLANT | 20 | eletric 70 | eletric 1400.0 | coal 1.2 | coal 24.0 | 0.00 |
| `powerplant_gas` | POWERPLANT | 15 | eletric 70 | eletric 1050.0 | oil 0.59 | oil 8.8 | 0.00 |
| `powerplant_nuclear_double` | POWERPLANT | 80 | eletric 117, nuclearfuelburned 0.0005, vehicles 1 | eletric 9360.0, nuclearfuelburned 0.0, vehicles 80.0 | nuclearfuel 0.001, vehicles 1, water 0.17 | nuclearfuel 0.1, vehicles 80.0, water 13.6 | 0.00 |
| `powerplant_nuclear_single` | POWERPLANT | 40 | eletric 117, nuclearfuelburned 0.0005, vehicles 1 | eletric 4680.0, nuclearfuelburned 0.0, vehicles 40.0 | nuclearfuel 0.001, vehicles 1, water 0.17 | nuclearfuel 0.0, vehicles 40.0, water 6.8 | 0.00 |
| `powerplant_solar` | POWERPLANT | 8 | eletric 70 | eletric 560.0 | - | - | 0.00 |
| `powerplant_wind1` | POWERPLANT | 0 | eletric 15 | - | - | - | 0.00 |
| `powerplant_wind2` | POWERPLANT | 0 | eletric 35 | - | - | - | 0.00 |
| `production_airplane` | PRODUCTION_LINE | 500 | vehicles 1 | vehicles 500.0 | steel 1, aluminium 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | steel 500.0, aluminium 500.0, plastics 500.0, mcomponents 500.0, ecomponents 500.0, fabric 500.0, eletronics 500.0 | 0.00 |
| `production_train` | PRODUCTION_LINE | 400 | vehicles 1 | vehicles 400.0 | steel 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | steel 400.0, plastics 400.0, mcomponents 400.0, ecomponents 400.0, fabric 400.0, eletronics 400.0 | 0.00 |
| `production_train2` | PRODUCTION_LINE | 400 | vehicles 1 | vehicles 400.0 | steel 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | steel 400.0, plastics 400.0, mcomponents 400.0, ecomponents 400.0, fabric 400.0, eletronics 400.0 | 0.00 |
| `production_vehicle` | PRODUCTION_LINE | 400 | vehicles 1 | vehicles 400.0 | steel 1, plastics 1, mcomponents 1, ecomponents 1, fabric 1, eletronics 1 | steel 400.0, plastics 400.0, mcomponents 400.0, ecomponents 400.0, fabric 400.0, eletronics 400.0 | 0.00 |
| `sawmill` | FACTORY | 20 | boards 7 | boards 140.0 | wood 9 | wood 180.0 | 0.54 |
| `sewage_treatment_big` | SEWAGE_TREATMENT | 20 | water 22 | water 440.0 | usagewater 11, chemicals 0.03 | usagewater 220.0, chemicals 0.6 | 3.08 |
| `sewage_treatment_small` | SEWAGE_TREATMENT | 10 | water 16.5 | water 165.0 | usagewater 8, chemicals 0.032 | usagewater 80.0, chemicals 0.3 | 0.85 |
| `slaughterhouse` | FACTORY | 50 | meat 1.2 | meat 60.0 | livestock 3 | livestock 150.0 | 0.00 |
| `steel_mill` | FACTORY | 500 | steel 0.086 | steel 43.0 | coal 0.75, iron 0.4 | coal 375.0, iron 200.0 | 95.00 |
| `steel_mill_v2` | FACTORY | 500 | steel 0.086 | steel 43.0 | coal 0.75, iron 0.4 | coal 375.0, iron 200.0 | 275.00 |
| `uranium_conversion` | FACTORY | 80 | uf6 0.012, vehicles 1 | uf6 1.0, vehicles 80.0 | yellowcake 0.025, chemicals 0.0026 | yellowcake 2.0, chemicals 0.2 | 64.00 |
| `uranium_mine` | MINE_URANIUM | 100 | uranium 0.75 | uranium 75.0 | - | - | 0.00 |
| `uranium_processing` | FACTORY | 60 | yellowcake 0.0285 | yellowcake 1.7 | uranium 0.6 | uranium 36.0 | 36.00 |
| `waste_aluminiumrecycling` | FACTORY | 25 | aluminium 0.5 | aluminium 12.5 | waste_aluminium 1 | waste_aluminium 25.0 | 0.00 |
| `waste_gravelrecycling` | FACTORY | 15 | gravel 4 | gravel 60.0 | waste_gravel 6 | waste_gravel 90.0 | 4.50 |
| `waste_plasticrecycling` | FACTORY | 20 | plastics 0.1 | plastics 2.0 | waste_plastic 0.5 | waste_plastic 10.0 | 1.40 |
| `waste_steelrecycling` | FACTORY | 30 | steel 0.65 | steel 19.5 | waste_steel 1.2 | waste_steel 36.0 | 0.00 |
| `water_to_water` | MINE_WATER_SURFACE | 0 | water 150 | - | - | - | 0.00 |
| `water_treatment_big` | WATER_TREATMENT | 10 | water 30 | water 300.0 | usagewater 3, chemicals 0.047 | usagewater 30.0, chemicals 0.5 | 1.27 |
| `water_treatment_small` | WATER_TREATMENT | 5 | water 24 | water 120.0 | usagewater 2.6, chemicals 0.05 | usagewater 13.0, chemicals 0.2 | 0.33 |
| `water_well_big` | MINE_WATER | 7 | water 215 | water 1505.0 | - | - | 0.67 |
| `water_well_small` | MINE_WATER | 0 | water 70 | - | - | - | 0.00 |
| `woodcutting_post` | MINE_WOOD | 10 | wood 6.25 | wood 62.5 | - | - | 0.00 |
