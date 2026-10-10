// guess.prg - ACCEPT / INPUT / WAIT on the console (feed it with: printf '50\n25\n' | ./xbase examples/guess.prg)
PROCEDURE Main()
   LOCAL nSecret := 37, nTries := 0, nGuess := 0
   ? "Guess my number (1-100)"
   DO WHILE nGuess != nSecret
      INPUT "Your guess: " TO nGuess
      nTries++
      IF nGuess < nSecret
         ? "higher"
      ELSEIF nGuess > nSecret
         ? "lower"
      ENDIF
      IF Eof_Input()
         EXIT
      ENDIF
   ENDDO
   ? "Done after", nTries, "tries"
RETURN

FUNCTION Eof_Input()
RETURN .F.
