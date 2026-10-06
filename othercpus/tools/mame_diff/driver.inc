static u8 memA[65536], memB[65536], memInit[65536];
static std::vector<std::pair<int,int>> logA, logB;
static uint8_t mine_read(void *c, uint16_t a){ (void)c; return memB[a]; }
static void mine_write(void *c, uint16_t a, uint8_t v){ (void)c; logB.push_back({a,v}); memB[a]=v; }
static uint32_t rng=12345;
static uint32_t rnd(){ rng = rng*1664525u+1013904223u; return rng>>8; }

int main(int argc, char **argv)
{
    int iters = argc>1 ? atoi(argv[1]) : 2000;
    long bad=0, total=0;
    extern const char *names_dummy;
    for (int op=0; op<256; op++) {
        m6800_cpu_device ref;
        if (m6800_cpu_device::insn[op]==&m6800_cpu_device::illegl1 || m6800_cpu_device::insn[op]==&m6800_cpu_device::illegl2 ||
            m6800_cpu_device::insn[op]==&m6800_cpu_device::illegl3) continue;
        if (op==0x3e) continue;       /* WAI: leaves the CPU waiting */
        int opbad=0;
        for (int it=0; it<iters; it++) {
            for (int i=0;i<65536;i++) memInit[i]=(u8)rnd();
            u8 a=rnd(), b=rnd(); u16 x=0x1000+rnd()%0xE000, sp=0x1000+rnd()%0xE000; u8 cc=rnd()&0x3F;
            if (it%7==0) { a=0; }          /* corner values */
            if (it%11==0) { a=0x80; b=0x80; }
            if (it%13==0) { a=0x7F; b=0xFF; }
            if (it%17==0) { x=0x7FFF; }
            /* instruction at 0x0200 with sane operand bytes */
            u16 pc=0x0200;
            memInit[pc]=(u8)op;
            int hi=op>>4, lo=op&15;
            u8 o1=(u8)rnd(), o2=(u8)rnd();
            bool direct=(hi==0x9||hi==0xD), ext=(hi==0xB||hi==0xF||hi==7);
            if (direct) o1=0x80|(o1&0x7F);
            if (ext) { o1=0x10+(o1%0xD0); }
            memInit[pc+1]=o1; memInit[pc+2]=o2;
            memcpy(memA,memInit,65536); memcpy(memB,memInit,65536);
            logA.clear(); logB.clear();
            ref.m_program.m=ref.m_cprogram.m=ref.m_copcodes.m=memA; ref.m_program.log=&logA; ref.m_cprogram.log=ref.m_copcodes.log=nullptr;
            ref.m_pc.d=pc+1; ref.m_s.d=sp; ref.m_x.d=x; ref.m_d.b.h=a; ref.m_d.b.l=b; ref.m_cc=0xC0|cc; ref.m_wai_state=0;
            (ref.*(m6800_cpu_device::insn[op]))();
            cpu6803_t c; memset(&c,0,sizeof c);
            c.mem_read=mine_read; c.mem_write=mine_write;
            cpu6803_reset(&c);
            c.ram_enabled=false;
            c.pc=pc; c.sp=sp; c.x=x; c.a=a; c.b=b; c.cc=cc;
            int n=cpu6803_step(&c);
            (void)n;
            bool ok = c.pc==ref.m_pc.w.l && c.sp==ref.m_s.w.l && c.x==ref.m_x.w.l && c.a==ref.m_d.b.h && c.b==ref.m_d.b.l
                      && (c.cc&0x3F)==(ref.m_cc&0x3F) && logA==logB && !c.trapped;
            total++;
            if (!ok) { bad++; opbad++;
                if (opbad<=2) printf("op %%02X it %%d: pc %%04X/%%04X sp %%04X/%%04X x %%04X/%%04X a %%02X/%%02X b %%02X/%%02X cc %%02X/%%02X  trap=%%d logs %%zu/%%zu  in a=%%02X b=%%02X x=%%04X cc=%%02X o=%%02X %%02X\n",
                    op,it,c.pc,ref.m_pc.w.l,c.sp,ref.m_s.w.l,c.x,ref.m_x.w.l,c.a,ref.m_d.b.h,c.b,ref.m_d.b.l,c.cc&0x3F,ref.m_cc&0x3F,c.trapped,logB.size(),logA.size(),a,b,x,cc,o1,o2); }
        }
    }
    printf("%%ld tests, %%ld mismatches\n", total, bad);
    return bad!=0;
}
