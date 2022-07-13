import sys
from wasm import decode_module, SEC_DATA, DataSection, format_instruction

with open(sys.argv[1], 'rb') as raw:
    raw = raw.read()

mod_iter = iter(decode_module(raw))
header, header_data = next(mod_iter)

SEGMENT_SIZE = 1024*8-1

def dump_data(i, offset, data):
    sys.stdout.write("  (data (;{0};) (i32.const {1}) ".format(i, offset))
    sys.stdout.write("\"")
    for b in data:
        if b >=32 and b < 128 and b != ord("\""):
            sys.stdout.write(chr(b))
            if b == 92:
                sys.stdout.write(chr(b))
        else:
            sys.stdout.write(r"\{0:02x}".format(b))
    sys.stdout.write("\")\n")

inx = 0
for cur_sec, cur_sec_data in mod_iter:
    if cur_sec_data.id == SEC_DATA:
        for idx, entry in enumerate(cur_sec_data.payload.entries):
            #print(entry.offset[0].imm.value, entry.index, entry.size)
            offset = entry.offset[0].imm.value
            _from = 0
            remaining = entry.size
            while remaining > 0:
                to_dump = min(remaining, SEGMENT_SIZE)
                dump_data(inx, offset, entry.data[_from: _from+to_dump])
                _from += to_dump
                offset += to_dump
                remaining -= to_dump
                inx += 1
