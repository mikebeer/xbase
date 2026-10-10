// addrbook.prg - a small DBF application: create, fill, index, search and list
PROCEDURE Main()
   LOCAL aPeople := { ;
      { "Smith",  "John",  "London",  41 }, ;
      { "Jones",  "Mary",  "Cardiff", 36 }, ;
      { "Brown",  "Alice", "Leeds",   29 }, ;
      { "Taylor", "Bob",   "London",  52 }, ;
      { "Davies", "Eve",   "Swansea", 33 } }
   LOCAL aRec

   dbCreate( "people", { { "LAST", "C", 12, 0 }, { "FIRST", "C", 10, 0 }, ;
                         { "CITY", "C", 10, 0 }, { "AGE", "N", 3, 0 } } )
   USE people
   FOR EACH aRec IN aPeople
      APPEND BLANK
      REPLACE LAST WITH aRec[ 1 ], FIRST WITH aRec[ 2 ], CITY WITH aRec[ 3 ], AGE WITH aRec[ 4 ]
   NEXT

   INDEX ON Upper( LAST ) TO people_last
   ? "Sorted by last name:"
   GO TOP
   DO WHILE ! Eof()
      ? RecNo(), Trim( LAST ) + ",", FIRST, CITY, AGE
      SKIP
   ENDDO

   ?
   ? "Looking for JONES:"
   SEEK "JONES"
   IF Found()
      ? "  found:", Trim( FIRST ), Trim( LAST ), "of", Trim( CITY )
   ENDIF

   ?
   ? "Londoners:"
   LIST LAST, AGE FOR CITY = "London"
   COUNT TO nOld FOR AGE > 35
   AVERAGE AGE TO nAvg
   ? "Over 35:", nOld, " average age:", nAvg

   USE
   ERASE people.dbf
   ERASE people_last.xdx
RETURN
