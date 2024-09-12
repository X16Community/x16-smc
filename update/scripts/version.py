f = open("../version.h", "r")

version = {}
version["version_major"] = 0
version["version_minor"] = 0
version["version_patch"] = 0

line = f.readline()
while line:
    parts = line.split()
    if len(parts) >= 3 and parts[0].lower() == "#define":
        version[parts[1].strip().lower()] = int(parts[2].strip())
    line = f.readline()

f.close()

f = open("version.asm", "w")
f.write("version_major = " + str(version["version_major"]) + "\n")
f.write("version_minor = " + str(version["version_minor"]) + "\n")
f.write("version_patch = " + str(version["version_patch"]) + "\n")
f.close()
