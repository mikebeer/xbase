#PRIM DATE 0 0
d = date('S')
return mkdate(ymd2jdn(substr(d, 1, 4) as .int, substr(d, 5, 2) as .int, substr(d, 7, 2) as .int))
#PRIM TIME 0 0
return mkstr(time())
#PRIM SECONDS 0 0
t = time()
secs = (substr(t, 1, 2) as .int) * 3600 + (substr(t, 4, 2) as .int) * 60 + (substr(t, 7, 2) as .int)
return mknum(secs * 1.0, 10, 2)
#PRIM HB_MILLISECONDS 0 0
return mkint(time('US') as .int / 1000)
#PRIM YEAR 1 1
if isdate($1) = 0 then return argerr('YEAR')
if cint[$1] = 0 then return mkint(0)
call jdn2ymd cint[$1]
return mkint(gy)
#PRIM MONTH 1 1
if isdate($1) = 0 then return argerr('MONTH')
if cint[$1] = 0 then return mkint(0)
call jdn2ymd cint[$1]
return mkint(gm)
#PRIM DAY 1 1
if isdate($1) = 0 then return argerr('DAY')
if cint[$1] = 0 then return mkint(0)
call jdn2ymd cint[$1]
return mkint(gd)
#PRIM DOW 1 1
if isdate($1) = 0 then return argerr('DOW')
if cint[$1] = 0 then return mkint(0)
return mkint((cint[$1] + 1) % 7 + 1)
#PRIM CDOW 1 1
if isdate($1) = 0 then return argerr('CDOW')
if cint[$1] = 0 then return mkstr('')
k = (cint[$1] + 1) % 7 + 1
return mkstr(word('Sunday Monday Tuesday Wednesday Thursday Friday Saturday', k))
#PRIM CMONTH 1 1
if isdate($1) = 0 then return argerr('CMONTH')
if cint[$1] = 0 then return mkstr('')
call jdn2ymd cint[$1]
return mkstr(word('January February March April May June July August September October November December', gm))
#PRIM DTOC 1 1
if isdate($1) = 0 then return argerr('DTOC')
return mkstr(datestr(cint[$1]))
#PRIM DTOS 1 1
if isdate($1) = 0 then return argerr('DTOS')
return mkstr(dtos(cint[$1]))
#PRIM CTOD 1 1
if isstr($1) = 0 then return argerr('CTOD')
fmt = setdatefmt
if setcentury = 1 & pos('YYYY', upper(fmt)) = 0 then fmt = changestr('yy', fmt, 'yyyy')
return mkdate(parsedate(cstr[$1], fmt))
#PRIM STOD|HB_STOD 0 1
if n < 1 then return mkdate(0)
if isstr($1) = 0 then return argerr('STOD')
return datelit(strip(cstr[$1]))
#PRIM HB_DATE 3 3
return mkdate(ymd2jdn(iarg($1, 1900), iarg($2, 1), iarg($3, 1)))
#PRIM ARRAY 0 -1
dims = n
if dims = 0 then return mkarray(0)
return mkmulti(base, 1, n)
#PRIM AADD 2 2
if isarr($1) = 0 then return argerr('AADD')
call arrpush $1, $2
return $2
#PRIM ADEL 2 2
if isarr($1) = 0 | isnum($2) = 0 then return argerr('ADEL')
p = toint(numval($2))
len = arrlen($1)
if p < 1 | p > len then return $1
do i = p to len - 1
  call arrset $1, i, arrget($1, i + 1)
end
call arrset $1, len, 0
return $1
#PRIM AINS 2 2
if isarr($1) = 0 | isnum($2) = 0 then return argerr('AINS')
p = toint(numval($2))
len = arrlen($1)
if p < 1 | p > len then return $1
do i = len to p + 1 by -1
  call arrset $1, i, arrget($1, i - 1)
end
call arrset $1, p, 0
return $1
#PRIM HB_AINS 3 4
if isarr($1) = 0 | isnum($2) = 0 then return argerr('HB_AINS')
p = toint(numval($2))
len = arrlen($1)
if n >= 4 then do
  if $4 = 2 then do
    call arrresize $1, len + 1
    len = len + 1
  end
