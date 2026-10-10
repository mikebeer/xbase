import re,sys,os,glob
S=os.path.dirname(os.path.abspath(__file__))
def rd(n): return open(os.path.join(S,n)).read()
# ---- constants
consts={}
auto=None
for line in rd('consts.txt').split('\n'):
    line=line.split('#')[0].strip() if not line.startswith('@') else line.strip()
    if not line: continue
    if line.startswith('@'):
        auto=int(line[1:].split()[1]) if len(line.split())>1 else 1
        prefix=line[1:].split()[0]
        continue
    parts=line.split()
    if len(parts)==2:
        consts[parts[0]]=int(parts[1])
    else:
        consts[parts[0]]=auto; auto+=1
def subst(text):
    def f(m):
        k=m.group(0)
        if k not in consts: return k
        return str(consts[k])
    return re.sub(r'\b[KTEC]_[A-Z0-9_]+\b',f,text)
# ---- prims
prims=[]
for f in sorted(glob.glob(os.path.join(S,'lib*.tpl'))):
    t=open(f).read()
    for blk in re.split(r'(?m)^(?=#PRIM )',t):
        if not blk.strip(): continue
        lines=blk.split('\n')
        hd=lines[0].split()
        names=hd[1].split('|'); mn=int(hd[2]); mx=int(hd[3])
        body='\n'.join(lines[1:]).rstrip('\n')
        prims.append((names,mn,mx,body))
def pbody(b):
    b=re.sub(r'\$(\d)',lambda m:'fv[base + %s]'%m.group(1),b)
    return b
out=[]
out.append('/* ===================================================================== */\n/*                       BUILTIN FUNCTIONS                               */\n/* ===================================================================== */')
for i,(names,mn,mx,body) in enumerate(prims,1):
    out.append('/* %s */' % ' '.join(names))
    out.append('pf%d: procedure = .int' % i)
    out.append('  arg base = .int, n = .int')
    for l in pbody(body).split('\n'):
        out.append('  '+l if l.strip() else '')
    out.append('  return 0')
    out.append('')
G=30
ng=(len(prims)+G-1)//G
for g in range(ng):
    out.append('primg%d: procedure = .int' % g)
    out.append('  arg id = .int, base = .int, n = .int')
    for i in range(g*G+1,min(len(prims),(g+1)*G)+1):
        out.append('  if id = %d then return pf%d(base, n)' % (i,i))
    out.append('  return 0')
    out.append('')
out.append('callprim: procedure = .int')
out.append('  arg id = .int, base = .int, n = .int')
out.append('  g = (id - 1) / %d' % G)
for g in range(ng):
    out.append('  if g = %d then return primg%d(id, base, n)' % (g,g))
out.append('  return 0')
out.append('')
out.append('initprims: procedure = .void')
for i,(names,mn,mx,body) in enumerate(prims,1):
    for nm in names:
        out.append("  call registerprim '%s', %d, %d, %d" % (nm.upper(),i,mn,mx))
out.append('  return')
out.append('')
gen='\n'.join(out)+'\n'
# ---- parts
files=sorted(glob.glob(os.path.join(S,'[0-9][0-9]_*.crexx')))
parts=[]
for f in files: parts.append(open(f).read())
allsrc='\n'.join(parts)+'\n'+gen
names=[]
for line in allsrc.split('\n'):
    m=re.match(r'^([a-zA-Z][a-zA-Z0-9_]*)\s*=\s',line)
    if m and m.group(1) not in names: names.append(m.group(1))
allsrc=re.sub(r'namespace xbase expose .*','namespace xbase expose '+' '.join(names),allsrc,count=1)
allsrc=subst(allsrc)
open(sys.argv[1],'w').write(allsrc)
print(len(prims),'prims',len(names),'names')
