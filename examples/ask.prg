// ask.prg - ACCEPT and INPUT read one line each from standard input
PROCEDURE Main()
   LOCAL nAge
   ACCEPT "Name? " TO cName
   INPUT "Age? " TO nAge
   ? "Hello,", Trim( cName ) + ". Next year you will be", nAge + 1
RETURN
