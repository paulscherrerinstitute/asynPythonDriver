import re
import cStringIO, tokenize

def parse_ignore(next):
    while True:
        try:
            token = next()
        except StopIteration:
            break
        if token[1] == ')':
            break

def parse_field(next):
    token = next()
    if token[1] != '(':
        return

    token = next()
    if token[0] not in (tokenize.NAME, tokenize.STRING):
        return
    field = token[1]

    token = next()
    if token[1] != ',':
        return

    token = next()
    if token[0] != tokenize.STRING:
        return
    value = token[1]
    return field, value.strip('"')

def parse_rtyp(next):
    token = next()
    if token[1] != '(':
        return

    token = next()
    if token[0] != tokenize.NAME:
        return
    rtyp = token[1]

    token = next()
    if token[1] != ',':
        return

    token = next()
    if token[0] != tokenize.STRING:
        return
    name = token[1]
    return rtyp, name.strip('"')

def parse_record(next):
    """
    :param func next: token generator function
    :return: record dict
    :rtype: dict
    """
    record = {'info' : {}}

    rtyp, name = parse_rtyp(next)
    record['RTYP'] = rtyp
    record['recordname'] = name

    token = next()
    while True:
        if token[1] == '}':
            break
        elif token[1] == 'field':
            field, value = parse_field(next)
            record[field] = value
        elif token[1] == 'info':
            field, value = parse_field(next)
            record['info'][field] = value
        elif token[1] == 'alias':
            parse_ignore(next)

        token = next()

    return record

def parse_database(source):
    """
    :param str source: EPICS database
    :return: list of record dict
    :rtype: list of dicts
    """

    src = cStringIO.StringIO(source).readline
    src = tokenize.generate_tokens(src)

    records = []
    while True:
        try:
            token = src.next()
        except StopIteration:
            break
        if token[0] is tokenize.COMMENT:
            continue
        if token[1] == 'record':
            records.append(parse_record(src.next))
    
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
