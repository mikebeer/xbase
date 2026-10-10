#PRIM LEN 1 1
v = $1
if isstr(v) = 1 then return mkint(length(cstr[v]))
if isarr(v) = 1 then return mkint(arrlen(v))
if ishash(v) = 1 then return mkint(hashlen(v))
return argerr('LEN')
#PRIM EMPTY 1 1
v = $1
if v = 0 then return 2
if v < 0 then do
  if v = mkint(0) then return 2
  return 1
end
if v = 1 then return 2
if v = 2 then return 1
t = ctag[v]
if t = T_STR then do
  if strip(cstr[v]) == '' then return 2
  return 1
end
if t = T_NUM then do
  if cnum[v] = 0.0 then return 2
  return 1
end
if t = T_DATE then do
  if cint[v] = 0 then return 2
  return 1
end
if t = T_ARR then do
  if arrlen(v) = 0 then return 2
  return 1
end
if t = T_HASH then do
  if hashlen(v) = 0 then return 2
  return 1
end
return 1
#PRIM VALTYPE 1 1
return mkstr(typech($1))
#PRIM STR 1 3
v = $1
if isnum(v) = 0 then return argerr('STR')
w = -1
d = -1
if n >= 2 then do
  if isnum($2) = 1 then w = toint(numval($2))
end
if n >= 3 then do
  if isnum($3) = 1 then d = toint(numval($3))
end
if w >= 0 & d < 0 then d = 0
return mkstr(numstrw(v, w, d))
#PRIM STRZERO 1 3
v = $1
if isnum(v) = 0 then return argerr('STRZERO')
w = 10
d = 0
if n >= 2 then w = iarg($2, 10)
if n >= 3 then d = iarg($3, 0)
s = numstrw(v, w, d)
neg = 0
if left(strip(s), 1) = '-' then do
  neg = 1
  s = substr(strip(s), 2)
end
s = strip(s)
do while length(s) < w - neg
  s = '0' || s
end
if neg = 1 then s = '-' || s
return mkstr(s)
#PRIM VAL 1 1
v = $1
if isstr(v) = 0 then return argerr('VAL')
return valnum(cstr[v])
#PRIM ALLTRIM 1 1
if isstr($1) = 0 then return argerr('ALLTRIM')
return mkstr(strip(cstr[$1]))
#PRIM RTRIM|TRIM 1 2
if isstr($1) = 0 then return argerr('RTRIM')
return mkstr(strip(cstr[$1], 'T'))
#PRIM LTRIM 1 1
if isstr($1) = 0 then return argerr('LTRIM')
return mkstr(strip(cstr[$1], 'L'))
#PRIM LEFT 2 2
if isstr($1) = 0 | isnum($2) = 0 then return argerr('LEFT')
k = toint(numval($2))
if k <= 0 then return mkstr('')
return mkstr(left(cstr[$1], k))
#PRIM RIGHT 2 2
if isstr($1) = 0 | isnum($2) = 0 then return argerr('RIGHT')
k = toint(numval($2))
if k <= 0 then return mkstr('')
s = cstr[$1]
if k >= length(s) then return $1
return mkstr(right(s, k))
#PRIM SUBSTR 2 3
if isstr($1) = 0 | isnum($2) = 0 then return argerr('SUBSTR')
ln = -1
if n >= 3 then do
  if isnum($3) = 0 then return argerr('SUBSTR')
  ln = toint(numval($3))
end
return mkstr(xsubstr(cstr[$1], toint(numval($2)), ln))
#PRIM UPPER 1 1
if isstr($1) = 0 then return argerr('UPPER')
return mkstr(upper(cstr[$1]))
#PRIM LOWER 1 1
if isstr($1) = 0 then return argerr('LOWER')
return mkstr(lower(cstr[$1]))
#PRIM SPACE 1 1
if isnum($1) = 0 then return argerr('SPACE')
return mkstr(xaspace(toint(numval($1))))
#PRIM REPLICATE|REPL 2 2
if isstr($1) = 0 | isnum($2) = 0 then return argerr('REPLICATE')
k = toint(numval($2))
if k <= 0 then return mkstr('')
return mkstr(copies(cstr[$1], k))
#PRIM PADR 2 3
if isnum($2) = 0 then return argerr('PADR')
s = ''
if isstr($1) = 1 then s = cstr[$1]
else if isnum($1) = 1 then s = strip(numstr($1))
else if isdate($1) = 1 then s = datestr(cint[$1])
else return argerr('PADR')
k = toint(numval($2))
f = ' '
if n >= 3 then do
  if isstr($3) = 1 then f = left(cstr[$3] || ' ', 1)
