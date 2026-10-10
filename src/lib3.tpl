#PRIM TRANSFORM 2 2
if isstr($2) = 0 then return argerr('TRANSFORM')
return mkstr(xtransform($1, cstr[$2]))
#PRIM QOUT 0 -1
txt = ''
do i = 1 to n
  if i > 1 then txt = txt || ' '
  txt = txt || valstr(fv[base + i])
end
if ctl <> 0 then return 0
if outstarted = 1 then call emitnl
call emit txt
return 0
#PRIM QQOUT 0 -1
txt = ''
do i = 1 to n
  if i > 1 then txt = txt || ' '
  txt = txt || valstr(fv[base + i])
end
if ctl <> 0 then return 0
call emit txt
return 0
#PRIM OUTSTD 0 -1
txt = ''
do i = 1 to n
  if i > 1 then txt = txt || ' '
  txt = txt || valstr(fv[base + i])
end
call emit txt
return 0
#PRIM OUTERR 0 -1
txt = ''
do i = 1 to n
  if i > 1 then txt = txt || ' '
  txt = txt || valstr(fv[base + i])
end
call flushout
call charout 'stderr', txt
return 0
#PRIM DEVOUT 1 2
call emit valstr($1)
return 0
#PRIM DEVPOS|SETPOS 2 2
call gotopos iarg($1, 0), iarg($2, 0)
return 0
#PRIM ROW 0 0
return mkint(scrrow)
#PRIM COL 0 0
return mkint(outcol)
#PRIM MAXROW 0 1
return mkint(24)
#PRIM MAXCOL 0 1
return mkint(79)
#PRIM SETCOLOR|COLORSELECT|SETCURSOR|SETBLINK|HB_GTINFO|SETMODE|SCROLL|DISPBEGIN|DISPEND|SETKEY|HB_SETKEYSAVE|HB_KEYPUT 0 -1
return mkstr('W/N')
#PRIM INKEY|HB_KEYSTD|HB_KEYINKEY 0 2
return mkint(readkey())
#PRIM NEXTKEY|LASTKEY 0 0
return mkint(0)
#PRIM ALERT 1 4
msg = vtostr($1)
call flushout
if outcol > 0 then call emitnl
call emit msg
call emitnl
opts = ''
if n >= 2 then do
  if isarr($2) = 1 then do
    do i = 1 to arrlen($2)
      if i > 1 then opts = opts || '  '
      opts = opts || '[' || i || '] ' || vtostr(arrget($2, i))
    end
  end
end
if opts <> '' then do
  call emit opts
  call emitnl
  s = readcon()
  k = 1
  if ineof = 0 & strip(s) <> '' then do
    if isdigitc(left(strip(s), 1)) = 1 then k = strip(s) as .int
    else do
      u = upper(left(strip(s), 1))
      do i = 1 to arrlen($2)
        if upper(left(vtostr(arrget($2, i)), 1)) == u then do
          k = i
          leave
        end
      end
    end
  end
  return mkint(k)
end
return mkint(1)
#PRIM SET 1 3
k = iarg($1, 0)
old = 0
if k = 1 then do
  old = mklog(setexact)
  if n >= 2 then setexact = truthy($2)
end
else if k = 2 then do
  old = mklog(setfixed)
  if n >= 2 then setfixed = truthy($2)
end
else if k = 3 then do
  old = mkint(setdec)
  if n >= 2 then setdec = iarg($2, 2)
end
else if k = 4 then do
  old = mkstr(setdatefmt)
  if n >= 2 then call setdate cstr[$2]
end
else if k = 5 then do
  old = mkint(setepoch)
  if n >= 2 then setepoch = iarg($2, 1900)
end
else if k = 9 then do
  old = mklog(setsoft)
  if n >= 2 then setsoft = truthy($2)
end
else if k = 11 then do
  old = mklog(setdeleted)
  if n >= 2 then setdeleted = truthy($2)
end
else if k = 17 then do
  old = mklog(setconsole)
  if n >= 2 then setconsole = truthy($2)
end
return old
#PRIM __SETCENTURY|SETCENTURY 0 1
old = mklog(setcentury)
if n >= 1 then do
  if islog($1) = 1 then setcentury = truthy($1)
