# GDB's default pretty printer barfs up something unreadable for flx,
# thanks to my use of unions and bitfields. 

import gdb.printing

class FlxPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return '{0x%08x:%s %s, %s}' % (int(self.val['constexp'] & ~15),
                                       str(self.val['nil']),
                                       str(self.val['st']),
                                       str(self.val['gen']))

def lookup_type (val):
        if str(val.type) == 'flx':
            return FlxPrinter(val)
        return None
    
gdb.pretty_printers.append(lookup_type)
