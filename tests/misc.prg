FUNCTION Main()
   LOCAL e, a := {1, 2, 3}, h := {"a" => 1, "b" => 2}, b, i, x
   AAdd(a, 4)
   ? Len(a), a[2], ATail(a)
   FOR EACH x IN a
      ?? x, ""
   NEXT
   ?
   ? h["a"], Len(h), hb_HHasKey(h, "b")
   b := {|p, q| p * q + 1}
   ? Eval(b, 3, 4)
   x := 10
   b := {|p| p + x}
   x := 20
   ? Eval(b, 1)
   ? Fact(10), Fib(15)
   BEGIN SEQUENCE
      ? 1 / 0
   RECOVER USING e
      ? "caught:", e:description, e:operation
   END SEQUENCE
   ? PadL("ab", 5, "*") + "|" + PadR("cd", 5) + "|" + PadC("x", 5) + "|"
   ? Substr("Hello World", 7), At("o", "foo"), StrTran("a-b-c", "-", "+")
   ? Round(3.14159, 2), Int(7.9), Abs(-3), Max(3, 8), Min(2, 1), Mod(-7, 3), -7 % 3
   ? Date() > CToD("01/01/2020"), Year(CToD("12/25/2020")), DToC(CToD("12/25/2020")), DToS(CToD("12/25/2020"))
   ? Val("12.5abc") + 1, Str(Val("0012")), Transform(1234.5, "9,999.99")
   a := ASort({5, 3, 9, 1})
   ?? a[1], a[2], a[3], a[4]
   ?
   a := ASort({"pear", "apple", "fig"},,, {|p, q| p > q})
   ? a[1], a[2], a[3]
   DO CASE
   CASE x > 100
      ? "big"
   CASE x > 5
      ? "medium"
   OTHERWISE
      ? "small"
   ENDCASE
   SWITCH x
   CASE 20
      ? "twenty"
      EXIT
   CASE 30
      ? "thirty"
   ENDSWITCH
RETURN NIL

FUNCTION Fact(n)
RETURN IIF(n <= 1, 1, n * Fact(n - 1))

FUNCTION Fib(n)
   IF n < 2
      RETURN n
   ENDIF
RETURN Fib(n - 1) + Fib(n - 2)
