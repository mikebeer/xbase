// control flow, operators, strings, numbers
PROCEDURE Main()
   LOCAL i, n := 0, s := "", x
   FOR i := 1 TO 10 STEP 3
      n += i
   NEXT
   ? "for:", n
   i := 0
   DO WHILE i < 5
      i++
      IF i == 2
         LOOP
      ELSEIF i == 4
         EXIT
      ENDIF
      s += Str(i, 1)
   ENDDO
   ? "while:", s
   DO CASE
   CASE n > 100 ; ? "big"
   CASE n > 10  ; ? "medium"
   OTHERWISE    ; ? "small"
   ENDCASE
   SWITCH 3
   CASE 1 ; ? "one"
   CASE 3 ; ? "three"
      EXIT
   OTHERWISE ; ? "other"
   ENDSWITCH
   ? 7 / 2, 7 % 3, 2 ^ 10, -5 + 3 * 2, Int(7 / 2), Round(2.675, 2), Abs(-3.5)
   ? "abc" + "def", "abc" - "  d", "a" $ "banana", "abc" < "abd", "AB" == "AB", "AB" = "ABC"
   ? Upper("hello"), Lower("WORLD"), Len("four"), Left("abcdef", 2), Right("abcdef", 2), SubStr("abcdef", 3, 2)
   ? At("lo", "hello"), RAt("l", "hello"), Replicate("ab", 3), Space(3) + "|", PadL("7", 4, "0"), PadR("x", 3, ".") + "|", PadC("m", 5, "*")
   ? Str(3.14159, 8, 3), Val("12.5abc"), Str(-7), Transform(1234567.891, "@E 9,999,999.99"), Transform("ab", "@!")
   ? Empty(""), Empty(0), Empty(NIL), Empty(" x "), ValType(1), ValType("a"), ValType(.T.), ValType(NIL), ValType({}), ValType({|| 1 })
   ? StrTran("a-b-c", "-", "+"), AllTrim("  pad  ") + "|", LTrim("  x"), RTrim("y  ") + "|", Chr(65), Asc("a"), Stuff("abcdef", 2, 2, "XY")
   ? iif(n > 5, "yes", "no"), Max(3, 9), Min(3, 9), Mod(-7, 3), Sqrt(144), Exp(0), Log(1)
   x := 5
   x *= 3 ; x -= 1 ; x /= 2
   ? x, x++ + ++x, x
   ? .T. .AND. .F., .T. .OR. .F., !.T., .NOT. .F.
   ? 1e3, 0.1 + 0.2, 100000000 * 100000000, 10 / 4
   ? Replicate("=", 20)
   ? Year(Date()) > 2000, DToS(CToD("12/25/2023")), CDoW(CToD("12/25/2023")), CMonth(CToD("12/25/2023")), Day(CToD("12/25/2023")) + 1
   ? CToD("12/25/2023") + 10, CToD("12/25/2023") - CToD("12/01/2023"), DToC(CToD("01/02/2003"))
