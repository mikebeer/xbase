// DBF tables, indexes, seek, scoped commands
PROCEDURE Main()
   LOCAL aS := { {"NAME","C",12,0}, {"AGE","N",3,0}, {"SALARY","N",9,2}, {"HIRED","D",8,0}, {"ACTIVE","L",1,0} }
   LOCAL i, n
   dbCreate( "emp", aS )
   USE emp
   FOR i := 1 TO 6
      APPEND BLANK
      REPLACE NAME WITH { "Zed","Anna","Mike","Carl","Beth","Dora" }[i]
      REPLACE AGE WITH 20 + i * 5, SALARY WITH 1000.5 * i, HIRED WITH CToD("01/0"+LTrim(Str(i))+"/2020"), ACTIVE WITH (i % 2 == 0)
   NEXT
   ? "Records:", RecCount(), "Fields:", FCount()
   GO 3
   ? Name, Age, Salary, Hired, Active
   INDEX ON Upper(Name) TO empname
   GO TOP
   DO WHILE !Eof()
      ? RecNo(), Name, Age
      SKIP
   ENDDO
   SEEK "MIKE"
   ? "Found:", Found(), Name
   SEEK "NOBODY"
   ? "Found:", Found(), Eof()
   COUNT TO n FOR Age > 30
   ? "Count>30:", n
   SUM Salary, Age TO s1, s2
   ? s1, s2
   AVERAGE Age TO av
   ? av
   DELETE FOR Name = "Zed"
   GO BOTTOM
   ? "Last:", Name, Deleted()
   SET DELETED ON
   GO TOP
   SKIP -1
   ? "Bof:", Bof(), Name
   GO BOTTOM
   ? Name
   SET DELETED OFF
   LIST Name, Age FOR Age > 40
   LOCATE FOR Age = 35
   ? Found(), Name
   CONTINUE
   ? Found()
   PACK
   ? RecCount()
   REPLACE ALL Age WITH Age + 1
   GO TOP
   ? Name, Age
   SET ORDER TO 0
   GO TOP
   ? Name, RecNo()
   SET FILTER TO Age > 40
   GO TOP
   DO WHILE !Eof()
      QQOut(Trim(Name), " ")
      SKIP
   ENDDO
   ? 
   SET FILTER TO
   CLOSE ALL
   USE emp INDEX empname
   ? RecCount(), Name, IndexKey()
   DISPLAY STRUCTURE
   FIELD->AGE := 99
   ? emp->Age
   USE
   ? Select(), Used()
   USE emp NEW ALIAS staff
   ? Alias(), staff->Name, FieldName(2), FieldPos("SALARY")
   ERASE emp.dbf
   ERASE empname.xdx
   ? File("emp.dbf")
RETURN
