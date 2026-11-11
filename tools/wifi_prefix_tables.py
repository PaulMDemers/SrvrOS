"""Bind legacy capture-proof literals to the selected native MCU source.

Workspace bookkeeping must never rewrite register addresses or values. This
checks source identity only; full transfer, calibration and cleanup proofs are
still required separately.
"""
import ast
import re

TABLES = {'switch_table': ('bbp_switch', 20), 'dcoc_table': ('dcoc_plan', 9)}


def source_tables(source):
    result = {}
    for proof_name, (name, count) in TABLES.items():
        matches = re.findall(r'static\s+const\s+struct\s+mt76_mcu_register_pair\s+' +
                             name + r'\[' + str(count) + r'\]\s*=\s*\{(.*?)\};',
                             source, re.S)
        if len(matches) != 1:
            raise ValueError('missing/ambiguous native table: ' + name)
        body = re.sub(r'/\*.*?\*/|//[^\n]*', '', matches[0], flags=re.S)
        pair = r'\{\s*(0x[0-9a-fA-F]+|0|[1-9][0-9]*)\s*,\s*(0x[0-9a-fA-F]+|0|[1-9][0-9]*)\s*\}'
        found = re.findall(pair, body)
        if re.sub(pair, '', body).replace(',', '').strip():
            raise ValueError('nonliteral native table: ' + name)
        values = [(int(a, 0), int(v, 0)) for a, v in found]
        if (len(values) != count or len({a for a, _ in values}) != count or
                any(not 0 <= n <= 0xffffffff for p in values for n in p)):
            raise ValueError('invalid native table: ' + name)
        result[proof_name] = values
    return result


def check(proof, source):
    expected = source_tables(source)
    assignments = {n: [] for n in TABLES}
    try:
        tree = ast.parse(proof)
        for node in ast.walk(tree):
            if isinstance(node, ast.Assign):
                for target in node.targets:
                    if isinstance(target, ast.Name) and target.id in assignments:
                        assignments[target.id].append(ast.literal_eval(node.value))
    except (SyntaxError, ValueError, TypeError) as error:
        raise ValueError('nonliteral capture-proof table') from error
    for name, values in assignments.items():
        if values != [expected[name]]:
            raise ValueError('capture-proof table differs from native source: ' + name)
    return dict(source_tables_bound=True,
                tables={name: len(rows) for name, rows in expected.items()},
                native_accepted=False, connectivity_tested=False)
