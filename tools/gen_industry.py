"""Generates docs/industry.md from the game's building definitions (media_soviet/buildings_types/*.ini).

Every number in the tables is copied from an ini line ($WORKERS_NEEDED, $PRODUCTION, $CONSUMPTION,
$CONSUMPTION_PER_SECOND); the per-building columns are those values multiplied by the workers.
Run: python tools/gen_industry.py
"""
import os, re, collections

GAME = os.path.join(os.path.dirname(__file__), '..', '..')
BT = os.path.join(GAME, 'media_soviet', 'buildings_types')
OUT = os.path.join(os.path.dirname(__file__), '..', 'docs', 'industry.md')

rows = []
for fn in sorted(os.listdir(BT)):
    if not fn.endswith('.ini'):
        continue
    workers, prod, cons, power, types = 0, [], [], 0.0, []
    for line in open(os.path.join(BT, fn), encoding='latin1'):
        line = line.strip()
        if not line.startswith('$'):
            continue
        parts = line[1:].split()
        key = parts[0]
        if key == 'WORKERS_NEEDED' and len(parts) > 1:
            workers = float(parts[1])
        elif key in ('PRODUCTION', 'CONSUMPTION') and len(parts) > 2:
            (prod if key == 'PRODUCTION' else cons).append((parts[1], float(parts[2])))
        elif key == 'CONSUMPTION_PER_SECOND' and len(parts) > 2 and parts[1] == 'eletric':
            power = float(parts[2])
        elif key.startswith('TYPE_'):
            types.append(key[5:])
    if prod:
        rows.append((fn[:-4], types[0] if types else '-', workers, prod, cons, power))

by_res = collections.defaultdict(list)
for r in rows:
    for p, _ in r[3]:
        by_res[p].append(r[0])

L = []
L.append('# Промышленность: цепочки производства по данным игры\n')
L.append('Сгенерировано `tools/gen_industry.py` из `media_soviet/buildings_types/*.ini` (игра 1.1.1.13). '
         'Каждое число — строка ini: `$WORKERS_NEEDED`, `$PRODUCTION <ресурс> <число>`, `$CONSUMPTION <ресурс> <число>`, '
         '`$CONSUMPTION_PER_SECOND eletric <число>`.\n')
L.append('## Как считает мод (вкладка «Промышленность»)\n')
L.append('- **Единица.** Числа `$PRODUCTION`/`$CONSUMPTION` мод считает тоннами на одного рабочего в сутки при 100 % '
         'эффективности. Это **гипотеза**: она согласуется с тем, что в ini у всех заводов эти числа малы и стоят рядом с '
         '`$WORKERS_NEEDED`, но в игре пока не проверена замером (открытый вопрос, см. ниже). Расчёт цепочки от единицы '
         'не зависит: выпуск и потребление берутся в одной и той же мере, поэтому соотношения «сырьё на тонну продукта» '
         'и число заводов на заданный выпуск верны при любой общей единице времени, а абсолютные т/сутки — только если гипотеза верна.\n')
L.append('- **Здание на ресурс.** Из всех зданий с рабочими, выпускающих ресурс, берётся то, у которого выпуск здания в сутки (на рабочего × рабочие) больше (по выпуску на рабочего выигрывала переработка металлолома у сталелитейного завода).\n')
L.append('- **Число зданий** = нужный выпуск в сутки / (выпуск на рабочего × рабочие), округление вверх.\n')
L.append('- **Сырьё** каждого звена = потребление на рабочего × рабочие × число зданий; дальше то же для каждого входа. '
         'Вход, отмеченный «привозим» (своя шахта, ж/д станция, импорт), в цепочку не раскладывается.\n')
L.append('- **Электричество** = `$CONSUMPTION_PER_SECOND eletric` × рабочие (МВт; так же гипотеза о единице).\n')
L.append('- Фермы и поля в ini не имеют `$PRODUCTION` с рабочими (урожай зависит от техники и сезона) — калькулятор их не считает.\n')
L.append('\n## Открытые вопросы (проверить в игре)\n')
L.append('1. Единица `$PRODUCTION`: тонн на рабочего в сутки? Проверка: построить завод, дождаться полной загрузки, сравнить суточный выпуск в окне здания с `число × рабочие`.\n')
L.append('2. Влияние эффективности (образование, лояльность, оборудование) на выпуск — в расчёт не входит.\n')
L.append('3. Логистика (склады, станции, транспорт) не моделируется: мод показывает потоки в т/сутки, по ним выбираются склады и подвоз.\n')
L.append('\n## Производители по ресурсу\n')
L.append('| ресурс | кто производит |\n|---|---|\n')
for res in sorted(by_res):
    L.append(f'| `{res}` | {", ".join("`%s`" % x for x in by_res[res])} |\n')
L.append('\n## Все производственные здания\n')
L.append('| здание | тип | рабочие | выпуск на рабочего | выпуск здания, т/сут | сырьё на рабочего | сырьё здания, т/сут | МВт |\n')
L.append('|---|---|---|---|---|---|---|---|\n')
for name, typ, w, prod, cons, power in rows:
    p1 = ', '.join(f'{r} {v:g}' for r, v in prod)
    p2 = ', '.join(f'{r} {v * w:.1f}' for r, v in prod) if w else '-'
    c1 = ', '.join(f'{r} {v:g}' for r, v in cons) or '-'
    c2 = ', '.join(f'{r} {v * w:.1f}' for r, v in cons) if w and cons else '-'
    L.append(f'| `{name}` | {typ} | {w:g} | {p1} | {p2} | {c1} | {c2} | {power * w:.2f} |\n')
open(OUT, 'w', encoding='utf8').write(''.join(L))
print('wrote', OUT, len(rows), 'buildings')
