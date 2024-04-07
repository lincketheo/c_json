# https://datatracker.ietf.org/doc/html/rfc8259

JSON_TEXT -> *ws value *ws

ws -> Space | Horizontal Tab | Line feed | New Line | Carriage Return

value -> false | null | true | object | array | number | string

object -> begin-object member *(value-separator member) end-object

member -> string name-separator value

array -> begin-array value *(value-separator value) end-array

number -> [ minus ] int [ frac ] [ exp ]

minus -> -

int -> zero | ( digit1-9 *DIGIT )

digit1-9 -> 1-9

zero -> 0

frac -> decimal-point 1*DIGIT

decimal-point -> .

exp -> e [ minus | plus ] 1*DIGIT

e -> e | E

plus -> +

string -> " *char "

char = unescaped | escape ( " | \ | / | b | f | n | r | t | hexdig )

hexdig -> uXXXX

escape -> \

unescaped -> %x20-21 / %x23-5B / %x5D-10FFFF
