import cStringIO, tokenize

def parse_field(next, token):
    token = next()
    if token[1] != '(':
        return

    token = next()
    if token[0] != tokenize.NAME:
        return
    field = token[1]

    token = next()
    if token[1] != ',':
        return

    token = next()
    if token[0] != tokenize.STRING:
        return
    value = token[1]
    return field, value

def parse_record(next, token):
    record = {'info' : {}}

    token = next()
    while True:
        if token[0] == tokenize.NAME:
            record['RTYP'] = token[1]

        if token[0] == tokenize.STRING:
            record['recordname'] = token[1]

        if token[1] == '}':
            break
        if token[1] == 'field':
            field, value = parse_field(next, token)
            record[field] = value
        if token[1] == 'info':
            field, value = parse_field(next, token)
            record['info'][field] = value

        token = next()

    return record


def parse_database(source):
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
            records.append(parse_record(src.next, token))
    
    return records

if __name__ == '__main__':
    import sys
    from pprint import pprint
    pprint(parse_database(open(sys.argv[1]).read()))
