# GDB's default pretty printer barfs up something unreadable for flx,
# thanks to my use of unions and bitfields. 

import gdb.printing

class FlxPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return '{0x%08x:%s %s, %s}' % (int(self.val['pt'] << 3),
                                       str(self.val['nil']),
                                       str(self.val['st']),
                                       str(self.val['gen']))

class AtomicFlxPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val['_M_i']
    
def lookup_type (val):
    if val.type.unqualified().strip_typedefs().tag == 'flx':
        return FlxPrinter(val)
    if val.type.unqualified().strip_typedefs().tag == 'flx_na':
        return FlxPrinter(val)
    if val.type.unqualified().strip_typedefs().tag == 'half_atomic_flx':
        return AtomicFlxPrinter(val)
    
    return None
    
gdb.pretty_printers.append(lookup_type)