end
if k <= 0 then return mkstr('')
if length(s) >= k then return mkstr(left(s, k))
return mkstr(s || copies(f, k - length(s)))
#PRIM PADL 2 3
if isnum($2) = 0 then return argerr('PADL')
s = ''
if isstr($1) = 1 then s = cstr[$1]
else if isnum($1) = 1 then s = strip(numstr($1))
else if isdate($1) = 1 then s = datestr(cint[$1])
else return argerr('PADL')
k = toint(numval($2))
f = ' '
if n >= 3 then do
  if isstr($3) = 1 then f = left(cstr[$3] || ' ', 1)
end
if k <= 0 then return mkstr('')
if length(s) >= k then return mkstr(left(s, k))
return mkstr(copies(f, k - length(s)) || s)
#PRIM PADC 2 3
if isnum($2) = 0 then return argerr('PADC')
s = ''
if isstr($1) = 1 then s = cstr[$1]
else if isnum($1) = 1 then s = strip(numstr($1))
else if isdate($1) = 1 then s = datestr(cint[$1])
else return argerr('PADC')
k = toint(numval($2))
f = ' '
if n >= 3 then do
  if isstr($3) = 1 then f = left(cstr[$3] || ' ', 1)
end
if k <= 0 then return mkstr('')
if length(s) >= k then return mkstr(left(s, k))
tot = k - length(s)
lft = tot / 2
return mkstr(copies(f, lft) || s || copies(f, tot - lft))
#PRIM AT|HB_AT 2 4
if isstr($1) = 0 | isstr($2) = 0 then return argerr('AT')
st = 1
en = 0
if n >= 3 then st = iarg($3, 1)
if n >= 4 then en = iarg($4, 0)
return mkint(xat(cstr[$1], cstr[$2], st, en))
#PRIM RAT 2 2
if isstr($1) = 0 | isstr($2) = 0 then return argerr('RAT')
return mkint(xrat(cstr[$1], cstr[$2]))
#PRIM STRTRAN 2 5
if isstr($1) = 0 | isstr($2) = 0 then return argerr('STRTRAN')
s = cstr[$1]
f = cstr[$2]
r = ''
if n >= 3 then do
  if isstr($3) = 1 then r = cstr[$3]
end
st = 1
mx = 0
if n >= 4 then st = iarg($4, 1)
if n >= 5 then mx = iarg($5, 0)
if f == '' then return $1
out = ''
rest = s
cnt = 0
occ = 0
do forever
  p = pos(f, rest)
  if p = 0 then leave
  occ = occ + 1
  if occ < st then do
    out = out || substr(rest, 1, p + length(f) - 1)
    rest = substr(rest, p + length(f))
    iterate
  end
  if mx > 0 & cnt >= mx then leave
  out = out || substr(rest, 1, p - 1) || r
  rest = substr(rest, p + length(f))
  cnt = cnt + 1
end
return mkstr(out || rest)
#PRIM STUFF 4 4
if isstr($1) = 0 | isnum($2) = 0 | isnum($3) = 0 | isstr($4) = 0 then return argerr('STUFF')
s = cstr[$1]
st = toint(numval($2))
dl = toint(numval($3))
if st < 1 then st = 1
if st > length(s) + 1 then st = length(s) + 1
return mkstr(substr(s, 1, st - 1) || cstr[$4] || substr(s, st + dl))
#PRIM CHR 1 1
if isnum($1) = 0 then return argerr('CHR')
k = toint(numval($1))
if k < 0 then k = k + 256
return mkstr(d2c(k % 256))
#PRIM ASC 1 1
if isstr($1) = 0 then return argerr('ASC')
if cstr[$1] == '' then return mkint(0)
return mkint(c2d(left(cstr[$1], 1)))
#PRIM ABS 1 1
if isnum($1) = 0 then return argerr('ABS')
if $1 < 0 then return mkint(abs(intval($1)))
c = newcell(T_NUM)
x = cnum[$1]
if x < 0.0 then x = 0.0 - x
cnum[c] = x
cint[c] = cint[$1]
return c
#PRIM INT 1 1
if isnum($1) = 0 then return argerr('INT')
if $1 < 0 then return $1
x = cnum[$1]
if x < 9.0e15 & x > -9.0e15 then return mkint(toint(x))
return mknum(x, 20, 0)
#PRIM ROUND 2 2
if isnum($1) = 0 | isnum($2) = 0 then return argerr('ROUND')
d = toint(numval($2))
if d < 0 then do
  f = fpow(10.0, 0.0 - d)
  x = numval($1) / f
  s = fmtfloat(x, 0)
  return mknum((s as .float) * f, 10, 0)
