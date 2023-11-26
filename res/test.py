import sys

arg = sys.argv[1]
arg2 = sys.argv[2]
file = open(arg, "r")

str = file.read()

str = str.replace("\"", "")
str = str.replace("c9 ", "")
str = str.replace(";", "")

open(arg2, "w").write(str)