end
if p < 1 | p > len then return $1
do i = len to p + 1 by -1
  call arrset $1, i, arrget($1, i - 1)
end
call arrset $1, p, $3
return $1
#PRIM HB_ADEL 2 3
if isarr($1) = 0 | isnum($2) = 0 then return argerr('HB_ADEL')
p = toint(numval($2))
len = arrlen($1)
if p < 1 | p > len then return $1
do i = p to len - 1
  call arrset $1, i, arrget($1, i + 1)
end
if n >= 3 then do
  if $3 = 2 then do
    call arrresize $1, len - 1
    return $1
  end
end
call arrset $1, len, 0
return $1
#PRIM ASIZE 2 2
if isarr($1) = 0 | isnum($2) = 0 then return argerr('ASIZE')
k = toint(numval($2))
if k < 0 then k = 0
call arrresize $1, k
return $1
#PRIM ATAIL 1 1
if isarr($1) = 0 then return argerr('ATAIL')
if arrlen($1) = 0 then return 0
return arrget($1, arrlen($1))
#PRIM ASCAN|HB_ASCAN 2 4
if isarr($1) = 0 then return argerr('ASCAN')
st = 1
if n >= 3 then st = iarg($3, 1)
len = arrlen($1)
cnt = len - st + 1
if n >= 4 then cnt = iarg($4, cnt)
if st < 1 then st = 1
en = st + cnt - 1
if en > len then en = len
do i = st to en
  el = arrget($1, i)
  if isblk($2) = 1 then do
    r = evalblock1($2, el)
    if ctl <> 0 then return 0
    if r = 2 then return mkint(i)
  end
  else do
    if isstr($2) = 1 & isstr(el) = 1 then do
      if cstr[$2] == cstr[el] then return mkint(i)
    end
    else do
      if keyeq(el, $2) = 1 then return mkint(i)
      if isnum(el) = 1 & isnum($2) = 1 then do
        if numval(el) = numval($2) then return mkint(i)
      end
    end
  end
end
return mkint(0)
#PRIM AEVAL 2 4
if isarr($1) = 0 | isblk($2) = 0 then return argerr('AEVAL')
st = 1
if n >= 3 then st = iarg($3, 1)
len = arrlen($1)
cnt = len - st + 1
if n >= 4 then cnt = iarg($4, cnt)
if st < 1 then st = 1
en = st + cnt - 1
if en > len then en = len
do i = st to en
  junk = evalblock2($2, arrget($1, i), mkint(i))
  if ctl <> 0 then return 0
end
return $1
#PRIM ASORT 1 4
if isarr($1) = 0 then return argerr('ASORT')
st = 1
if n >= 2 then st = iarg($2, 1)
len = arrlen($1)
cnt = len - st + 1
if n >= 3 then cnt = iarg($3, cnt)
blk = 0
if n >= 4 then do
  if isblk($4) = 1 then blk = $4
end
if st < 1 then st = 1
if st + cnt - 1 > len then cnt = len - st + 1
if cnt > 1 then call msort cint[$1] + st - 1, cnt, blk
return $1
#PRIM ACLONE 1 1
if isarr($1) = 0 then return argerr('ACLONE')
return cloneval($1)
#PRIM ACOPY 2 5
if isarr($1) = 0 | isarr($2) = 0 then return argerr('ACOPY')
st = 1
if n >= 3 then st = iarg($3, 1)
len = arrlen($1)
cnt = len - st + 1
if n >= 4 then cnt = iarg($4, cnt)
dp = 1
if n >= 5 then dp = iarg($5, 1)
do i = 0 to cnt - 1
  if st + i <= len & dp + i <= arrlen($2) then call arrset $2, dp + i, arrget($1, st + i)
end
return $2
#PRIM AFILL 2 4
if isarr($1) = 0 then return argerr('AFILL')
st = 1
if n >= 3 then st = iarg($3, 1)
len = arrlen($1)
cnt = len - st + 1
if n >= 4 then cnt = iarg($4, cnt)
do i = st to st + cnt - 1
  if i >= 1 & i <= len then call arrset $1, i, $2