end
if $1 < 0 then return $1
s = fmtfloat(cnum[$1], d)
return mknum(s as .float, numwid($1), d)
#PRIM SQRT 1 1
if isnum($1) = 0 then return argerr('SQRT')
return mknum(lsqrt(numval($1)), 10, setdec)
#PRIM EXP 1 1
if isnum($1) = 0 then return argerr('EXP')
return mknum(lexp(numval($1)), 10, setdec)
#PRIM LOG 1 1
if isnum($1) = 0 then return argerr('LOG')
if numval($1) <= 0.0 then return mknum(0.0, 10, setdec)
return mknum(lln(numval($1)), 10, setdec)
#PRIM SIN 1 1
if isnum($1) = 0 then return argerr('SIN')
return mknum(lsin(numval($1)), 10, setdec)
#PRIM COS 1 1
if isnum($1) = 0 then return argerr('COS')
return mknum(lcos(numval($1)), 10, setdec)
#PRIM MAX 2 2
a = $1
b = $2
r = cmpv(a, b)
if r = 2 then return argerr('MAX')
if r >= 0 then return a
return b
#PRIM MIN 2 2
a = $1
b = $2
r = cmpv(a, b)
if r = 2 then return argerr('MIN')
if r <= 0 then return a
return b
#PRIM MOD 2 2
if isnum($1) = 0 | isnum($2) = 0 then return argerr('MOD')
y = numval($2)
if y = 0.0 then return rterr('BASE', 1341, 'Zero divisor', 'MOD')
x = numval($1)
q = toint(x / y)
if (x / y) < 0.0 & q * 1.0 <> x / y then q = q - 1
r = x - (q * 1.0) * y
d = numdec($1)
if numdec($2) > d then d = numdec($2)
return mknum(r, 10, d)
#PRIM PCOUNT 0 0
return mkint(fpc)
#PRIM HB_NTOS|NTOS 1 1
if isnum($1) = 0 then return argerr('HB_NTOS')
return mkstr(ntos($1))
#PRIM HB_VALTOSTR 1 1
return mkstr(vtostr($1))
#PRIM ISALPHA 1 1
if isstr($1) = 0 then return 1
if cstr[$1] == '' then return 1
return mklog(isalphac(left(cstr[$1], 1)))
#PRIM ISDIGIT 1 1
if isstr($1) = 0 then return 1
if cstr[$1] == '' then return 1
return mklog(isdigitc(left(cstr[$1], 1)))
#PRIM ISUPPER 1 1
if isstr($1) = 0 then return 1
if cstr[$1] == '' then return 1
c = left(cstr[$1], 1)
return mklog(isalphac(c) = 1 & c == upper(c))
#PRIM ISLOWER 1 1
if isstr($1) = 0 then return 1
if cstr[$1] == '' then return 1
c = left(cstr[$1], 1)
return mklog(isalphac(c) = 1 & c == lower(c))
#PRIM HB_ISSTRING 1 1
return mklog(isstr($1))
#PRIM HB_ISNUMERIC 1 1
return mklog(isnum($1))
#PRIM HB_ISARRAY 1 1
return mklog(isarr($1))
#PRIM HB_ISHASH 1 1
return mklog(ishash($1))
#PRIM HB_ISLOGICAL 1 1
return mklog(islog($1))
#PRIM HB_ISDATE 1 1
return mklog(isdate($1))
#PRIM HB_ISBLOCK 1 1
return mklog(isblk($1))
#PRIM HB_ISOBJECT 1 1
return mklog(isobj($1))
#PRIM HB_ISNIL 1 1
return mklog($1 = 0)
#PRIM HB_DEFAULT 2 2
r = $1
if r > 2 then do
  if ctag[r] = T_REF then do
    if ca1[r] = 0 then ca1[r] = $2
    return 0
  end
end
return 0
#PRIM RANDOM|HB_RANDOM 0 1
x = nextrand() / 2147483648.0
if n >= 1 then do
  if isnum($1) = 1 then x = x * numval($1)
end
return mknum(x, 10, 8)
#PRIM HB_RANDOMINT 0 2
r = nextrand()
if n = 0 then return mkint(r)
if n = 1 then do
  m = iarg($1, 1)
  if m < 1 then m = 1
  return mkint(r % m)
end
lo = iarg($1, 0)
hi = iarg($2, 1)
if hi < lo then hi = lo
return mkint(lo + r % (hi - lo + 1))
#PRIM HB_BITAND 2 2
return mkint(bitop(1, iarg($1, 0), iarg($2, 0)))
#PRIM HB_BITOR 2 2
return mkint(bitop(2, iarg($1, 0), iarg($2, 0)))
#PRIM HB_BITXOR 2 2
return mkint(bitop(3, iarg($1, 0), iarg($2, 0)))
#PRIM HB_BITSHIFT 2 2
k = iarg($2, 0)
v = iarg($1, 0)
if k >= 0 then do
  do k
    v = v * 2
  end
end
else do
  do 0 - k
    v = v / 2
  end
end
return mkint(v)
