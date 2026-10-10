// primes.prg - sieve of Eratosthenes with an array and a codeblock
PROCEDURE Main( cLimit )
   LOCAL nMax := Val( iif( cLimit == NIL, "100", cLimit ) )
   LOCAL aSieve := Array( nMax ), i, j, aPrimes := {}
   AFill( aSieve, .T. )
   FOR i := 2 TO nMax
      IF aSieve[ i ]
         AAdd( aPrimes, i )
         FOR j := i * i TO nMax STEP i
            aSieve[ j ] := .F.
         NEXT
      ENDIF
   NEXT
   ? "Primes up to", nMax, ":", Len( aPrimes )
   ?
   AEval( aPrimes, {| n | QQOut( LTrim( Str( n ) ) + " " ) } )
   ?
RETURN
