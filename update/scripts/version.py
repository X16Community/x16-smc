f = open("../version.h")

version = {}
version["version_major"] = 0
version["version_minor"] = 0
version["version_patch"] = 0

line = f.readline()
while line:
    parts = line.split()
    if parts[0].strip().lower() == "#define":
        version[parts[1].strip().lower()] = int(parts[2].strip())
    line = f.readline()

# Create version.asm
asm = open("version.asm", "w")
asm.write("version_major = " + str(version["version_major"]) + "\n")
asm.write("version_minor = " + str(version["version_minor"]) + "\n")
asm.write("version_patch = " + str(version["version_patch"]) + "\n")
asm.close()
