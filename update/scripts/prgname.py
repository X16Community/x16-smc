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

# Return SMCUPDATE file name with version
print("SMCUPDATE-" + str(version["version_major"]) + "." + str(version["version_minor"]) + "." + str(version["version_patch"]) + ".PRG")