end
return old
#PRIM FOPEN 1 2
if isstr($1) = 0 then return mkint(-1)
fname = cstr[$1]
if fileexists(fname) = 0 then return mkint(-1)
md = 0
if n >= 2 then md = iarg($2, 0) % 4
return mkint(newhandle(fname, bytesof(fname), md))
#PRIM FCREATE 1 2
if isstr($1) = 0 then return mkint(-1)
h = newhandle(cstr[$1], '', 1)
fhdirty[h - 2] = 1
junk = putbytes(cstr[$1], '')
return mkint(h)
#PRIM FCLOSE 1 1
return mklog(fclosehandle(iarg($1, 0)))
#PRIM FWRITE 2 3
i = iarg($1, 0) - 2
if i < 1 | i > nfh then return mkint(-1)
if isstr($2) = 0 then return mkint(0)
s = cstr[$2]
if n >= 3 then do
  k = iarg($3, length(s))
  if k < length(s) then s = left(s, k)
end
p = fhpos[i]
d = fhdata[i]
if p > length(d) then d = d || copies('00'x, p - length(d))
fhdata[i] = left(d, p) || s || substr(d, p + length(s) + 1)
fhpos[i] = p + length(s)
fhdirty[i] = 1
return mkint(length(s))
#PRIM FREAD 3 3
i = iarg($1, 0) - 2
if i < 1 | i > nfh then return mkint(0)
k = iarg($3, 0)
p = fhpos[i]
s = substr(fhdata[i], p + 1, k)
fhpos[i] = p + length(s)
r = $2
if r > 2 then do
  if ctag[r] = T_REF then ca1[r] = mkstr(s)
end
return mkint(length(s))
#PRIM FREADSTR 2 2
i = iarg($1, 0) - 2
if i < 1 | i > nfh then return mkstr('')
k = iarg($2, 0)
p = fhpos[i]
s = substr(fhdata[i], p + 1, k)
fhpos[i] = p + length(s)
return mkstr(s)
#PRIM FSEEK 2 3
i = iarg($1, 0) - 2
if i < 1 | i > nfh then return mkint(-1)
off = iarg($2, 0)
org = 0
if n >= 3 then org = iarg($3, 0)
if org = 0 then p = off
else if org = 1 then p = fhpos[i] + off
else p = length(fhdata[i]) + off
if p < 0 then p = 0
fhpos[i] = p
return mkint(p)
#PRIM HB_FEOF 1 1
i = iarg($1, 0) - 2
if i < 1 | i > nfh then return 2
if fhpos[i] >= length(fhdata[i]) then return 2
return 1
#PRIM FERROR 0 0
return mkint(0)
#PRIM FERASE 1 1
if isstr($1) = 0 then return mkint(-1)
if fileexists(cstr[$1]) = 0 then return mkint(-1)
junk = eraseFile(cstr[$1])
return mkint(0)
#PRIM FRENAME 2 2
if isstr($1) = 0 | isstr($2) = 0 then return mkint(-1)
if fileexists(cstr[$1]) = 0 then return mkint(-1)
junk = putbytes(cstr[$2], bytesof(cstr[$1]))
junk = eraseFile(cstr[$1])
return mkint(0)
#PRIM FILE|HB_FILEEXISTS 1 1
if isstr($1) = 0 then return 1
return mklog(fileexists(strip(cstr[$1])))
#PRIM MEMOREAD|HB_MEMOREAD 1 1
if isstr($1) = 0 then return mkstr('')
if fileexists(cstr[$1]) = 0 then return mkstr('')
t = loadtext(cstr[$1], '0a'x, 'noraise')
if right(t, 1) = '0a'x then t = substr(t, 1, length(t) - 1)
return mkstr(t)
#PRIM MEMOWRIT|HB_MEMOWRIT 2 2
if isstr($1) = 0 | isstr($2) = 0 then return 1
junk = eraseFile(cstr[$1])
junk = charout(cstr[$1], cstr[$2])
junk = charout(cstr[$1])
return 2
#PRIM HB_FREADLINE 1 2
i = iarg($1, 0) - 2
if i < 1 | i > nfh then return mkstr('')
d = fhdata[i]
p = fhpos[i]
if p >= length(d) then return mkstr('')
s = ''
q = pos('0a'x, d, p + 1)
if q = 0 then do
  s = substr(d, p + 1)
  fhpos[i] = length(d)
end
else do
  s = substr(d, p + 1, q - p - 1)
  fhpos[i] = q
end
if right(s, 1) = '0d'x then s = substr(s, 1, length(s) - 1)
return mkstr(s)
