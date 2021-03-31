import sys

tocnames = {}
tocnames['BOOL'] = 'bool'
tocnames['INT'] = 'int'
tocnames['U8'] = 'u8'
tocnames['CST'] = 'const char *'
tocnames['ENM'] = 'int'
tocnames['DICE'] = 'DICE'

def replacetext(text, replace):
    """ does a replacement of each key/value of replace into text
    """
    for key, value in replace.iteritems():
        text = text.replace(key, value)
    return text


def error(text):
    """ Reports an error
    """
    print text
    sys.exit()

def pushtoken(token, text):
    """ returns text with token prefixed
    """
    return token + ' ' + text

def validatetype(itemttype, text):
    """ errors if itemtype isn't valid
    """

    itemlist = [ 'BOOL', 'U8', 'CST', 'INT', 'ENM', 'DICE' ]

    if itemtype not in itemlist:
	error('Invalid item type ' + itemtype + ' at' + text)


def processdice(dice):
    """ Returns a {} initializer for given dice text
    """

    dice = dice.lower()

    pieces = dice.split('d')
    if len(pieces) == 1:
        return '{ 0, 0, ' + pieces[0] + ' }'
    numdie = pieces[0]

    if len(pieces) != 2:
        error("Malformed dice " + dice)

    neg = ''
    tail = pieces[1].split('+')
    if len(tail) == 1:
        neg = '-'
        tail = pieces[1].split('-')

    if len(tail) == 1:
        return '{ ' + pieces[0] + ', ' + pieces[1] + ', 0 }'

    return '{ ' + pieces[0] + ', ' + tail[0] + ', ' + neg + tail[1] + ' }'
        

def readtoken(text):
    """ returns (token, text) for next token from stream
    """

    if len(text) == 0:
	return ('', '')

    nc = text[0]
    if nc.isspace():
	return readtoken(text[1:])

    if nc == "'":
	result = nc
	result += text[1]
	if text[1] == '\\':
	    result += text[2]
	    if text[3] != "'":
		error('Unterminated quote' + text)
	    result += text[3]
	    return (result, text[4:])
	if text[2] != "'":
	    error('Unterminated quote' + text)
	result += text[2]
	return (result, text[3:])

    if nc == '"':
	idx = 1
	result = nc
	while text[idx] != '"':
	    if text[idx] == '\\':
		result += text[idx]
		idx +=1
	    result += text[idx]
	    idx += 1
	result += text[idx]
	return (result, text[idx+1:])

    result = nc
    idx = 1
    while not text[idx].isspace() and idx < len(text):
	result += text[idx]
	idx += 1

    return (result, text[idx:])

sf = open('source.txt', 'rt')

ohf = open('glbdef.h', 'wt')
ocf = open('glbdef.cpp', 'wt')

text = sf.read()


typeorder = []
defn = {}
defns = {}

while len(text):
    (token, text) = readtoken(text)

    if token == '':
        break
    elif token == 'COMMENT':
	(token, text) = readtoken(text)
	if token != '{':
	    error('Missing start brace in comment ' + text)
	while token != '}':
	    (token, text) = readtoken(text)
    elif token == 'DEFINE':
	(type, text) = readtoken(text)

	if type in defn:
	    error("Redefinition of " + type + " at " + text)

	(token, text) = readtoken(text)
	if token != '{':
	    error('Missing start brace in define ' + text)

	itemlist = []
	while True:
	    (itemtype, text) = readtoken(text)
	    if itemtype == '}':
		break

            itemtype = itemtype.upper()
	    validatetype(itemtype, text)

	    (itemname, text) = readtoken(text)
	    itemenum = None
	    if itemtype == 'ENM':
		(itemenum, text) = readtoken(text)
	    elif itemtype == 'ENMLIST':
		(itemenum, text) = readtoken(text)
	    (itemdef, text) = readtoken(text)

	    itemlist += ((itemtype, itemname, itemenum, itemdef),)

        typeorder += (type, )
	defn[type] = itemlist
    else:
        # We require this to be a pre-existing token!
        if token not in defn:
            error('Unknown token ' +token + ' at ' + text)
        type = token

        (name, text) = readtoken(text)

        (open, text) = readtoken(text)

        # It could be this is { starting a block.
        # or a number forcing a specific enum value
        # or something else, suggesting we just are a throw-away enum.
        overrides = {}

        forceindex = None
        if open.isdigit():
            forceindex = int(open)
            (open, text) = readtoken(text)

        if open != '{':
            # throw away
            text = pushtoken(open, text)
        else:
            # {} block
            while True:
                (overname, text) = readtoken(text)

                if overname == '}':
                    break

                # Wrong for enumlist
                if overname in overrides:
                    error("Repeated definition " + overname + ' in ' + text)

                (overval, text) = readtoken(text)

                overrides[overname] = overval

        # Store our result.
        deflist = []
        if type in defns:
            deflist = defns[type]

        deflist += ((name, forceindex, overrides), )

        defns[type] = deflist

# Okay we've parsed into defn all the defintions
# and into defns all the items
# Let us build the C++ code

