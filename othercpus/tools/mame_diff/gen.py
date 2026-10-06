import re
vec="../vec/"
m=open(vec+"m6800.cpp",encoding="utf-8",errors="replace").read().split("\n")
# macro block: from '#define pPPC' to before '#include "6800ops.hxx"'
s=next(i for i,l in enumerate(m) if l.startswith("#define pPPC"))
e=next(i for i,l in enumerate(m) if l.startswith('#include "6800ops.hxx"'))
block="\n".join(m[s:e])
block=block.replace("const u8 m6800_cpu_device::flags8i[256]","const u8 m6800_cpu_device::flags8i[256]")
ops=open(vec+"6800ops.hxx",encoding="utf-8",errors="replace").read()
names=sorted(set(re.findall(r"OP_HANDLER\(\s*(\w+)\s*\)",ops)))
t=open(vec+"m6801.cpp",encoding="utf-8",errors="replace").read().split("\n")
ts=next(i for i,l in enumerate(t) if "m6803_insn[0x100]" in l)
tab="\n".join(t[ts+1:ts+1+40])
tab=tab[:tab.index("};")]
tab=tab.replace("m6801_cpu_device","m6800_cpu_device")
decl="\n".join("  void %s();"%n for n in names)
out=r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <utility>
extern "C" {
#include "cpu6803.h"
}
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int16_t s16; typedef int8_t s8;
union PAIR { struct { u16 l; u16 h; } w; struct { u8 l; u8 h; u8 h2; u8 h3; } b; u32 d; };
#define M6800_WAI 8
#define M6800_SLP 16
struct mem_t { u8 *m; std::vector<std::pair<int,int>> *log;
  u8 read_byte(u32 a){ return m[a&0xffff]; }
  void write_byte(u32 a,u8 v){ if(log) log->push_back({(int)(a&0xffff),(int)v}); m[a&0xffff]=v; } };
class m6800_cpu_device {
public:
  PAIR m_ppc,m_pc,m_s,m_x,m_d,m_ea; u8 m_cc; int m_wai_state; int m_icount;
  mem_t m_program, m_cprogram, m_copcodes;
  static const u8 flags8i[256]; static const u8 flags8d[256];
  typedef void (m6800_cpu_device::*op_func)();
  static const op_func insn[256];
  u16 RM16(u32 a){ u16 h=m_program.read_byte(a); return (h<<8)|m_program.read_byte((a+1)&0xffff);}
  void WM16(u32 a, PAIR *p){ m_program.write_byte(a,p->b.h); m_program.write_byte((a+1)&0xffff,p->b.l);}
  void execute_one(){} void take_trap(){} void check_irq_lines(){} void eat_cycles(){}
  void logerror(const char*,...){}
%s
};
%s
const m6800_cpu_device::op_func m6800_cpu_device::insn[256] = {
%s
};
#undef XX
#include "%s6800ops.hxx"
'''%(decl,block,tab,vec)
open("harness_body.inc","w").write(out)
print(len(names),"handlers")
