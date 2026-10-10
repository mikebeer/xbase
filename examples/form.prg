// form.prg - @ SAY / GET and READ
PROCEDURE Main()
   LOCAL cName := Space( 10 ), nAge := 0, dDate := CToD( "" )
   @ 1, 2 SAY "Name:" GET cName PICTURE "@!"
   @ 2, 2 SAY "Age :" GET nAge VALID nAge > 0 .AND. nAge < 120
   @ 3, 2 SAY "Date:" GET dDate
   READ
   ?
   ? "Got:", Trim( cName ), nAge, dDate
RETURN
