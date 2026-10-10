// classes, inheritance, errors, macros
#include "hbclass.ch"

CREATE CLASS Animal
   VAR cName INIT "?"
   VAR nLegs INIT 4
   METHOD New( cName ) CONSTRUCTOR
   METHOD Speak()
   METHOD Describe()
ENDCLASS

METHOD New( cName ) CLASS Animal
   ::cName := cName
RETURN Self

METHOD Speak() CLASS Animal
RETURN "..."

METHOD Describe() CLASS Animal
RETURN ::cName + " has " + LTrim(Str(::nLegs)) + " legs and says " + ::Speak()

CREATE CLASS Bird INHERIT Animal
   METHOD New( cName ) CONSTRUCTOR
   METHOD Speak()
ENDCLASS

METHOD New( cName ) CLASS Bird
   ::Animal:New( cName )
   ::nLegs := 2
RETURN Self

METHOD Speak() CLASS Bird
RETURN "tweet"

PROCEDURE Main()
   LOCAL o, r, e
   o := Animal():New( "Rex" )
   ? o:Describe()
   o := Bird():New( "Tweety" )
   ? o:Describe(), o:ClassName()
   BEGIN SEQUENCE
      ? 1 / 0
      ? "not reached"
   RECOVER USING e
      ? "caught:", e:description
   END SEQUENCE
   r := &( "2 + 3 * 4" )
   ? r
   PRIVATE cVar := "'macro text'"
   ? &cVar
   ErrorBlock({|x| Break(x) })
   BEGIN SEQUENCE
      Undefined()
   RECOVER USING e
      ? "err:", e:description, e:operation
   END SEQUENCE
   TRY
      e := ErrorNew()
      e:description := "custom"
      Throw( e )
   CATCH e
      ? "try-catch ok:", e:description
   END
RETURN
