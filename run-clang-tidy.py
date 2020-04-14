import os, sys, subprocess
from multiprocessing import Pool

cpp_extensions = (".cpp", ".cxx", ".c++", ".cc", ".cp", ".c", ".i", ".ii", ".h", ".h++", ".hpp", ".hxx", ".hh", ".inl", ".inc", ".ipp", ".ixx", ".txx", ".tpp", ".tcc", ".tpl")
failedFiles = []
checkFiles = []

def runClangTidy(filePath):
    print("Checking " + filePath)
    proc = subprocess.Popen("clang-tidy --quiet -p=" + buildDir + " " + filePath, shell=True)
    returnCode = proc.wait()
    if returnCode != 0:
        failedFiles.append(filePath)

buildDir = os.getcwd() + "/build"
print("Current directory " + os.getcwd())
print("Build directory " + buildDir)
for root, dirs, files in os.walk(os.getcwd()):
    for file in files:
        filePath = root + "/" + file
        if not filePath.startswith(buildDir) and filePath.endswith(cpp_extensions):
            checkFiles.append(filePath)

pool = Pool()
pool.map(runClangTidy, checkFiles)
if len(failedFiles) > 0:
    sys.exit(1)
sys.exit(0)
            