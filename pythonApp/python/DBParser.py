import re
import sys
import tokenize
if sys.hexversion < 0x03000000:
    from cStringIO import StringIO
else:
    from io import StringIO

def parse_ignore(src):
    while True:
        try:
            token = next(src)
        except StopIteration:
            break
        if token[1] == ')':
            break

def parse_field(src):
    token = next(src)
    if token[1] != '(':
        return

    token = next(src)
    if token[0] not in (tokenize.NAME, tokenize.STRING):
        return
    field = token[1]

    token = next(src)
    if token[1] != ',':
        return

    token = next(src)
    if token[0] != tokenize.STRING:
        return
    value = token[1]
    return field, value.strip('"')

def parse_rtyp(src):
    token = next(src)
    if token[1] != '(':
        return

    token = next(src)
    if token[0] != tokenize.NAME:
        return
    rtyp = token[1]

    token = next(src)
    if token[1] != ',':
        return

    token = next(src)
    if token[0] != tokenize.STRING:
        return
    name = token[1]
    return rtyp, name.strip('"')

def parse_record(src):
    """
    :param func next: token generator function
    :return: record dict
    :rtype: dict
    """
    record = {'info' : {}}

    rtyp, name = parse_rtyp(src)
    record['RTYP'] = rtyp
    record['recordname'] = name

    token = next(src)
    while True:
        if token[1] == '}':
            break
        elif token[1] == 'field':
            field, value = parse_field(src)
            record[field] = value
        elif token[1] == 'info':
            field, value = parse_field(src)
            record['info'][field] = value
        elif token[1] == 'alias':
            parse_ignore(src)

        token = next(src)

    return record

def parse_database(source):
    """
    :param str source: EPICS database
    :return: list of record dict
    :rtype: list of dicts
    """

    src = StringIO(source).readline
    src = tokenize.generate_tokens(src)

    records = []
    while True:
        try:
            token = next(src)
        except StopIteration:
            break
        if token[0] is tokenize.COMMENT:
            continue
        if token[1] == 'record':
            records.append(parse_record(src))
    
    return records

def find_params(records):
    """
    :param list records: list of EPICS record dicts
    :return: list of drvInfo dict
    :rtype: list of dicts
    """

    # regexp patterns to remove macros like $(P)
    macro_pat = re.compile('\$\(.*?\)')
    # regexp pattern to extract drv info string
    link_pat = re.compile('^@asyn\(.*\)\s*(.*)')
    # extract data type info from device type (DTYP)
    dtype_pat = re.compile('^asyn(.*?)(|In|Out|Read|Write)$')

    params = {}
    for record in records:
        recname = macro_pat.sub('', record['recordname'])
        # strip off _RBV suffix, this is only a convention
        recname = recname.rstrip('_RBV')

        # check dtype is asynXXX(In|Out|Read|Write)
        m = dtype_pat.match(record.get('DTYP', ''))
        if m is None or m.group(1) == '':
            continue
        dtype = m.group(1)

        # get INP or OUT link
        if 'INP' in record:
            iolink = record['INP']
        elif 'OUT' in record:
            iolink = record['OUT']
        else:
            continue
        # check link is of form @asyn()drvinfo
        m = link_pat.match(iolink)
        if m is None or m.group(1) == '':
            continue
        drvinfo = m.group(1)

        # try 'pyname' info
        pyname = ''
        if 'info' in record:
            if 'pyname' in record['info']:
                pyname = record['info']['pyname']

        if drvinfo not in params:
            if pyname != '':
                name = pyname
            else:
                name = recname
            params[drvinfo] = (name, dtype)
    return params


if __name__ == '__main__':
    import sys
    from pprint import pprint
    records = parse_database(open(sys.argv[1]).read())
    pprint(records)
    params = find_params(records)
    pprint(params)
