// errors.prg - BEGIN SEQUENCE, TRY/CATCH, ErrorBlock and Throw
PROCEDURE Main()
   LOCAL e, oErr

   BEGIN SEQUENCE
      ? 1 / 0
      ? "not reached"
   RECOVER USING e
      ? "recovered:", e:description
   END SEQUENCE

   ErrorBlock( {| x | Break( x ) } )
   BEGIN SEQUENCE
      NoSuchFunction()
   RECOVER USING e
      ? "caught:", e:description, e:operation
   END SEQUENCE

   TRY
      oErr := ErrorNew()
      oErr:description := "made by hand"
      Throw( oErr )
   CATCH e
      ? "catch:", e:description
   END
RETURN
