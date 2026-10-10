// bank.prg - classes, inheritance and error handling
#include "hbclass.ch"

CREATE CLASS Account
   VAR cOwner
   VAR nBalance INIT 0
   METHOD New( cOwner, nOpen ) CONSTRUCTOR
   METHOD Deposit( n )
   METHOD Withdraw( n )
   METHOD Report()
ENDCLASS

METHOD New( cOwner, nOpen ) CLASS Account
   ::cOwner := cOwner
   IF nOpen != NIL
      ::nBalance := nOpen
   ENDIF
RETURN Self

METHOD Deposit( n ) CLASS Account
   ::nBalance += n
RETURN Self

METHOD Withdraw( n ) CLASS Account
   LOCAL oErr
   IF n > ::nBalance
      oErr := ErrorNew()
      oErr:description := "Insufficient funds"
      oErr:operation := "Withdraw"
      Throw( oErr )
   ENDIF
   ::nBalance -= n
RETURN Self

METHOD Report() CLASS Account
RETURN ::cOwner + ": " + LTrim( Str( ::nBalance, 10, 2 ) )

CREATE CLASS Savings INHERIT Account
   VAR nRate INIT 0.05
   METHOD AddInterest()
ENDCLASS

METHOD AddInterest() CLASS Savings
   ::nBalance += ::nBalance * ::nRate
RETURN Self

PROCEDURE Main()
   LOCAL a := Account():New( "Ann", 100 ), s := Savings():New( "Sam", 1000 ), e
   a:Deposit( 50 ):Withdraw( 30 )
   ? a:Report()
   s:AddInterest()
   ? s:Report()
   TRY
      a:Withdraw( 1000 )
   CATCH e
      ? "Error:", e:description
   END
RETURN
