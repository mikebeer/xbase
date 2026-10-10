// arrays, hashes, blocks, closures, sorting
PROCEDURE Main()
   LOCAL a := { 5, 3, 9, 1 }, h := { "x" => 1, "y" => 2 }, b, c, i, m
   AAdd(a, 7)
   ? Len(a), a[1], a[Len(a)]
   ASort(a)
   ? a[1], a[2], a[3], a[4], a[5]
   ASort(a, , , {|p, q| p > q })
   ? a[1], a[5]
   ? AScan(a, 3), AScan(a, {|e| e > 8 })
   ADel(a, 1) ; ASize(a, 4)
   ? Len(a)
   AIns(a, 1) ; a[1] := 100
   ? a[1], a[2]
   m := Array(2, 3)
   m[2, 3] := "z"
   ? Len(m), Len(m[1]), m[2][3]
   AEval({ 1, 2, 3 }, {|e| QQOut(e * e, "") })
   ? 
   ? ATail({ 1, 2, 3 }), Len(AClone({ 1, 2 })), Len(ACopy({ 1, 2, 3 }, Array(3)))
   h["z"] := 3
   ? Len(h), h["y"], HB_HHasKey(h, "z"), HB_HHasKey(h, "q")
   FOR EACH i IN hb_HKeys(h) ; QQOut(i, " ") ; NEXT
   ? 
   b := {|x, y| x + y }
   ? Eval(b, 2, 3)
   c := MakeCounter()
   ? Eval(c), Eval(c), Eval(c)
   FOR EACH i IN { "a", "b" }
      QQOut(i)
   NEXT
   ? 
   ? AScan({ "x", "y" }, "y"), Len({}), Empty({})
   ? ValType(h), ValType(m)
RETURN

FUNCTION MakeCounter()
   LOCAL n := 0
RETURN {|| ++n }
