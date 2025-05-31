import sys
print("""<!DOCTYPE html>
<html>
<head>
<title>Test image page</title>
</head>
<body>""")
for i in range(int(sys.argv[1])):
    print("<img src='./1.jpeg?%d'>" % i)
    print("<img src='./2.jpeg?%d'>" % i)
print("""</body>
</html>""")
