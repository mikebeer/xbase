#PRIM RECNO 0 0
if curarea < 1 then return mkint(0)
if wa_used[curarea] = 0 then return mkint(0)
return mkint(wa_recno[curarea])
#PRIM EOF 0 0
if curarea < 1 then return 2
if wa_used[curarea] = 0 then return 2
return mklog(wa_eof[curarea])
#PRIM BOF 0 0
if curarea < 1 then return 2
if wa_used[curarea] = 0 then return 2
return mklog(wa_bof[curarea])
#PRIM FOUND 0 0
if curarea < 1 then return 1
if wa_used[curarea] = 0 then return 1
return mklog(wa_found[curarea])
#PRIM DELETED 0 0
if curarea < 1 then return 1
if wa_used[curarea] = 0 then return 1
return mklog(isdeleted(curarea))
#PRIM LASTREC|RECCOUNT 0 0
if curarea < 1 then return mkint(0)
if wa_used[curarea] = 0 then return mkint(0)
return mkint(wa_nrec[curarea])
#PRIM FCOUNT 0 0
if curarea < 1 then return mkint(0)
if wa_used[curarea] = 0 then return mkint(0)
return mkint(wa_nfld[curarea])
#PRIM FIELDNAME|FIELD 1 2
a = curarea
if n = 2 then a = areaarg($2)
if a < 1 then return mkstr('')
if wa_used[a] = 0 then return mkstr('')
i = iarg($1, 0)
if i < 1 | i > wa_nfld[a] then return mkstr('')
return mkstr(fnam[(a - 1) * 256 + i])
#PRIM FIELDPOS 1 2
a = curarea
if n = 2 then a = areaarg($2)
if a < 1 then return mkint(0)
if wa_used[a] = 0 then return mkint(0)
return mkint(fieldfind(a, intern(upper(strip(sarg($1))))))
#PRIM FIELDGET 1 2
a = curarea
if n = 2 then a = areaarg($2)
if a < 1 then return 0
if wa_used[a] = 0 then return 0
i = iarg($1, 0)
if i < 1 | i > wa_nfld[a] then return 0
return fieldval(a, i)
#PRIM FIELDPUT 2 3
a = curarea
if n = 3 then a = areaarg($3)
if a < 1 then return 0
if wa_used[a] = 0 then return 0
i = iarg($1, 0)
if i < 1 | i > wa_nfld[a] then return 0
call fieldput a, i, $2
return $2
#PRIM HB_FIELDLEN|FIELDLEN 1 2
a = curarea
if a < 1 then return mkint(0)
i = iarg($1, 0)
if i < 1 | i > wa_nfld[a] then return mkint(0)
return mkint(flen[(a - 1) * 256 + i])
#PRIM HB_FIELDDEC|FIELDDEC 1 2
a = curarea
if a < 1 then return mkint(0)
i = iarg($1, 0)
if i < 1 | i > wa_nfld[a] then return mkint(0)
return mkint(fdec[(a - 1) * 256 + i])
#PRIM HB_FIELDTYPE|FIELDTYPE 1 2
a = curarea
if a < 1 then return mkstr('')
i = iarg($1, 0)
if i < 1 | i > wa_nfld[a] then return mkstr('')
return mkstr(ftyp[(a - 1) * 256 + i])
#PRIM ALIAS 0 1
a = curarea
if n = 1 then a = areaarg($1)
if a < 1 then return mkstr('')
if wa_used[a] = 0 then return mkstr('')
return mkstr(wa_alias[a])
#PRIM SELECT 0 1
if n = 0 then return mkint(curarea)
if isstr($1) = 1 then do
  u = upper(strip(cstr[$1]))
  if u == '' then return mkint(curarea)
  return mkint(aliasarea(u))
end
return mkint(iarg($1, 0))
#PRIM DBSELECTAREA 1 1
if isstr($1) = 1 then do
  a = aliasarea(upper(strip(cstr[$1])))
  if a = 0 then return rterr('BASE', 1002, 'Alias does not exist', cstr[$1])
  curarea = a
end
else do
  k = iarg($1, 0)
  if k = 0 then k = findfreearea()
  curarea = k
end
return 0
#PRIM USED 0 0
if curarea < 1 then return 1
return mklog(wa_used[curarea])
#PRIM NETERR 0 1
return 1
#PRIM RLOCK|FLOCK|DBRLOCK|DBFLOCK 0 2
return 2
#PRIM DBUNLOCK|DBUNLOCKALL|DBRUNLOCK|DBCOMMITALL 0 1
return 0
#PRIM DBCOMMIT 0 0
if curarea > 0 then do
  call flushdbf curarea
  call flushindexes curarea
