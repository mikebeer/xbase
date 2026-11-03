PROCEDURE Main()
   LOCAL cName := Space( 10 ), nAge := 0
   SET GDSM ON
   CLS
   @ 1, 1 SAY "Name:" GET cName PICTURE "@!"
   @ 2, 1 SAY "Age :" GET nAge VALID nAge > 0
   READ
   SET GDSM OFF
   ? "Name", cName, "Age", nAge, "key", LastKey(), Updated()
RETURN