ohf.write(
"""// Automatically generated by enummaker.py.
//DO NOT EDIT THIS FILE (Yes, I mean you!)
#ifndef __glbdef_h__
#define __glbdef_h__

#include "mygba.h"
#include "rand.h"
#include "buf.h"

inline BUF lcl_savedata(int data)
{
    BUF buf;
    buf.sprintf("%d", data);
    return buf;
}

inline BUF lcl_savedata(u8 data)
{
    BUF buf;
    buf.sprintf("%d", data);
    return buf;
}

inline BUF lcl_savedata(bool data)
{
    BUF buf;
    buf.sprintf("%s", data ? "true" : "false");
    return buf;
}

inline BUF lcl_savedata(const char *data)
{
    BUF buf;
    buf.reference(data);
    buf = buf.protectWithQuotes();
    return buf;
}

inline BUF lcl_savedata(const DICE &dice)
{
    BUF buf;
    buf.sprintf("%dd%d%+d", dice.myNumDie, dice.mySides, dice.myBonus);
    return buf;
}

inline void lcl_loaddata(BUF buf, int &data)
{
    data = atoi(buf.buffer());
}

inline void lcl_loaddata(BUF buf, u8 &data)
{
    if (buf.startsWith("'"))
    {
        data = buf.buffer()[1];
    }
    else
        data = atoi(buf.buffer());
}

inline void lcl_loaddata(BUF buf, bool &data)
{
    if (!buf.strcasecmp("true"))
        data = true;
    else if (!buf.strcasecmp("false"))
        data = false;
    else data = atoi(buf.buffer()) != 0;
}

inline void lcl_loaddata(BUF buf, const char *&data)
{
    // Leak!  Joyous Leaks!
    data = buf.strdup();
}

""")

structuredefn = ''

for key in typeorder:
    value = defn[key]
    replace = {}
    replace['$$TYPE$$'] = key
    replace['$$type$$'] = key.lower()
    instances =[]
    if key in defns:
        instances = defns[key]
    enumbody = ''
    for instance in instances:
        enumbody += '    '
        enumbody += key
        enumbody += '_'
        enumbody += instance[0]
        if instance[1] is not None:
            enumbody += ' = '
            enumbody += str(instance[1])
        enumbody += ',\n'
    replace['$$ENUMBODY$$'] = enumbody

    structbody = ''
    resetbody = ''
    loadbody = ''
    savebody = ''

    for member in value:
        (itemtype, itemname, itemenum, itemdef) = member
        structbody += '    '
        structbody += tocnames[itemtype]
        structbody += ' '
        structbody += itemname
        structbody += ';\n'

        savebody += '        '
        savebody += 'os << "    ' + itemname + ' " '
        savebody += '<< lcl_savedata(' + itemname + ').buffer() '
        savebody += '<< "\\n";\n'

        resetbody += '        '
        if itemtype == 'ENM':
            resetbody += itemname + ' = ' + itemenum + '_' + itemdef + ';\n'
        else:
            resetbody += itemname + ' = ' + itemdef + ';\n'

        loadbody += '                '
        loadbody += 'if (!_name.strcmp("' + itemname + '"))\n'
        loadbody += '                    '
        loadbody += 'lcl_loaddata(_value, ' + itemname + ');\n'


    replace['$$STRUCTBODY$$'] = structbody
    replace['$$RESETBODY$$'] = resetbody
    replace['$$SAVEBODY$$'] = savebody
    replace['$$LOADBODY$$'] = loadbody

    ohf.write(replacetext(
"""enum $$TYPE$$_NAMES
{
$$ENUMBODY$$
    NUM_$$TYPE$$S
};

#define FOREACH_$$TYPE$$(x) \\
    for ((x) = ($$TYPE$$_NAMES) 0; \\
         (x) < NUM_$$TYPE$$S; \\
         (x) = ($$TYPE$$_NAMES) ((int)(x)+1))
""", replace))

    structuredefn += replacetext(
"""struct $$TYPE$$_DEF
{
$$STRUCTBODY$$

    void reset()
    {
$$RESETBODY$$
    }

    void saveData(ostream &os) const
    {
$$SAVEBODY$$
    }

    void loadData(istream &is)
    {
        reset();
        BUF     _name, _value;

        while (1)
        {
            _name = BUF::readtoken(is);
            if (_name.isstring() && _name.strcmp("}"))
            {
                _value = BUF::readtoken(is);
$$LOADBODY$$
            }
            else
                break;
        }
    }


};

extern const $$TYPE$$_DEF glb_$$type$$defs[];

""", replace)


ohf.write(structuredefn)

ohf.write("""
#endif
""")


ocf.write(
"""// Automatically generated by enummaker.py.
//DO NOT EDIT THIS FILE (Yes, I mean you!)

#include "mygba.h"
#include "glbdef.h"

""")

for key in typeorder:
    value = defn[key]
    replace = {}
    replace['$$TYPE$$'] = key
    replace['$$type$$'] = key.lower()
    instances =[]
    if key in defns:
        instances = defns[key]

    initbody = ''
    for instance in instances:
        initbody += '  {\n'

        overrides = instance[2]

        validlist = {}

        for member in value:
            (itemtype, itemname, itemenum, itemdef) = member
            if itemname in overrides:
                itemdef = overrides[itemname]
            if itemenum is not None:
                itemdef = itemenum + '_' + itemdef
            if itemtype == 'DICE':
                itemdef = processdice(itemdef)
            initbody += '    '
            initbody += itemdef
            initbody += ',\n'

            validlist[itemname] = True

        # Verify there is no unknown elements
        for itemname in overrides:
            if itemname not in validlist:
                error("Invalid member " + itemname + ' in ' + key + '_' + instance[0])

        enumbody += key
        enumbody += '_'
        enumbody += instance[0]
        if instance[1] is not None:
            enumbody += ' = '
            enumbody += str(instance[1])
        initbody += '  },\n'
    replace['$$INITBODY$$'] = initbody

    ocf.write(replacetext(
"""// Definitions for $$TYPE$$
const $$TYPE$$_DEF
glb_$$type$$defs[NUM_$$TYPE$$S] =
{
$$INITBODY$$
};
""", replace))