end
return 0
#PRIM DBUSEAREA 0 8
/* DbUseArea( [lNew], [cDriver], cFile, [cAlias], [lShared], [lReadOnly] ) */
newf = 0
if n >= 1 then do
  if islog($1) = 1 then newf = $1 - 1
end
if n < 3 then do
  if curarea > 0 then call closearea curarea
  return 0
end
f = sarg($3)
if newf = 1 | curarea < 1 then a = findfreearea()
else a = curarea
if a = 0 then return rterr('DBFNTX', 1001, 'No free work area', f)
if wa_used[a] = 1 then call closearea a
al = ''
if n >= 4 then do
  if isstr($4) = 1 then al = upper(strip(cstr[$4]))
end
if al == '' then al = dbfstem(f)
ro = 0
if n >= 6 then do
  if $6 = 2 then ro = 1
end
if openarea(a, f, al, ro) = 0 then return 0
curarea = a
return 0
#PRIM DBCLOSEAREA 0 0
if curarea > 0 then call closearea curarea
return 0
#PRIM DBCLOSEALL 0 0
call closeallareas
curarea = 0
return 0
#PRIM DBCREATE 2 5
if isarr($2) = 0 then return argerr('DBCREATE')
junk = createdbf(sarg($1), $2)
return 0
#PRIM DBSTRUCT 0 0
a = curarea
if a < 1 then return mkarray(0)
r = mkarray(0)
do i = 1 to wa_nfld[a]
  e = mkarray(0)
  call arrpush e, mkstr(fnam[(a - 1) * 256 + i])
  call arrpush e, mkstr(ftyp[(a - 1) * 256 + i])
  call arrpush e, mkint(flen[(a - 1) * 256 + i])
  call arrpush e, mkint(fdec[(a - 1) * 256 + i])
  call arrpush r, e
end
return r
#PRIM DBAPPEND 0 1
if curarea < 1 then return noarea()
call dbappend curarea
return 0
#PRIM DBDELETE 0 0
if curarea < 1 then return noarea()
call dbdelete curarea, 1
return 0
#PRIM DBRECALL 0 0
if curarea < 1 then return noarea()
call dbdelete curarea, 0
return 0
#PRIM DBSKIP 0 1
if curarea < 1 then return noarea()
call dbskip curarea, iarg($1, 1)
return 0
#PRIM DBGOTO 1 1
if curarea < 1 then return noarea()
call dbgoto curarea, iarg($1, 0)
return 0
#PRIM DBGOTOP 0 0
if curarea < 1 then return noarea()
call dbgotop curarea
return 0
#PRIM DBGOBOTTOM 0 0
if curarea < 1 then return noarea()
call dbgobottom curarea
return 0
#PRIM DBSEEK 1 3
if curarea < 1 then return noarea()
soft = 0
if n >= 2 then do
  if $2 = 2 then soft = 1
end
return mklog(dbseek(curarea, $1, soft))
#PRIM DBPACK 0 0
if curarea < 1 then return noarea()
call dbpack curarea
return 0
#PRIM DBZAP 0 0
if curarea < 1 then return noarea()
call dbzap curarea
return 0
#PRIM DBSETFILTER 1 2
if curarea < 1 then return noarea()
wa_filter[curarea] = 0
if n = 2 then do
  if isstr($2) = 1 then wa_filter[curarea] = compileexpr(cstr[$2])
end
return 0
#PRIM DBCLEARFILTER 0 0
if curarea > 0 then wa_filter[curarea] = 0
return 0
#PRIM DBFILTER 0 0
return mkstr('')
#PRIM DBCREATEINDEX|ORDCREATE 2 5
/* DbCreateIndex( cFile, cKey, [bKey], [lUnique] ) : cKey is compiled from text */
if curarea < 1 then return noarea()
kt = sarg($2)
if n = 5 then kt = sarg($2)
nd2 = compileexpr(kt)
if nd2 = 0 then return rterr('DBFNTX', 1004, 'Index key error', kt)
fl = 0
if n >= 4 then do
  if $4 = 2 then fl = 1
end
junk = createindex(curarea, nd2, sarg($1), 0, fl, kt, '')
return 0
#PRIM DBSETINDEX|ORDLISTADD 1 2
if curarea < 1 then return noarea()
junk = openindex(curarea, sarg($1))
return 0
#PRIM DBREINDEX|ORDLISTREBUILD 0 0
if curarea < 1 then return noarea()
call reindexall curarea
return 0
#PRIM ORDLISTCLEAR|DBCLEARINDEX 0 0
if curarea > 0 then do
  wa_nix[curarea] = 0
  wa_order[curarea] = 0
