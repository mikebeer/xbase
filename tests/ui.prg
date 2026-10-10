// console commands: ACCEPT, INPUT, @ SAY/GET, READ, WAIT
PROCEDURE Main()
   LOCAL cName := Space(10), nAge := 0, dDate := CToD("")
   MEMVAR cAns
   ACCEPT "Your name? " TO cAns
   ? "Hello,", Trim(cAns)
   INPUT "Number: " TO nVal
   ? "Double:", nVal * 2
   @ 5, 2 SAY "Name:" GET cName PICTURE "@!"
   @ 6, 2 SAY "Age :" GET nAge VALID nAge > 0 .AND. nAge < 120
   @ 7, 2 SAY "Date:" GET dDate
   READ
   ? "Got:", Trim(cName), nAge, dDate
   WAIT "Press a key"
   STORE 10 TO a, b
   ? a + b
RETURN
