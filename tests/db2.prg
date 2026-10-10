// memo fields and relations
PROCEDURE Main()
   dbCreate("memo", { {"TITLE","C",8,0}, {"NOTE","M",10,0} })
   USE memo
   APPEND BLANK
   REPLACE TITLE WITH "one", NOTE WITH "First memo text" + Chr(13) + Chr(10) + "line two"
   APPEND BLANK
   REPLACE TITLE WITH "two", NOTE WITH "Second"
   REPLACE NOTE WITH "Second, rewritten and longer"
   USE
   USE memo
   GO 1
   ? Title, Len(Note), Note
   GO 2
   ? Title, Note
   USE
   // relations
   dbCreate("cust", { {"ID","N",3,0}, {"NAME","C",10,0} })
   dbCreate("ord", { {"CID","N",3,0}, {"ITEM","C",10,0} })
   USE cust NEW
   APPEND BLANK ; REPLACE ID WITH 1, NAME WITH "Ann"
   APPEND BLANK ; REPLACE ID WITH 2, NAME WITH "Bob"
   INDEX ON ID TO cust_id
   USE ord NEW
   APPEND BLANK ; REPLACE CID WITH 2, ITEM WITH "pen"
   APPEND BLANK ; REPLACE CID WITH 1, ITEM WITH "ink"
   SELECT ord
   SET RELATION TO CID INTO cust
   GO TOP
   DO WHILE !Eof()
      ? ITEM, cust->NAME
      SKIP
   ENDDO
   CLOSE ALL
RETURN