end
return $1
#PRIM HB_ATOKENS 1 3
if isstr($1) = 0 then return argerr('HB_ATOKENS')
d = ' '
if n >= 2 then do
  if isstr($2) = 1 then d = cstr[$2]
end
r = mkarray(0)
s = cstr[$1]
if d == '' then d = ' '
do forever
  p = pos(d, s)
  if p = 0 then do
    call arrpush r, mkstr(s)
    leave
  end
  call arrpush r, mkstr(substr(s, 1, p - 1))
  s = substr(s, p + length(d))
end
return r
#PRIM HB_HASH 0 -1
h = mkhash()
i = 1
do while i < n
  call hset h, $1, $2
  i = i + 2
end
return h
#PRIM HB_HSET 3 3
if ishash($1) = 0 then return argerr('HB_HSET')
call hset $1, $2, $3
return $1
#PRIM HB_HGET 2 2
if ishash($1) = 0 then return argerr('HB_HGET')
p = hfind($1, $2)
if p = 0 then return rterr('BASE', 1132, 'Bound error', 'hash access')
return arrget(ca2[$1], p)
#PRIM HB_HGETDEF 2 3
if ishash($1) = 0 then return argerr('HB_HGETDEF')
p = hfind($1, $2)
if p = 0 then return $3
return arrget(ca2[$1], p)
#PRIM HB_HHASKEY 2 2
if ishash($1) = 0 then return argerr('HB_HHASKEY')
return mklog(hfind($1, $2) > 0)
#PRIM HB_HDEL 2 2
if ishash($1) = 0 then return argerr('HB_HDEL')
p = hfind($1, $2)
if p > 0 then call hdel $1, p
return $1
#PRIM HB_HKEYS 1 1
if ishash($1) = 0 then return argerr('HB_HKEYS')
r = mkarray(0)
do i = 1 to hashlen($1)
  call arrpush r, arrget(ca1[$1], i)
end
return r
#PRIM HB_HVALUES 1 1
if ishash($1) = 0 then return argerr('HB_HVALUES')
r = mkarray(0)
do i = 1 to hashlen($1)
  call arrpush r, arrget(ca2[$1], i)
end
return r
#PRIM HB_HPOS 2 2
if ishash($1) = 0 then return argerr('HB_HPOS')
return mkint(hfind($1, $2))
#PRIM HB_HKEYAT 2 2
if ishash($1) = 0 | isnum($2) = 0 then return argerr('HB_HKEYAT')
p = toint(numval($2))
if p < 1 | p > hashlen($1) then return rterr('BASE', 1187, 'Bound error', 'hash access')
return arrget(ca1[$1], p)
#PRIM HB_HVALUEAT 2 3
if ishash($1) = 0 | isnum($2) = 0 then return argerr('HB_HVALUEAT')
p = toint(numval($2))
if p < 1 | p > hashlen($1) then return rterr('BASE', 1187, 'Bound error', 'hash access')
if n >= 3 then call arrset ca2[$1], p, $3
return arrget(ca2[$1], p)
#PRIM HB_HCLONE 1 1
if ishash($1) = 0 then return argerr('HB_HCLONE')
return cloneval($1)
#PRIM HB_HMERGE 2 2
if ishash($1) = 0 | ishash($2) = 0 then return argerr('HB_HMERGE')
do i = 1 to hashlen($2)
  call hset $1, arrget(ca1[$2], i), arrget(ca2[$2], i)
end
return $1
#PRIM EVAL 1 -1
if isblk($1) = 0 then return rterr('BASE', 1004, 'Argument error', 'EVAL')
b = ftop
k = n - 1
do i = 1 to k
  fv[b + i] = fv[base + i + 1]
end
ftop = b + k
return callblock($1, b, k)
#PRIM DO 1 -1
nm = $1
k = n - 1
a = mkarray(k)
do i = 1 to k
  call arrset a, i, fv[base + i + 1]
end
if isblk(nm) = 1 then do
  b = ftop
  do i = 1 to k
    fv[b + i] = arrget(a, i)
  end
  ftop = b + k
  return callblock(nm, b, k)