end
return 0
#PRIM DBSETORDER|ORDSETFOCUS 0 2
if curarea < 1 then return mkint(0)
a = curarea
old = 0
do j = 1 to wa_nix[a]
  if waix[(a - 1) * 8 + j] = wa_order[a] then old = j
end
if n >= 1 then do
  k = -1
  if isnum($1) = 1 then k = iarg($1, 0)
  else if isstr($1) = 1 then do
    u = upper(strip(cstr[$1]))
    k = 0
    do j = 1 to wa_nix[a]
      if ix_name[waix[(a - 1) * 8 + j]] == u then k = j
    end
  end
  if k >= 0 & k <= wa_nix[a] then do
    if k = 0 then wa_order[a] = 0
    else wa_order[a] = waix[(a - 1) * 8 + k]
    call dbgotop a
  end
end
return mkint(old)
#PRIM INDEXORD 0 0
if curarea < 1 then return mkint(0)
a = curarea
do j = 1 to wa_nix[a]
  if waix[(a - 1) * 8 + j] = wa_order[a] then return mkint(j)
end
return mkint(0)
#PRIM INDEXKEY|ORDKEY 0 1
if curarea < 1 then return mkstr('')
a = curarea
if wa_order[a] = 0 then return mkstr('')
return mkstr(ix_text[wa_order[a]])
#PRIM ORDNAME 0 2
if curarea < 1 then return mkstr('')
a = curarea
k = iarg($1, 0)
if n = 0 then do
  if wa_order[a] = 0 then return mkstr('')
  return mkstr(ix_name[wa_order[a]])
end
if k < 1 | k > wa_nix[a] then return mkstr('')
return mkstr(ix_name[waix[(a - 1) * 8 + k]])
#PRIM ORDCOUNT 0 2
if curarea < 1 then return mkint(0)
return mkint(wa_nix[curarea])
#PRIM DBEVAL 1 6
/* DbEval( bBlock, [bFor], [bWhile], [nNext], [nRecord], [lRest] ) */
if curarea < 1 then return noarea()
a = curarea
nxt = -1
if n >= 4 then do
  if isnum($4) = 1 then nxt = iarg($4, -1)
end
if n >= 5 then do
  if isnum($5) = 1 then do
    call dbgoto a, iarg($5, 0)
    nxt = 1
  end
end
rest = 0
if n >= 6 then do
  if $6 = 2 then rest = 1
end
if nxt = -1 & rest = 0 & n < 5 then call dbgotop a
do while wa_eof[a] = 0 & nxt <> 0
  if n >= 3 then do
    if $3 > 2 then do
      if evalblock0($3) <> 2 then leave
    end
  end
  ok = 1
  if n >= 2 then do
    if $2 > 2 then do
      if evalblock0($2) <> 2 then ok = 0
    end
  end
  if ok = 1 then junk = evalblock0($1)
  if ctl <> 0 then return 0
  if nxt > 0 then nxt = nxt - 1
  if n >= 5 then leave
  call dbskip a, 1
end
return 0
#PRIM HB_DBPACK 0 0
if curarea > 0 then call dbpack curarea
return 0
#PRIM LUPDATE 0 0
return mkdate(ymd2jdn(substr(date('S'), 1, 4) as .int, substr(date('S'), 5, 2) as .int, substr(date('S'), 7, 2) as .int))
#PRIM HEADER 0 0
if curarea < 1 then return mkint(0)
return mkint(wa_hdr[curarea])
#PRIM RECSIZE 0 0
if curarea < 1 then return mkint(0)
return mkint(wa_reclen[curarea])
#PRIM DBINFO|DBORDERINFO|DBRECORDINFO|DBFIELDINFO 0 -1
return 0
#PRIM DBRELATION|DBRSELECT 0 1
return mkstr('')
#PRIM DBSETRELATION 2 3
if curarea < 1 then return noarea()
a = curarea
k = areaarg($1)
if k < 1 then return 0
if isstr($2) = 0 then return 0
nd2 = compileexpr(cstr[$2])
wa_nrel[a] = wa_nrel[a] + 1
wa_relchild[(a - 1) * 8 + wa_nrel[a]] = k
wa_relkey[(a - 1) * 8 + wa_nrel[a]] = nd2
call relsync a
return 0
#PRIM DBCLEARRELATION 0 0
if curarea > 0 then wa_nrel[curarea] = 0
return 0
