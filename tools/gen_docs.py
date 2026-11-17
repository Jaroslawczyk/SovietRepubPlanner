# Builds docs/data/buildings.json and docs/buildings.md from the game's own files
# (media_soviet/buildings_types/*.ini + *.bbox, names from sovietRussian.btf / sovietEnglish.btf).
# Run:  python tools/gen_docs.py        (from the DistrictPlanner folder)
import glob, json, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.normpath(os.path.join(HERE, '..', '..'))
MEDIA = os.path.join(GAME, 'media_soviet')
OUT = os.path.join(HERE, '..', 'docs')

def load_names(path):
    d = open(path, 'rb').read()
    count = struct.unpack('>I', d[0:4])[0]
    text = 12 + count * 10
    names = {}
    for i in range(count):
        e = 12 + i * 10
        ident, off = struct.unpack('>II', d[e:e + 8])
        ln = struct.unpack('>H', d[e + 8:e + 10])[0]
        names[ident] = d[text + off * 2: text + off * 2 + ln * 2].decode('utf-16-be', 'replace')
    return names

def load_bbox(path):
    d = open(path, 'rb').read()
    if len(d) < 24: return None
    parts = struct.unpack('<i', d[:4])[0]
    if parts < 1 or len(d) != 4 + parts * 540: parts = 1
    lo = [1e9] * 3; hi = [-1e9] * 3
    for p in range(parts):
        o = len(d) - 24 - (parts - 1 - p) * 540
        g = struct.unpack('<6f', d[o:o + 24])
        for k in range(3): lo[k] = min(lo[k], g[k]); hi[k] = max(hi[k], g[k + 3])
    return lo, hi

NUM3 = re.compile(r'^\s*(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)')

def parse_ini(path):
    b = {'types': [], 'subtypes': [], 'flags': [], 'storage': {}, 'consumption': {}, 'production': {}, 'per_second': {}, 'conns': {}, 'zones': {}, 'values': {}}
    cur = None      # list that bare "x y z" lines / advanced points go to
    for raw in open(path, encoding='latin-1'):
        line = raw.strip()
        if not line: continue
        if not line.startswith('$'):
            m = NUM3.match(line)
            if m and cur is not None: cur.append([float(m.group(1)), float(m.group(2)), float(m.group(3))])
            continue                        # also skips "-$..." (commented out)
        tok = line[1:].split()
        key, args = tok[0], tok[1:]
        if key == 'CONNECTION_ADVANCED_POINT':
            if cur is not None and len(args) >= 3:
                try: cur.append([float(args[0]), float(args[1]), float(args[2])])
                except ValueError: pass
            continue
        cur = None
        if key.startswith('CONNECTION') or key.endswith('_SQUARE') or key in ('STATION_NOT_BLOCK', 'PARKING_NOT_BLOCK', 'VEHICLE_STATION', 'VEHICLE_PARKING', 'VEHICLE_PARKING_PERSONAL', 'HELIPORT_AREA', 'WORKER_RENDERING_AREA'):
            dead = 'DEAD' in key or 'NOT_BLOCK' in key or key in ('VEHICLE_STATION', 'VEHICLE_PARKING', 'VEHICLE_PARKING_PERSONAL', 'HELIPORT_AREA', 'WORKER_RENDERING_AREA')
            grp = b['zones'] if dead else b['conns']
            cur = []; grp.setdefault(key, []).append(cur)
            nums = []
            for a in args:
                try: nums.append(float(a))
                except ValueError: break
            for k in range(0, len(nums) - 2, 3): cur.append(nums[k:k + 3])     # some keys carry the points inline
        elif key == 'NAME' and args: b['name_id'] = int(args[0]) if args[0].lstrip('-').isdigit() else None
        elif key.startswith('TYPE_'): b['types'].append(key[5:])
        elif key.startswith('SUBTYPE_'): b['subtypes'].append(key[8:])
        elif key in ('STORAGE', 'STORAGE_SPECIAL', 'STORAGE_FUEL', 'STORAGE_IMPORT', 'STORAGE_EXPORT', 'STORAGE_DEMAND_BASIC', 'STORAGE_DEMAND_ADVANCED', 'STORAGE_DEMAND_HOTEL') and len(args) >= 2:
            try: b['storage'][(key[8:] + ':' if key != 'STORAGE' else '') + args[0].replace('RESOURCE_TRANSPORT_', '')] = float(args[1])
            except ValueError: pass
        elif key in ('CONSUMPTION', 'PRODUCTION', 'CONSUMPTION_PER_SECOND') and len(args) >= 2:
            try: b[{'CONSUMPTION': 'consumption', 'PRODUCTION': 'production', 'CONSUMPTION_PER_SECOND': 'per_second'}[key]][args[0]] = float(args[1])
            except ValueError: pass
        elif args and re.match(r'^-?\d+(\.\d+)?$', args[0]) and not key.startswith(('COST_', 'PARTICLE', 'RESOURCE_', 'VEHICLE_', 'OFFSET', 'TOKEN', 'MOVEABLE', 'TEXT')):
            b['values'][key] = float(args[0])
        elif not args and not key.startswith(('COST_', 'TEXT_')): b['flags'].append(key)
    return b