end
if isstr(nm) = 0 then return argerr('DO')
return callsym(intern(upper(strip(cstr[nm]))), a)
#PRIM BREAK|THROW|HB_THROW 0 1
brkval = $1
if ctl = 0 then ctl = E_BRK
return 0
#PRIM ERRORBLOCK 0 1
old = errblk
if n >= 1 then do
  if isblk($1) = 1 then errblk = $1
end
return old
#PRIM ERRORNEW 0 0
return mkerror('BASE', 0, '', '')
#PRIM PROCNAME 0 1
k = 0
if n >= 1 then k = iarg($1, 0)
idx = pctop - k
if idx < 1 then return mkstr('')
return mkstr(pcname[idx])
#PRIM PROCLINE 0 1
k = 0
if n >= 1 then k = iarg($1, 0)
idx = pctop - k
if idx < 1 then return mkint(0)
return mkint(pcln[idx])
#PRIM HB_ARGC 0 0
return mkint(nargvs)
#PRIM HB_ARGV 1 1
k = iarg($1, 0)
if k < 1 | k > nargvs then return mkstr('')
return mkstr(argvs[k])
#PRIM HB_PVALUE|PVALUE 1 1
k = iarg($1, 0)
if k < 1 | k > fpc then return 0
return fv[fbase + k]
#PRIM VERSION|HB_VERSION 0 1
return mkstr('xBase for cREXX 1.0')
#PRIM OS 0 0
return mkstr('Linux')
#PRIM GETENV|HB_GETENV 1 2
return mkstr('')
#PRIM ERRORLEVEL 0 1
old = exitcode
if n >= 1 then exitcode = iarg($1, 0)
return mkint(old)
#PRIM HB_DIRSEPARATOR|HB_PS 0 0
return mkstr('/')
#PRIM HB_CWD 0 0
return mkstr('.')
#PRIM VALTOPRG|HB_VALTOEXP 1 1
return mkstr(vtoexp($1))
#PRIM HB_STRREPLACE 2 3
if isstr($1) = 0 then return argerr('HB_STRREPLACE')
t = cstr[$1]
if n >= 3 & isstr($2) = 1 & isstr($3) = 1 then do
  r = ''
  do i = 1 to length(t)
    ch = substr(t, i, 1)
    k = pos(ch, cstr[$2])
    if k > 0 & k <= length(cstr[$3]) then ch = substr(cstr[$3], k, 1)
    r = r || ch
  end
  return mkstr(r)
end
if isarr($2) = 1 then do
  do k = 1 to arrlen($2)
    f = arrget($2, k)
    if isstr(f) = 1 then do
      rep = ''
      if n >= 3 then do
        if isarr($3) = 1 then do
          if k <= arrlen($3) then rep = sarg(arrget($3, k))
        end
        else if isstr($3) = 1 then rep = cstr[$3]
      end
      if cstr[f] <> '' then t = changestr(cstr[f], t, rep)
    end
  end
end
return mkstr(t)
#PRIM HB_STRFORMAT 1 -1
if isstr($1) = 0 then return argerr('HB_STRFORMAT')
return mkstr(strformat(base, n))
#PRIM DESCEND 1 1
v = $1
if isstr(v) = 1 then do
  s = cstr[v]
  r = ''
  do i = 1 to length(s)
    r = r || d2c(255 - c2d(substr(s, i, 1)))
  end
  return mkstr(r)
end
if isnum(v) = 1 then return numneg(v)
return v
#PRIM SOUNDEX 1 1
if isstr($1) = 0 then return argerr('SOUNDEX')
u = upper(strip(cstr[$1]))
if u == '' then return mkstr('0000')
codes = '01230120022455012623010202'
r = left(u, 1)
last = 0
p0 = c2d(r) - 64
if p0 >= 1 & p0 <= 26 then last = substr(codes, p0, 1) as .int
do i = 2 to length(u)
  ch = substr(u, i, 1)
  q = c2d(ch) - 64
  if q < 1 | q > 26 then iterate
  cd = substr(codes, q, 1) as .int
  if cd > 0 & cd <> last then r = r || cd
  if ch <> 'H' & ch <> 'W' then last = cd
  if length(r) = 4 then leave
end
return mkstr(left(r || '000', 4))
#PRIM HB_CSTR|HB_VALTOSTR2 1 1
return mkstr(vtostr($1))