def main():
    ru = load_names(os.path.join(MEDIA, 'sovietRussian.btf'))
    en = load_names(os.path.join(MEDIA, 'sovietEnglish.btf'))
    res = []
    for ini in sorted(glob.glob(os.path.join(MEDIA, 'buildings_types', '*.ini'))):
        stem = os.path.splitext(os.path.basename(ini))[0]
        b = parse_ini(ini); b['id'] = stem
        nid = b.get('name_id')
        b['name_ru'] = ru.get(nid, '') if nid is not None else ''
        b['name_en'] = en.get(nid, '') if nid is not None else ''
        bb = os.path.join(MEDIA, 'buildings_types', stem + '.bbox')
        box = load_bbox(bb) if os.path.exists(bb) else None
        if box:
            lo, hi = box
            b['bbox'] = {'min': [round(v, 2) for v in lo], 'max': [round(v, 2) for v in hi]}
            # the zone a building really claims: its box plus every connection / dead-zone / parking point of the ini
            xs = [lo[0], hi[0]]; zs = [lo[2], hi[2]]
            for grp in (b['conns'], b['zones']):
                for lists in grp.values():
                    for pts in lists:
                        for p in pts: xs.append(p[0]); zs.append(p[2])
            b['zone'] = {'min': [round(min(xs), 2), round(min(zs), 2)], 'max': [round(max(xs), 2), round(max(zs), 2)]}
        res.append(b)
    os.makedirs(os.path.join(OUT, 'data'), exist_ok=True)
    json.dump(res, open(os.path.join(OUT, 'data', 'buildings.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)

    def cnt(b, *keys): return sum(len(b['conns'].get(k, [])) for k in keys)
    def conn_text(b):
        out = []
        for label, keys in (('пеш', ('CONNECTION_PEDESTRIAN', 'CONNECTION_PEDESTRIAN_ALLOWPASS', 'CONNECTION_PEDESTRIAN_NOTPICK')),
                            ('дор', ('CONNECTION_ROAD', 'CONNECTION_ROAD_ALLOWPASS', 'CONNECTION_ROAD_INPUT', 'CONNECTION_ROAD_OUTPUT')),
                            ('вода вх', ('CONNECTION_WATERPIPE_INPUT',)), ('вода вых', ('CONNECTION_WATERPIPE_OUTPUT',)),
                            ('канал вх', ('CONNECTION_SEWAGE_INPUT',)), ('канал вых', ('CONNECTION_SEWAGE_OUTPUT',)),
                            ('тепло м', ('CONNECTION_HEATING_SMALL',)), ('тепло б', ('CONNECTION_HEATING_BIG',)),
                            ('эл ср вх', ('CONNECTION_ELETRIC_LOW_INPUT', 'CONNECTION_ELETRIC_LOW_INPUT_ONLY')), ('эл ср вых', ('CONNECTION_ELETRIC_LOW_OUTPUT', 'CONNECTION_ELETRIC_LOW_OUTPUT_ONLY')),
                            ('эл выс вх', ('CONNECTION_ELETRIC_HIGH_INPUT', 'CONNECTION_ELETRIC_HIGH_INPUT_ONLY')), ('эл выс вых', ('CONNECTION_ELETRIC_HIGH_OUTPUT', 'CONNECTION_ELETRIC_HIGH_OUTPUT_ONLY')),
                            ('жд', ('CONNECTION_RAIL', 'CONNECTION_RAIL_ALLOWPASS')), ('конвейер', ('CONNECTION_CONVEYOR_INPUT', 'CONNECTION_CONVEYOR_OUTPUT')),
                            ('труба', ('CONNECTION_PIPE_INPUT', 'CONNECTION_PIPE_OUTPUT'))):
            n = cnt(b, *keys)
            if n: out.append(f'{label} {n}')
        return ', '.join(out)
    def kv(d): return ', '.join(f'{k} {v:g}' for k, v in d.items())
    groups = {}
    for b in res: groups.setdefault(b['types'][0] if b['types'] else '(без типа)', []).append(b)
    L = ['# Здания игры: размеры, зоны, подключения, потребности', '',
         'Сгенерировано `tools/gen_docs.py` из `media_soviet/buildings_types`. Полные данные (все точки подключений и мёртвых зон с координатами) — в `data/buildings.json`.', '',
         '- **Размер** — габарит модели X×Z и высота, м (файл `.bbox`).',
         '- **Зона** — габарит вместе со всеми точками подключений, мёртвыми зонами дорог и стоянками (X×Z, м): столько места здание реально занимает, сюда нельзя ставить соседей.',
         '- **Потребление / производство** — на одного работника в день (т), `eletric` в строке «в секунду» — МВт.', '']
    for g in sorted(groups):
        L += [f'## {g} ({len(groups[g])})', '', '| id | название | размер | зона | рабочие | вместимость / склад | потребление | производство | прочее | подключения |', '|---|---|---|---|---|---|---|---|---|---|']
        for b in groups[g]:
            size = zone = ''
            if 'bbox' in b:
                lo, hi = b['bbox']['min'], b['bbox']['max']
                size = f'{hi[0] - lo[0]:.0f}×{hi[2] - lo[2]:.0f}, в {hi[1]:.0f}'
                z = b['zone']; zone = f'{z["max"][0] - z["min"][0]:.0f}×{z["max"][1] - z["min"][1]:.0f}'
            v = b['values']
            work = '%g' % v['WORKERS_NEEDED'] if 'WORKERS_NEEDED' in v else ''
            if 'PROFESORS_NEEDED' in v: work += f' +{v["PROFESORS_NEEDED"]:g} с высш.'
            other = [f'{k} {x:g}' for k, x in v.items() if k in ('QUALITY_OF_LIVING', 'CITIZEN_ABLE_SERVE', 'ATTRACTIVE_SCORE', 'REPAIR_AREA', 'COUNT_LIMIT', 'MONUMENT_GOVERNMENT_LOYALTY_RADIUS', 'CONSUMPTION_WATER_REQUIRED_QUALITY', 'ELETRIC_WITHOUT_LIGHTING_FACTOR', 'ELETRIC_WITHOUT_WORKING_FACTOR')]
            other += [f for f in b['flags'] if f in ('HEATING_ENABLE', 'HEATING_DISABLE', 'WATERSEWAGE_DISABLE', 'SEWAGE_DISABLE', 'ELECTRIC_DISABLE', 'CIVIL_BUILDING', 'OBSOLETE')]
            cons = kv(b['consumption']) + ('; в секунду: ' + kv(b['per_second']) if b['per_second'] else '')
            L.append(f'| `{b["id"]}` | {b["name_ru"] or b["name_en"]} | {size} | {zone} | {work} | {kv(b["storage"])} | {cons} | {kv(b["production"])} | {", ".join(other)} | {conn_text(b)} |')
        L.append('')
    open(os.path.join(OUT, 'buildings.md'), 'w', encoding='utf-8').write('\n'.join(L))
    print('buildings', len(res), 'groups', len(groups))

main()
